#include "remote_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>

using nlohmann::json;

namespace {

int connectUnix(const std::string& path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) { ::close(fd); return -1; }
    std::strcpy(addr.sun_path, path.c_str());
    if (::connect(fd, (sockaddr*)&addr, sizeof addr) != 0) { ::close(fd); return -1; }
    return fd;
}

std::string tempSocketPath() {
    static std::atomic<int> counter{0};
    const char* rt = std::getenv("XDG_RUNTIME_DIR");
    const std::string dir = (rt && *rt) ? rt : "/tmp";
    return dir + "/haruka-remote-" + std::to_string(::getpid()) + "-" + std::to_string(counter++) + ".sock";
}

json toJsonValue(const SceneProperty& p) {
    if (p.type == "string") return p.str;
    if (p.type == "bool") return !p.num.empty() && p.num[0] != 0.0;
    if (p.type == "int") return p.num.empty() ? 0 : (long long)p.num[0];
    if (p.type == "vec3" || p.type == "color") return p.num;
    return p.num.empty() ? 0.0 : p.num[0];
}

SceneProperty fromJson(const json& j) {
    SceneProperty p;
    p.name = j.value("name", "");
    p.type = j.value("type", "");
    p.readOnly = j.value("readOnly", false);
    const json& v = j.contains("value") ? j["value"] : json();
    if (v.is_string()) p.str = v.get<std::string>();
    else if (v.is_boolean()) p.num = {v.get<bool>() ? 1.0 : 0.0};
    else if (v.is_number()) p.num = {v.get<double>()};
    else if (v.is_array()) for (const auto& x : v) p.num.push_back(x.is_number() ? x.get<double>() : 0.0);
    return p;
}

} // namespace

bool RemoteClient::launch(const std::string& engineExe, std::string& err, int timeoutMs) {
    disconnect();
    const std::string sock = tempSocketPath();
    // argv ANTES del fork: entre fork y exec, en un proceso con hilos, solo vale lo async-signal-safe.
    std::vector<std::string> args = {engineExe, "--socket", sock};
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0) { err = std::string("fork: ") + std::strerror(errno); return false; }
    if (pid == 0) {
        ::execv(argv[0], argv.data());
        ::_exit(127);   // exec falló: el padre lo ve como "el motor terminó"
    }
    child = pid;

    // El motor tarda un poco en abrir el socket: reintentar hasta el límite, vigilando que siga vivo.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        int status = 0;
        if (::waitpid(child, &status, WNOHANG) == child) {
            child = -1;
            err = "el motor termino al arrancar (" +
                  (WIFEXITED(status) ? "codigo " + std::to_string(WEXITSTATUS(status)) : std::string("señal")) +
                  (WIFEXITED(status) && WEXITSTATUS(status) == 127 ? ": no se pudo ejecutar " + engineExe : "") + ")";
            return false;
        }
        fd = connectUnix(sock);
        if (fd >= 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (fd < 0) {
        err = "el motor no abrio " + sock + " en " + std::to_string(timeoutMs) + " ms";
        disconnect();
        return false;
    }
    alive = true;
    reader = std::thread(&RemoteClient::readerLoop, this);
    if (!handshake(err, timeoutMs)) { disconnect(); return false; }
    return true;
}

bool RemoteClient::connectTo(const std::string& socketPath, std::string& err, int timeoutMs) {
    disconnect();
    fd = connectUnix(socketPath);
    if (fd < 0) { err = "no se pudo conectar a " + socketPath + ": " + std::strerror(errno); return false; }
    alive = true;
    reader = std::thread(&RemoteClient::readerLoop, this);
    if (!handshake(err, timeoutMs)) { disconnect(); return false; }
    return true;
}

bool RemoteClient::handshake(std::string& err, int timeoutMs) {
    auto hello = call({{"type", "hello"}, {"proto", remote::kProtocolVersion}}, timeoutMs);
    if (!hello) { err = "el motor no contesto al saludo: " + lastError(); return false; }
    if ((*hello).value("proto", 0) != remote::kProtocolVersion) {
        err = "protocolo del motor v" + std::to_string((*hello).value("proto", 0)) + ", el editor habla v" +
              std::to_string(remote::kProtocolVersion);
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(m);
        engine = (*hello).value("engine", "?") + " " + (*hello).value("version", "");
    }
    auto scene = call({{"type", "get_scene"}}, timeoutMs);   // el lector la aplica al llegar
    if (!scene) { err = "el motor no mando su escena"; return false; }
    return true;
}

