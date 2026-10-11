// Motor SIMULADO para probar el backend remoto del editor sin Unreal ni O3DE instalados.
//
// Es un proceso aparte que habla `remote_protocol.h` igual que lo haría el adaptador de un motor
// real: escucha en un socket Unix, tiene una escena (nodos con propiedades), dibuja por software lo
// que ve una cámara que se mueve con el ratón, y guarda/carga documentos JSON. No enlaza el editor
// ni ImGui ni ningún motor.
//
//   haruka_mock_engine --socket /ruta/al.sock
//
// Cierra cuando el editor se desconecta o manda `quit`.
#include "remote_protocol.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

struct Node {
    std::string id, name, type, parent;
    json props = json::array();   // [{name, type, value, readOnly}]
};

struct World {
    std::vector<Node> nodes;
    std::string selected;
    uint64_t revision = 1;
    bool playing = false;
    double playStart = 0.0;
    // Cámara orbital alrededor del origen
    double yaw = 0.6, pitch = 0.35, dist = 12.0;
};

json prop(const char* name, const char* type, json value, bool ro = false) {
    return {{"name", name}, {"type", type}, {"value", std::move(value)}, {"readOnly", ro}};
}

World defaultWorld() {
    World w;
    w.nodes.push_back({"sun", "Sun", "Light", "", json::array({
        prop("position", "vec3", {4.0, 8.0, 3.0}),
        prop("color", "color", {1.0, 0.95, 0.8}),
        prop("intensity", "float", 2.0)})});
    w.nodes.push_back({"cube", "Cube", "Mesh", "", json::array({
        prop("position", "vec3", {0.0, 1.0, 0.0}),
        prop("color", "color", {0.85, 0.3, 0.25}),
        prop("size", "float", 1.0),
        prop("spin", "bool", true),
        prop("speed", "float", 1.0)})});
    w.nodes.push_back({"moon", "Moon", "Mesh", "cube", json::array({
        prop("position", "vec3", {3.0, 1.5, 0.0}),
        prop("color", "color", {0.3, 0.55, 0.9}),
        prop("size", "float", 0.5),
        prop("spin", "bool", true),     // gira alrededor del eje Y: la luna, que está fuera del origen, ORBITA
        prop("speed", "float", 2.0)})});
    w.nodes.push_back({"tag", "Label", "Note", "", json::array({
        prop("text", "string", "motor simulado"),
        prop("id", "string", "tag", true),
        prop("priority", "int", 3)})});
    return w;
}

json* findProp(Node& n, const std::string& name) {
    for (auto& p : n.props) if (p["name"] == name) return &p;
    return nullptr;
}
Node* findNode(World& w, const std::string& id) {
    for (auto& n : w.nodes) if (n.id == id) return &n;
    return nullptr;
}
double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

json sceneMsg(const World& w) {
    json nodes = json::array();
    for (const auto& n : w.nodes)
        nodes.push_back({{"id", n.id}, {"name", n.name}, {"type", n.type}, {"parent", n.parent}, {"props", n.props}});
    return {{"type", "scene"}, {"revision", w.revision}, {"selected", w.selected}, {"nodes", nodes}};
}

// ── Cámara y dibujo por software ────────────────────────────────────────────────────────────
struct V3 { double x, y, z; };
V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 norm(V3 a) { double l = std::sqrt(dot(a, a)); return {a.x / l, a.y / l, a.z / l}; }

struct Cam { V3 eye, fwd, right, up; double f; };
Cam camera(const World& w, int width, int height) {
    Cam c;
    c.eye = {w.dist * std::cos(w.pitch) * std::sin(w.yaw), w.dist * std::sin(w.pitch), w.dist * std::cos(w.pitch) * std::cos(w.yaw)};
    c.fwd = norm(sub({0, 0.5, 0}, c.eye));
    c.right = norm(cross(c.fwd, {0, 1, 0}));
    c.up = cross(c.right, c.fwd);
    c.f = 0.5 * height / std::tan(0.5 * 60.0 * M_PI / 180.0);
    (void)width;
    return c;
}
bool project(const Cam& c, V3 p, int w, int h, double& sx, double& sy, double& depth) {
    const V3 d = sub(p, c.eye);
    depth = dot(d, c.fwd);
    if (depth < 0.1) return false;
    sx = w * 0.5 + dot(d, c.right) * c.f / depth;
    sy = h * 0.5 - dot(d, c.up) * c.f / depth;
    return true;
}

/// Posición en pantalla de un nodo (con la animación del play mode aplicada).
V3 nodePosition(const World& w, const Node& n) {
    const json& pv = n.props;
    V3 p{0, 0, 0};
    bool spin = false; double speed = 0;
    for (const auto& q : pv) {
        if (q["name"] == "position") p = {q["value"][0], q["value"][1], q["value"][2]};
        if (q["name"] == "spin") spin = q["value"];
        if (q["name"] == "speed") speed = q["value"];
    }
    if (w.playing && spin) {
        const double a = (now() - w.playStart) * speed;
        p = {p.x * std::cos(a) - p.z * std::sin(a), p.y, p.x * std::sin(a) + p.z * std::cos(a)};
    }
    return p;
}

