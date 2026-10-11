/**
 * @file remote_protocol.h
 * @brief Protocolo editor <-> motor en OTRO proceso. Lo comparten el backend remoto y el motor.
 *
 * Es lo que hablaría un adaptador de Unreal (plugin Remote Control) u O3DE (una Gem): el editor no
 * enlaza el motor ni comparte su proceso, solo intercambia mensajes. `tools/mock_engine` es un
 * motor simulado que lo implementa entero para poder probar el editor sin tener ninguno instalado.
 *
 * TRANSPORTE: socket Unix de flujo. El MOTOR escucha y el editor se conecta (como Remote Control,
 * donde el servidor es el editor del motor).
 *
 * MARCO: [u32 little-endian: bytes del JSON][JSON][binario opcional]
 *   El JSON puede llevar "binary": N, y entonces le siguen N bytes crudos (los píxeles de un frame).
 *   Así una imagen de 2 MB no pasa por base64 ni por el parser de JSON.
 *
 * MENSAJES (editor -> motor; todos con "id", la respuesta lleva "re": <id>):
 *   hello {proto}                      -> hello_ok {engine, version, proto}
 *   get_scene                          -> scene (ver abajo)
 *   set_property {node, name, value}   -> ok          (+ empuja `scene` con la revisión nueva)
 *   select {node}                      -> ok          (+ empuja `scene`)
 *   render {w, h}                      -> frame {w, h, format:"rgba8", binary}
 *   input {kind:"drag", dx, dy} | {kind:"wheel", delta} | {kind:"click", x, y, w, h}
 *                                      -> ok          (un click puede cambiar la selección)
 *   play {on}                          -> ok          (+ empuja `state`)
 *   open_project {root}                -> ok {ok, error?}
 *   new_project {root, name}           -> ok {ok, document}
 *   save {path} | load {path}          -> ok {ok, error?}
 *   quit                               -> (cierra)
 * EMPUJES (motor -> editor, sin "re"):
 *   scene {revision, selected, nodes:[{id, name, type, parent, props:[{name, type, value, readOnly}]}]}
 *   state {playing}
 * Tipos de propiedad: "float", "int", "bool", "string", "vec3", "color" (vec3/color = [x,y,z]).
 */
#pragma once

#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <cerrno>
#include <cstdint>
#include <string>
#include <vector>

namespace remote {

constexpr int kProtocolVersion = 1;
/// Tope de un mensaje: un JSON roto o un motor que se vuelve loco no debe hacer reservar GB.
constexpr uint32_t kMaxJsonBytes = 16u << 20;
constexpr uint64_t kMaxBinaryBytes = 256ull << 20;

struct Message {
    nlohmann::json json;
    std::vector<uint8_t> binary;
};

/// Escribe TODO o falla (send puede escribir menos de lo pedido). MSG_NOSIGNAL: un motor que se
/// cae no debe matar al editor con SIGPIPE.
inline bool writeAll(int fd, const void* data, size_t n) {
    const auto* p = static_cast<const uint8_t*>(data);
    while (n > 0) {
        const ssize_t w = ::send(fd, p, n, MSG_NOSIGNAL);
        if (w < 0) { if (errno == EINTR) continue; return false; }
        p += w; n -= (size_t)w;
    }
    return true;
}

/// Lee EXACTAMENTE n bytes o falla (recv devuelve trozos; 0 = el otro lado cerró).
inline bool readAll(int fd, void* data, size_t n) {
    auto* p = static_cast<uint8_t*>(data);
    while (n > 0) {
        const ssize_t r = ::recv(fd, p, n, 0);
        if (r == 0) return false;
        if (r < 0) { if (errno == EINTR) continue; return false; }
        p += r; n -= (size_t)r;
    }
    return true;
}

inline bool send(int fd, nlohmann::json j, const std::vector<uint8_t>* binary = nullptr) {
    if (binary) j["binary"] = binary->size();
    const std::string body = j.dump();
    if (body.size() > kMaxJsonBytes) return false;
    const uint32_t len = (uint32_t)body.size();
    const uint8_t hdr[4] = {uint8_t(len), uint8_t(len >> 8), uint8_t(len >> 16), uint8_t(len >> 24)};
    if (!writeAll(fd, hdr, 4) || !writeAll(fd, body.data(), body.size())) return false;
    if (binary && !binary->empty() && !writeAll(fd, binary->data(), binary->size())) return false;
    return true;
}

/// false = conexión cerrada o mensaje inválido (en ambos casos el canal ya no sirve).
inline bool receive(int fd, Message& out, std::string* error = nullptr) {
    uint8_t hdr[4];
    if (!readAll(fd, hdr, 4)) { if (error) *error = "conexion cerrada"; return false; }
    const uint32_t len = uint32_t(hdr[0]) | uint32_t(hdr[1]) << 8 | uint32_t(hdr[2]) << 16 | uint32_t(hdr[3]) << 24;
    if (len == 0 || len > kMaxJsonBytes) { if (error) *error = "longitud de mensaje invalida: " + std::to_string(len); return false; }
    std::string body(len, '\0');
    if (!readAll(fd, body.data(), len)) { if (error) *error = "conexion cerrada a mitad de mensaje"; return false; }
    out.json = nlohmann::json::parse(body, nullptr, false);
    if (out.json.is_discarded() || !out.json.is_object()) { if (error) *error = "JSON invalido"; return false; }
    out.binary.clear();
    if (auto it = out.json.find("binary"); it != out.json.end()) {
        if (!it->is_number_unsigned() || it->get<uint64_t>() > kMaxBinaryBytes) {
            if (error) *error = "binario invalido";
            return false;
        }
        out.binary.resize(it->get<size_t>());
        if (!out.binary.empty() && !readAll(fd, out.binary.data(), out.binary.size())) {
            if (error) *error = "conexion cerrada a mitad del binario";
            return false;
        }
    }
    return true;
}

} // namespace remote