void RemoteClient::disconnect() {
    if (fd >= 0) {
        if (alive) {
            std::lock_guard<std::mutex> lk(writeMutex);
            remote::send(fd, {{"type", "quit"}, {"id", 0}});
        }
        ::shutdown(fd, SHUT_RDWR);   // desbloquea el recv del lector
    }
    if (reader.joinable()) reader.join();
    if (fd >= 0) { ::close(fd); fd = -1; }
    alive = false;
    if (child > 0) {
        // Se le da un momento para salir por su cuenta (acaba de recibir `quit`); si no, a la fuerza.
        for (int i = 0; i < 100; ++i) {
            if (::waitpid(child, nullptr, WNOHANG) == child) { child = -1; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (child > 0) { ::kill(child, SIGKILL); ::waitpid(child, nullptr, 0); child = -1; }
    }
    std::lock_guard<std::mutex> lk(m);
    frameInFlight = false;
    cv.notify_all();
}

void RemoteClient::fail(const std::string& why) {
    {
        std::lock_guard<std::mutex> lk(m);
        if (error.empty() || alive) error = why;
    }
    alive = false;
    cv.notify_all();
}

std::string RemoteClient::lastError() const { std::lock_guard<std::mutex> lk(m); return error; }
std::string RemoteClient::engineName() const { std::lock_guard<std::mutex> lk(m); return engine; }

int RemoteClient::nextRequestId() { std::lock_guard<std::mutex> lk(m); return nextId++; }

void RemoteClient::post(json request) {
    if (!alive) return;
    request["id"] = nextRequestId();
    std::lock_guard<std::mutex> lk(writeMutex);
    if (!remote::send(fd, request)) fail("no se pudo escribir al motor");
}

std::optional<json> RemoteClient::call(json request, int timeoutMs) {
    if (!alive) return std::nullopt;
    const int id = nextRequestId();
    request["id"] = id;
    {
        std::lock_guard<std::mutex> lk(m);
        waiting.insert(id);
    }
    {
        std::lock_guard<std::mutex> lk(writeMutex);
        if (!remote::send(fd, request)) fail("no se pudo escribir al motor");
    }
    std::unique_lock<std::mutex> lk(m);
    cv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&] { return replies.count(id) || !alive; });
    waiting.erase(id);
    auto it = replies.find(id);
    if (it == replies.end()) {
        if (alive && error.empty()) error = "el motor no contesto a '" + request.value("type", "") + "' en " +
                                            std::to_string(timeoutMs) + " ms";
        return std::nullopt;
    }
    json r = std::move(it->second);
    replies.erase(it);
    return r;
}

void RemoteClient::readerLoop() {
    remote::Message msg;
    std::string why;
    while (remote::receive(fd, msg, &why)) {
        const json& j = msg.json;
        const std::string type = j.value("type", "");
        const int re = j.value("re", -1);

        if (type == "frame") {
            std::lock_guard<std::mutex> lk(m);
            const int w = j.value("w", 0), h = j.value("h", 0);
            if (w > 0 && h > 0 && msg.binary.size() == (size_t)w * h * 4) {
                latest.w = w;
                latest.h = h;
                latest.rgba = std::move(msg.binary);
                ++latest.seq;
            } else {
                error = "frame con tamaño incoherente";
            }
            frameInFlight = false;
            continue;
        }
        if (type == "scene") applyScene(j);
        if (type == "state") { std::lock_guard<std::mutex> lk(m); isPlaying = j.value("playing", false); }

        if (re >= 0) {
            std::lock_guard<std::mutex> lk(m);
            if (waiting.count(re)) {
                replies[re] = j;
                cv.notify_all();
            } else if (j.contains("ok") && !j.value("ok", true)) {
                // Respuesta a algo enviado sin esperar (set_property…): solo importa si falló.
                error = "el motor rechazo una peticion: " + j.value("error", std::string("?"));
                std::cerr << "[remote] " << error << std::endl;
            }
        }
    }
    fail(why.empty() ? "conexion cerrada" : why);
}

void RemoteClient::applyScene(const json& s) {
    std::vector<SceneNode> nodes;
    for (const auto& n : s.value("nodes", json::array())) {
        SceneNode node;
        node.id = n.value("id", "");
        node.name = n.value("name", "");
        node.type = n.value("type", "");
        node.parent = n.value("parent", "");
        for (const auto& p : n.value("props", json::array())) node.properties.push_back(fromJson(p));
        nodes.push_back(std::move(node));
    }
    std::lock_guard<std::mutex> lk(m);
    sceneNodes = std::move(nodes);
    sceneSelected = s.value("selected", "");
    sceneRevision = s.value("revision", sceneRevision + 1);
}

void RemoteClient::requestFrame(int w, int h) {
    {
        std::lock_guard<std::mutex> lk(m);
        if (frameInFlight || !alive) return;
        frameInFlight = true;
    }
    post({{"type", "render"}, {"w", w}, {"h", h}});
}

bool RemoteClient::takeFrame(Frame& out, uint64_t lastSeq) const {
    std::lock_guard<std::mutex> lk(m);
    if (latest.seq == lastSeq || latest.seq == 0) return false;
    out = latest;
    return true;
}

void RemoteClient::drag(double dx, double dy) { post({{"type", "input"}, {"kind", "drag"}, {"dx", dx}, {"dy", dy}}); }
void RemoteClient::wheel(double delta) { post({{"type", "input"}, {"kind", "wheel"}, {"delta", delta}}); }
void RemoteClient::click(double x, double y, int w, int h) {
    post({{"type", "input"}, {"kind", "click"}, {"x", x}, {"y", y}, {"w", w}, {"h", h}});
}

void RemoteClient::setPlaying(bool on) { post({{"type", "play"}, {"on", on}}); }
bool RemoteClient::playing() const { std::lock_guard<std::mutex> lk(m); return isPlaying; }

uint64_t RemoteClient::revision() const { std::lock_guard<std::mutex> lk(m); return sceneRevision; }
std::vector<SceneNode> RemoteClient::nodes() const { std::lock_guard<std::mutex> lk(m); return sceneNodes; }
std::string RemoteClient::selected() const { std::lock_guard<std::mutex> lk(m); return sceneSelected; }
void RemoteClient::select(const std::string& nodeId) { post({{"type", "select"}, {"node", nodeId}}); }
void RemoteClient::setProperty(const std::string& nodeId, const SceneProperty& value) {
    post({{"type", "set_property"}, {"node", nodeId}, {"name", value.name}, {"value", toJsonValue(value)}});
}