struct Drawn { std::string id; double sx, sy, half, depth; uint8_t r, g, b; };

std::vector<Drawn> layout(const World& w, const Cam& c, int width, int height) {
    std::vector<Drawn> out;
    for (const auto& n : w.nodes) {
        double size = 0.0; double col[3] = {1, 1, 1};
        for (const auto& q : n.props) {
            if (q["name"] == "size") size = q["value"];
            if (q["name"] == "color") for (int i = 0; i < 3; ++i) col[i] = q["value"][i];
        }
        if (n.type == "Light") size = 0.35;
        if (size <= 0) continue;
        double sx, sy, d;
        if (!project(c, nodePosition(w, n), width, height, sx, sy, d)) continue;
        out.push_back({n.id, sx, sy, size * c.f / d, d,
                       uint8_t(std::clamp(col[0], 0.0, 1.0) * 255), uint8_t(std::clamp(col[1], 0.0, 1.0) * 255),
                       uint8_t(std::clamp(col[2], 0.0, 1.0) * 255)});
    }
    std::sort(out.begin(), out.end(), [](const Drawn& a, const Drawn& b) { return a.depth > b.depth; });
    return out;
}

std::vector<uint8_t> render(const World& w, int width, int height) {
    std::vector<uint8_t> px((size_t)width * height * 4);
    const Cam c = camera(w, width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t* o = &px[((size_t)y * width + x) * 4];
            // Rayo de cámara contra el suelo y=0: a cuadros donde lo toca, cielo donde no.
            const double u = (x - width * 0.5) / c.f, v = (height * 0.5 - y) / c.f;
            const V3 dir = norm({c.fwd.x + u * c.right.x + v * c.up.x, c.fwd.y + u * c.right.y + v * c.up.y,
                                 c.fwd.z + u * c.right.z + v * c.up.z});
            if (dir.y < -1e-4) {
                const double t = -c.eye.y / dir.y;
                const double gx = c.eye.x + dir.x * t, gz = c.eye.z + dir.z * t;
                const bool chk = ((int)std::floor(gx) + (int)std::floor(gz)) & 1;
                const double fog = std::min(1.0, t / 60.0);
                const double base = chk ? 0.42 : 0.32;
                const double g = base * (1 - fog) + 0.7 * fog;
                o[0] = uint8_t(g * 200); o[1] = uint8_t(g * 230); o[2] = uint8_t(g * 200);
            } else {
                const double s = std::clamp(dir.y, 0.0, 1.0);
                o[0] = uint8_t(150 - 90 * s); o[1] = uint8_t(190 - 70 * s); o[2] = uint8_t(235 - 20 * s);
            }
            o[3] = 255;
        }
    }
    for (const auto& d : layout(w, c, width, height)) {
        const int x0 = std::max(0, (int)(d.sx - d.half)), x1 = std::min(width - 1, (int)(d.sx + d.half));
        const int y0 = std::max(0, (int)(d.sy - d.half)), y1 = std::min(height - 1, (int)(d.sy + d.half));
        const bool sel = d.id == w.selected;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                uint8_t* o = &px[((size_t)y * width + x) * 4];
                const bool edge = x - x0 < 2 || x1 - x < 2 || y - y0 < 2 || y1 - y < 2;
                if (sel && edge) { o[0] = o[1] = o[2] = 255; }
                else { o[0] = d.r; o[1] = d.g; o[2] = d.b; }
            }
    }
    return px;
}

// ── Documentos ──────────────────────────────────────────────────────────────────────────────
bool saveDoc(const World& w, const std::string& path, std::string& err) {
    std::error_code ec;
    if (fs::path(path).has_parent_path()) fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream f(path);
    if (!f) { err = "no se puede escribir " + path; return false; }
    json nodes = json::array();
    for (const auto& n : w.nodes)
        nodes.push_back({{"id", n.id}, {"name", n.name}, {"type", n.type}, {"parent", n.parent}, {"props", n.props}});
    f << json{{"mockScene", 1}, {"nodes", nodes}}.dump(2);
    return true;
}
bool loadDoc(World& w, const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "no existe " + path; return false; }
    json j = json::parse(f, nullptr, false);
    if (j.is_discarded() || !j.contains("nodes")) { err = "no es una escena del motor simulado"; return false; }
    w.nodes.clear();
    for (const auto& n : j["nodes"])
        w.nodes.push_back({n.value("id", ""), n.value("name", ""), n.value("type", ""), n.value("parent", ""), n.value("props", json::array())});
    w.selected.clear();
    ++w.revision;
    return true;
}

int serve(int client) {
    World w = defaultWorld();
    remote::Message m;
    std::string err;
    auto reply = [&](const json& req, json body) {
        body["re"] = req.value("id", 0);
        if (!body.contains("type")) body["type"] = "ok";
        return remote::send(client, body);
    };
    while (remote::receive(client, m, &err)) {
        const json& q = m.json;
        const std::string t = q.value("type", "");
        bool sceneChanged = false, ok = true;
        if (t == "hello") {
            ok = reply(q, {{"type", "hello_ok"}, {"engine", "mock"}, {"version", "1.0"}, {"proto", remote::kProtocolVersion}});
        } else if (t == "get_scene") {
            json s = sceneMsg(w);
            ok = reply(q, s);
        } else if (t == "set_property") {
            Node* n = findNode(w, q.value("node", ""));
            json* p = n ? findProp(*n, q.value("name", "")) : nullptr;
            if (!p) ok = reply(q, {{"ok", false}, {"error", "no existe esa propiedad"}});
            else if ((*p)["readOnly"].get<bool>()) ok = reply(q, {{"ok", false}, {"error", "solo lectura"}});
            else { (*p)["value"] = q["value"]; ++w.revision; sceneChanged = true; ok = reply(q, {{"ok", true}}); }
        } else if (t == "select") {
            w.selected = q.value("node", ""); ++w.revision; sceneChanged = true; ok = reply(q, {{"ok", true}});
        } else if (t == "render") {
            const int rw = std::clamp(q.value("w", 64), 1, 4096), rh = std::clamp(q.value("h", 64), 1, 4096);
            std::vector<uint8_t> px = render(w, rw, rh);
            ok = remote::send(client, {{"type", "frame"}, {"re", q.value("id", 0)}, {"w", rw}, {"h", rh}, {"format", "rgba8"}}, &px);
        } else if (t == "input") {
            const std::string k = q.value("kind", "");
            if (k == "drag") {
                w.yaw -= q.value("dx", 0.0) * 0.01;
                w.pitch = std::clamp(w.pitch + q.value("dy", 0.0) * 0.01, 0.05, 1.45);
            } else if (k == "wheel") {
                w.dist = std::clamp(w.dist * std::pow(0.9, q.value("delta", 0.0)), 2.0, 80.0);
            } else if (k == "click") {
                const int cw = q.value("w", 1), ch = q.value("h", 1);
                const double cx = q.value("x", 0.0), cy = q.value("y", 0.0);
                std::string hit;
                for (const auto& d : layout(w, camera(w, cw, ch), cw, ch))   // de lejos a cerca: gana el último
                    if (std::abs(cx - d.sx) <= d.half && std::abs(cy - d.sy) <= d.half) hit = d.id;
                if (hit != w.selected) { w.selected = hit; ++w.revision; sceneChanged = true; }
            }
            ok = reply(q, {{"ok", true}});
        } else if (t == "play") {
            w.playing = q.value("on", false);
            w.playStart = now();
            ok = reply(q, {{"ok", true}}) && remote::send(client, {{"type", "state"}, {"playing", w.playing}});
        } else if (t == "open_project") {
            const std::string root = q.value("root", "");
            const bool exists = fs::is_directory(root);
            ok = reply(q, {{"ok", exists}, {"error", exists ? "" : "no existe " + root}});
        } else if (t == "new_project") {
            const std::string root = q.value("root", "");
            std::error_code ec;
            fs::create_directories(fs::path(root) / "scenes", ec);
            std::ofstream(fs::path(root) / "mock.project") << json{{"name", q.value("name", "")}}.dump(2);
            const std::string doc = (fs::path(root) / "scenes" / "main.mock.json").string();
            std::string e;
            const bool saved = saveDoc(w, doc, e);
            ok = reply(q, {{"ok", saved}, {"document", saved ? doc : ""}, {"error", e}});
        } else if (t == "save" || t == "load") {
            std::string e;
            const bool r = t == "save" ? saveDoc(w, q.value("path", ""), e) : loadDoc(w, q.value("path", ""), e);
            if (t == "load" && r) sceneChanged = true;
            ok = reply(q, {{"ok", r}, {"error", e}});
        } else if (t == "quit") {
            return 0;
        } else {
            ok = reply(q, {{"ok", false}, {"error", "mensaje desconocido: " + t}});
        }
        if (ok && sceneChanged) ok = remote::send(client, sceneMsg(w));
        if (!ok) break;
    }
    std::fprintf(stderr, "[mock_engine] editor desconectado (%s)\n", err.c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string sock;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--socket") == 0) sock = argv[i + 1];
    if (sock.empty()) { std::fprintf(stderr, "uso: %s --socket <ruta>\n", argv[0]); return 2; }

    const int srv = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (sock.size() >= sizeof(addr.sun_path)) { std::fprintf(stderr, "ruta de socket demasiado larga\n"); return 2; }
    std::strcpy(addr.sun_path, sock.c_str());
    ::unlink(sock.c_str());
    if (::bind(srv, (sockaddr*)&addr, sizeof addr) != 0 || ::listen(srv, 1) != 0) {
        std::perror("[mock_engine] bind/listen");
        return 1;
    }
    std::fprintf(stderr, "[mock_engine] escuchando en %s\n", sock.c_str());
    const int client = ::accept(srv, nullptr, nullptr);
    ::close(srv);
    ::unlink(sock.c_str());
    if (client < 0) { std::perror("[mock_engine] accept"); return 1; }
    const int r = serve(client);
    ::close(client);
    return r;
}
