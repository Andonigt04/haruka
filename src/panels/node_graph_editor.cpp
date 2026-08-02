#include "node_graph_editor.h"

#include "core/components/material_component.h"
#include "io/image_writer.h"
#include "tools/procgraph/proc_graph.h"
#include "tools/procgraph/proc_noise.h"
#include "tools/procgraph/proc_math.h"
#include "tools/procgraph/proc_texture.h"
#include "tools/procgraph/proc_organic.h"
#include "rhi/rhi_device.h"
#include "core/application.h"
#include "renderer/render_target.h"
#include "renderer/motor_instance.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

NodeGraphEditorPanel::NodeGraphEditorPanel() = default;
NodeGraphEditorPanel::~NodeGraphEditorPanel() = default;

using Haruka::Tools::ProcGraph::DataType;
using Haruka::Tools::ProcGraph::Graph;
using Haruka::Tools::ProcGraph::Node;
using Haruka::Tools::ProcGraph::SocketDesc;
using Haruka::Tools::ProcGraph::Value;

namespace {

// ===========================================================================
// Lecturas JSON tolerantes: nlohmann lanza excepciones si se lee con un tipo
// distinto al almacenado (p.ej. `get<int>` sobre un float), así que leemos
// number/boolean y casteamos manualmente.
// ===========================================================================
int jint(const nlohmann::json& j, const std::string& k, int def) {
    if (!j.is_object() || !j.contains(k)) return def;
    const auto& v = j[k];
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    if (v.is_number()) return (int)v.get<double>();
    return def;
}
float jfloat(const nlohmann::json& j, const std::string& k, float def) {
    if (!j.is_object() || !j.contains(k)) return def;
    const auto& v = j[k];
    if (v.is_boolean()) return v.get<bool>() ? 1.0f : 0.0f;
    if (v.is_number()) return (float)v.get<double>();
    return def;
}
bool jbool(const nlohmann::json& j, const std::string& k, bool def) {
    if (!j.is_object() || !j.contains(k)) return def;
    const auto& v = j[k];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number()) return v.get<double>() != 0.0;
    return def;
}

// ===========================================================================
// Catálogo de nodos del editor
// ===========================================================================
enum ParamKind { P_FLOAT, P_INT, P_COMBO, P_RAMP, P_BOOL };

struct ParamInfo {
    std::string key, label;
    int kind = P_FLOAT;
    float def = 0, min = 0, max = 1;
    std::vector<std::string> options;
};

struct NodeInfo {
    std::string type, label, category;
    std::vector<ParamInfo> params;
    std::vector<std::pair<std::string, int>> inputs;   // (name, kind)
    std::vector<std::pair<std::string, int>> outputs;  // (name, kind)
};

const std::vector<NodeInfo>& nodeCatalog() {
    static const std::vector<NodeInfo> cat = {
        // --- Noise ---
        { "perlin", "Perlin Noise", "Noise",
          { {"scale", "Scale", P_FLOAT, 2.0f, 0.01f, 20.0f},
            {"seed", "Seed", P_INT, 0, -1000, 1000} },
          {}, {{"Value", 0}} },
        { "fbm", "FBM", "Noise",
          { {"mode", "Mode", P_COMBO, 0, 0, 0, {"Sum", "Max", "Min", "AbsSum"}},
            {"scale", "Scale", P_FLOAT, 1.0f, 0.01f, 20.0f},
            {"octaves", "Octaves", P_INT, 4, 1, 8},
            {"lacunarity", "Lacunarity", P_FLOAT, 2.0f, 0.1f, 4.0f},
            {"gain", "Gain", P_FLOAT, 0.5f, 0.0f, 1.0f},
            {"seed", "Seed", P_INT, 0, -1000, 1000} },
          {}, {{"Value", 0}} },
        { "voronoi", "Voronoi", "Noise",
          { {"metric", "Metric", P_COMBO, 0, 0, 0, {"Euclidean", "Manhattan", "Chebyshev"}},
            {"output", "Output", P_COMBO, 0, 0, 0, {"F1", "F2", "F1-F2", "Edge"}},
            {"scale", "Scale", P_FLOAT, 2.0f, 0.01f, 20.0f},
            {"seed", "Seed", P_INT, 0, -1000, 1000} },
          {}, {{"Distance", 0}, {"Cell", 0}} },
        { "voronoi_edge", "Voronoi Edge", "Noise",
          { {"metric", "Metric", P_COMBO, 0, 0, 0, {"Euclidean", "Manhattan", "Chebyshev"}},
            {"scale", "Scale", P_FLOAT, 2.0f, 0.01f, 20.0f},
            {"seed", "Seed", P_INT, 0, -1000, 1000} },
          {}, {{"Edge", 0}} },
        { "white", "White Noise", "Noise",
          { {"scale", "Scale", P_FLOAT, 2.0f, 0.01f, 20.0f},
            {"seed", "Seed", P_INT, 0, -1000, 1000} },
          {}, {{"Value", 0}} },
        // --- Organic (árboles estilo anime) ---
        { "knot", "Knots (nudos)", "Organic",
          { {"count", "Count", P_INT, 8, 1, 64},
            {"size", "Size", P_FLOAT, 1.0f, 0.2f, 3.0f},
            {"seed", "Seed", P_INT, 0, -1000, 1000} },
          {}, {{"Mask", 0}} },
        { "branch", "Branches (ramas)", "Organic",
          { {"width", "Width", P_FLOAT, 0.02f, 0.002f, 0.2f},
            {"depth", "Depth", P_INT, 4, 2, 6},
            {"seed", "Seed", P_INT, 0, -1000, 1000} },
          {}, {{"Mask", 0}} },
        // --- Math ---
        { "math", "Math", "Math",
          { {"op", "Op", P_COMBO, 0, 0, 0,
             {"Add", "Subtract", "Multiply", "Divide", "Min", "Max", "Power", "ATan2", "Step", "SmoothStep"}} },
          {{"A", 0}, {"B", 0}}, {{"Result", 0}} },
        { "unary", "Unary", "Math",
          { {"op", "Op", P_COMBO, 0, 0, 0,
             {"Abs", "Negate", "Sin", "Cos", "Tan", "Sqrt", "Floor", "Ceil", "Round", "Fract", "Log", "Exp", "Normalize"}} },
          {{"Value", 0}}, {{"Result", 0}} },
        { "blend", "Blend", "Math",
          { {"mode", "Mode", P_COMBO, 0, 0, 0,
             {"Mix", "Add", "Subtract", "Multiply", "Screen", "Overlay", "SoftLight", "Burn", "Dodge"}} },
          {{"A", 0}, {"B", 0}, {"Factor", 0}}, {{"Result", 0}} },
        { "clamp", "Clamp", "Math",
          {},
          {{"Value", 0}, {"Min", 0}, {"Max", 0}}, {{"Result", 0}} },
        { "maprange", "Map Range", "Math",
          {},
          {{"Value", 0}, {"In Min", 0}, {"In Max", 0}, {"Out Min", 0}, {"Out Max", 0}}, {{"Result", 0}} },
        { "ramp", "Color Ramp", "Math",
          { {"stops", "Stops", P_RAMP, 0, 0, 0} },
          {{"Factor", 0}}, {{"Result", 0}} },
        // --- Input ---
        { "const", "Constant", "Input",
          { {"value", "Value", P_FLOAT, 0.0f, -10.0f, 10.0f} },
          {}, {{"Value", 0}} },
        // --- Color ---
        { "gradient", "Gradient", "Color",
          { {"type", "Type", P_COMBO, 0, 0, 0, {"Linear X", "Linear Y", "Linear Z", "Radial"}},
            {"scale", "Scale", P_FLOAT, 1.0f, 0.01f, 20.0f},
            {"offset", "Offset", P_FLOAT, 0.0f, -10.0f, 10.0f} },
          {}, {{"Value", 0}} },
        { "checker", "Checker", "Color",
          { {"size", "Size", P_FLOAT, 1.0f, 0.01f, 20.0f} },
          {}, {{"Value", 0}} },
        { "combine", "Combine RGB", "Color",
          {},
          {{"R", 0}, {"G", 0}, {"B", 0}}, {{"Color", 1}} },
        { "domain", "Domain", "Color",
          { {"op", "Op", P_COMBO, 0, 0, 0, {"Scale", "Offset", "Tile", "Mirror", "Warp"}},
            {"amount", "Amount", P_FLOAT, 1.0f, 0.0f, 20.0f} },
          {{"Source", 0}}, {{"Result", 0}} },
        // --- Output ---
        { "output", "Material Output", "Output",
          { {"normalFromHeight", "Normal from height", P_BOOL, 1, 0, 1} },
          {{"Albedo", 2}, {"Normal", 2}, {"Metallic", 2}, {"Roughness", 2}, {"AO", 2}},
          {} },
    };
    return cat;
}

const NodeInfo* nodeInfo(const std::string& type) {
    for (const auto& d : nodeCatalog()) {
        if (d.type == type) return &d;
    }
    return nullptr;
}

// Sockets efectivos de un nodo (los de `domain` dependen de su op).
std::pair<std::vector<std::pair<std::string, int>>, std::vector<std::pair<std::string, int>>>
socketsFor(const std::string& type, const nlohmann::json& params) {
    const NodeInfo* info = nodeInfo(type);
    if (!info) return {};
    if (type == "domain") {
        std::vector<std::pair<std::string, int>> ins = {{"Source", 0}};
        int op = jint(params, "op", 0);
        switch (op) {
            case 0: ins.push_back({"Scale", 0}); break;
            case 1: ins.push_back({"Offset", 0}); break;
            case 2: ins.push_back({"Size", 0}); break;
            case 3: ins.push_back({"Size", 0}); break;
            default: ins.push_back({"Amount", 0}); ins.push_back({"Noise", 0}); break;
        }
        return {ins, info->outputs};
    }
    return {info->inputs, info->outputs};
}

int outputKind(const std::string& type, int idx) {
    auto sock = socketsFor(type, {});
    if (idx >= 0 && idx < (int)sock.second.size()) return sock.second[idx].second;
    return 0;
}

nlohmann::json defaultParams(const std::string& type) {
    nlohmann::json j = nlohmann::json::object();
    const NodeInfo* info = nodeInfo(type);
    if (!info) return j;
    for (const auto& p : info->params) {
        if (p.kind == P_RAMP) {
            j[p.key] = nlohmann::json::array({nlohmann::json::array({0.0, 0.0}),
                                              nlohmann::json::array({1.0, 1.0})});
        } else if (p.kind == P_INT || p.kind == P_COMBO) {
            j[p.key] = (int)p.def;
        } else if (p.kind == P_BOOL) {
            j[p.key] = (bool)(p.def != 0.0f);
        } else {
            j[p.key] = p.def;
        }
    }
    return j;
}

// ===========================================================================
// Nodo del EDITOR que combina 3 floats en un Vec3 (el motor no lo trae).
// ===========================================================================
class CombineRGBNode : public Node {
public:
    std::string name() const override { return "Combine RGB"; }
    std::vector<SocketDesc> inputs() const override {
        return {{"R", DataType::Float}, {"G", DataType::Float}, {"B", DataType::Float}};
    }
    std::vector<SocketDesc> outputs() const override {
        return {{"Color", DataType::Vec3}};
    }
    void evaluate(const Value* inputs, int, Value* outputs, int,
                  float, float, float) override {
        outputs[0] = Value::Vec3(inputs[0].asFloat(), inputs[1].asFloat(), inputs[2].asFloat());
    }
};

// Construye el nodo del motor correspondiente a un tipo del editor.
std::unique_ptr<Node> engineNode(const std::string& type, const nlohmann::json& p) {
    using namespace Haruka::Tools::ProcGraph;
    if (type == "const")         return std::make_unique<ConstNode>(jfloat(p, "value", 0.0f));
    if (type == "perlin")        return std::make_unique<PerlinNode>(jint(p, "seed", 0), jfloat(p, "scale", 2.0f));
    if (type == "white")         return std::make_unique<WhiteNode>(jint(p, "seed", 0), jfloat(p, "scale", 2.0f));
    if (type == "voronoi")       return std::make_unique<VoronoiNode>(jint(p, "seed", 0), jfloat(p, "scale", 2.0f),
                                                                      (VoronoiNode::Metric)jint(p, "metric", 0),
                                                                      (VoronoiNode::Output)jint(p, "output", 0));
    if (type == "voronoi_edge")  return std::make_unique<VoronoiEdgeNode>(jint(p, "seed", 0), jfloat(p, "scale", 2.0f),
                                                                          (VoronoiNode::Metric)jint(p, "metric", 0));
    if (type == "fbm") {
        auto n = std::make_unique<FBMNode>(jint(p, "octaves", 4), jfloat(p, "lacunarity", 2.0f),
                                           jfloat(p, "gain", 0.5f), (FBMNode::Mode)jint(p, "mode", 0),
                                           jint(p, "seed", 0));
        n->setNoiseFn(PerlinNode::perlin);
        return n;
    }
    if (type == "gradient")      return std::make_unique<GradientNode>((GradientNode::Type)jint(p, "type", 0));
    if (type == "checker")       return std::make_unique<CheckerNode>(jfloat(p, "size", 1.0f));
    if (type == "math")          return std::make_unique<MathNode>((MathOp)jint(p, "op", 0));
    if (type == "unary")         return std::make_unique<UnaryNode>((UnaryOp)jint(p, "op", 0));
    if (type == "blend")         return std::make_unique<BlendNode>((BlendMode)jint(p, "mode", 0));
    if (type == "ramp") {
        auto n = std::make_unique<ColorRampNode>();
        if (p.contains("stops") && p["stops"].is_array()) {
            std::vector<ColorStop> stops;
            for (const auto& s : p["stops"]) {
                if (s.is_array() && s.size() >= 2) stops.push_back({(float)s[0], (float)s[1]});
            }
            if (!stops.empty()) n->setStops(stops);
        }
        return n;
    }
    if (type == "clamp")         return std::make_unique<ClampNode>(jfloat(p, "min", 0.0f), jfloat(p, "max", 1.0f));
    if (type == "maprange")      return std::make_unique<MapRangeNode>(jfloat(p, "inMin", 0.0f), jfloat(p, "inMax", 1.0f),
                                                                       jfloat(p, "outMin", 0.0f), jfloat(p, "outMax", 1.0f));
    if (type == "domain")        return std::make_unique<DomainNode>((DomainNode::Op)jint(p, "op", 0), jfloat(p, "amount", 1.0f));
    if (type == "combine")       return std::make_unique<CombineRGBNode>();
    if (type == "knot")          return std::make_unique<KnotNode>(jint(p, "seed", 0), jint(p, "count", 8), jfloat(p, "size", 1.0f));
    if (type == "branch")        return std::make_unique<BranchNode>(jint(p, "seed", 0), jfloat(p, "width", 0.02f), jint(p, "depth", 4));
    return nullptr;
}

// Slots del Material Output
const char* slotByIndex(int i) {
    static const char* s[5] = {"albedo", "normal", "metallic", "roughness", "ao"};
    return (i >= 0 && i < 5) ? s[i] : "albedo";
}
int slotIndex(const std::string& slot) {
    if (slot == "normal") return 1;
    if (slot == "metallic") return 2;
    if (slot == "roughness") return 3;
    if (slot == "ao") return 4;
    return 0;
}

std::string sanitizeName(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (std::isalnum((unsigned char)c) || c == '_' || c == '-') out += c;
        else out += '_';
    }
    if (out.empty()) out = "obj";
    return out;
}

ImU32 categoryColor(const std::string& cat) {
    if (cat == "Noise")  return IM_COL32(36, 110, 120, 255);
    if (cat == "Math")   return IM_COL32(150, 90, 20, 255);
    if (cat == "Input")  return IM_COL32(60, 120, 60, 255);
    if (cat == "Color")  return IM_COL32(110, 60, 140, 255);
    if (cat == "Output") return IM_COL32(150, 50, 50, 255);
    return IM_COL32(60, 60, 70, 255);
}

} // namespace

// ===========================================================================
// Modelo del grafo
// ===========================================================================
void NodeGraphEditorPanel::setScene(Haruka::SceneManager* scene) {
    currentScene = scene;
    if (!scene) {
        selectedObject = nullptr;
        selectedObjectName.clear();
        nodes.clear();
        connections.clear();
        nextNodeId = 1;
        selectedNodeId = -1;
        previewDirty = true;
    }
}

void NodeGraphEditorPanel::setSelectedObject(Haruka::SceneObject* obj) {
    if (obj == selectedObject) return;
    if (previewTex.id) {
        if (auto* dev = Haruka::RHI::device()) dev->destroy(previewTex);
        previewTex = {};
    }
    previewGL = 0;
    previewStatus.clear();

    selectedObject = obj;
    selectedObjectName = obj ? obj->name : "";
    selectedNodeId = -1;
    nodes.clear();
    connections.clear();
    nextNodeId = 1;

    if (obj) {
        if (!obj->material) {
            obj->material = std::make_shared<Haruka::MaterialComponent>();
            obj->material->name = obj->name + "_Material";
        }
        if (obj->properties.is_object() && obj->properties.contains("treeSeed") &&
            obj->properties["treeSeed"].is_number()) {
            treeSeed = obj->properties["treeSeed"].get<int>();
        }
        if (obj->properties.is_object() && obj->properties.contains("materialGraph") &&
            obj->properties["materialGraph"].is_object()) {
            deserializeGraph(obj->properties["materialGraph"]);
        } else {
            createDefaultGraph();
        }
    }
    previewDirty = true;
}

void NodeGraphEditorPanel::createDefaultGraph() {
    nodes.clear();
    connections.clear();
    nextNodeId = 1;
    selectedNodeId = -1;
    int out = addNodeRaw("output", 60, 120);
    int pn = addNodeRaw("perlin", -320, 150);
    connections.push_back({pn, 0, out, 0});
}

void NodeGraphEditorPanel::createTreeGraph(bool foliage) {
    nodes.clear();
    connections.clear();
    nextNodeId = 1;
    selectedNodeId = -1;

    const int S = treeSeed;
    auto setF = [&](int id, const char* key, float v) { findNode(id)->params[key] = v; };
    auto setI = [&](int id, const char* key, int v)  { findNode(id)->params[key] = v; };

    int out = addNodeRaw("output", 720, foliage ? -20 : 40);
    findNode(out)->params["normalFromHeight"] = true;

    int grain = addNodeRaw("perlin", -760, 40);
    setF(grain, "scale", foliage ? 9.0f : 5.0f);
    setI(grain, "seed", S);

    int norm = addNodeRaw("maprange", -520, 40);
    setF(norm, "inMin", -1.0f);
    setF(norm, "inMax", 1.0f);
    setF(norm, "outMin", 0.0f);
    setF(norm, "outMax", 1.0f);
    connections.push_back({grain, 0, norm, 0});

    int branch = -1, knot = -1, fbm = -1;
    if (foliage) {
        // Copa: fbm para la forma + detalle de hoja en la misma perlin (escala alta).
        fbm = addNodeRaw("fbm", -760, 160);
        setF(fbm, "scale", 3.5f);
        setI(fbm, "octaves", 4);
        setI(fbm, "gain", 0.5f);
        setI(fbm, "seed", S + 1);
        int norm2 = addNodeRaw("maprange", -520, 160);
        setF(norm2, "inMin", -1.0f);
        setF(norm2, "inMax", 1.0f);
        setF(norm2, "outMin", 0.0f);
        setF(norm2, "outMax", 1.0f);
        connections.push_back({fbm, 0, norm2, 0});

        int detail = addNodeRaw("math", -300, 100);
        setI(detail, "op", 2); // Multiply: copa * detalle = copa con hojitas
        connections.push_back({norm, 0, detail, 0});
        connections.push_back({norm2, 0, detail, 1});
        norm = detail;
    } else {
        // Corteza: ramas (rutas) + nudos restan a la madera base.
        branch = addNodeRaw("branch", -760, 220);
        setF(branch, "width", 0.02f);
        setI(branch, "depth", 4);
        setI(branch, "seed", S);

        knot = addNodeRaw("knot", -760, 340);
        setI(knot, "count", 12);
        setF(knot, "size", 1.0f);
        setI(knot, "seed", S + 2);

        int dark = addNodeRaw("math", -300, 200);
        setI(dark, "op", 1); // Subtract: madera - ramas
        connections.push_back({norm, 0, dark, 0});
        connections.push_back({branch, 0, dark, 1});

        int withKnots = addNodeRaw("math", -100, 300);
        setI(withKnots, "op", 1); // Subtract: - nudos
        connections.push_back({dark, 0, withKnots, 0});
        connections.push_back({knot, 0, withKnots, 1});
        norm = withKnots;
    }

    // Anime: 3 rampas una por canal -> Combine RGB (el color es Vec3).
    int rR = addNodeRaw("ramp", 60, 100);
    int rG = addNodeRaw("ramp", 60, 200);
    int rB = addNodeRaw("ramp", 60, 300);
    auto setStops = [&](int id, float v0, float v1, float v2, float v3) {
        findNode(id)->params["stops"] = nlohmann::json::array(
            {nlohmann::json::array({0.0f, v0}),
             nlohmann::json::array({0.45f, v1}),
             nlohmann::json::array({0.8f, v2}),
             nlohmann::json::array({1.0f, v3})});
    };
    if (foliage) {
        setStops(rR, 0.10f, 0.22f, 0.40f, 0.55f);
        setStops(rG, 0.32f, 0.50f, 0.68f, 0.78f);
        setStops(rB, 0.12f, 0.22f, 0.36f, 0.45f);
    } else {
        setStops(rR, 0.14f, 0.30f, 0.50f, 0.68f);
        setStops(rG, 0.09f, 0.21f, 0.37f, 0.52f);
        setStops(rB, 0.08f, 0.15f, 0.26f, 0.38f);
    }
    connections.push_back({norm, 0, rR, 0});
    connections.push_back({norm, 0, rG, 0});
    connections.push_back({norm, 0, rB, 0});

    int combine = addNodeRaw("combine", 340, 180);
    connections.push_back({rR, 0, combine, 0});
    connections.push_back({rG, 0, combine, 1});
    connections.push_back({rB, 0, combine, 2});
    connections.push_back({combine, 0, out, 0});

    if (selectedObject && selectedObject->properties.is_object())
        selectedObject->properties["treeSeed"] = S;
    markDirty();
}

NodeGraphEditorPanel::GNode* NodeGraphEditorPanel::findNode(int id) {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

const NodeGraphEditorPanel::GNode* NodeGraphEditorPanel::findNode(int id) const {
    for (const auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

int NodeGraphEditorPanel::outputNodeId() const {
    for (const auto& n : nodes) if (n.type == "output") return n.id;
    return -1;
}

int NodeGraphEditorPanel::addNodeRaw(const std::string& type, float x, float y) {
    if (type == "output") {
        int existing = outputNodeId();
        if (existing >= 0) return existing;
    }
    GNode n;
    n.id = nextNodeId++;
    n.type = type;
    n.x = x;
    n.y = y;
    n.params = defaultParams(type);
    nodes.push_back(std::move(n));
    selectedNodeId = n.id;
    return n.id;
}

void NodeGraphEditorPanel::removeNode(int id) {
    connections.erase(std::remove_if(connections.begin(), connections.end(),
        [&](const GConnection& c) { return c.from == id || c.to == id; }),
        connections.end());
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
        [&](const GNode& n) { return n.id == id; }),
        nodes.end());
    if (selectedNodeId == id) selectedNodeId = -1;
    markDirty();
}

void NodeGraphEditorPanel::duplicateNode(int id) {
    const GNode* src = findNode(id);
    if (!src) return;
    int nid = addNodeRaw(src->type, src->x + 30, src->y + 30);
    GNode* dst = findNode(nid);
    if (dst) dst->params = src->params;
    for (const auto& c : connections) {
        if (c.to == id) connections.push_back({c.from, c.fromOut, nid, c.toIn});
    }
    selectedNodeId = nid;
    markDirty();
}

void NodeGraphEditorPanel::markDirty() {
    syncToObject();
    previewDirty = true;
    if (onSceneChanged) onSceneChanged();
}

void NodeGraphEditorPanel::syncToObject() {
    if (!selectedObject) return;
    if (!selectedObject->properties.is_object()) selectedObject->properties = nlohmann::json::object();
    selectedObject->properties["materialGraph"] = serializeGraph();
}

nlohmann::json NodeGraphEditorPanel::serializeGraph() const {
    nlohmann::json j = nlohmann::json::object();
    j["version"] = 1;
    j["bakeSize"] = bakeSize;
    j["previewSlot"] = previewSlot;
    j["normalFromHeight"] = normalFromHeight;
    j["nodes"] = nlohmann::json::array();
    for (const auto& n : nodes) {
        nlohmann::json nj;
        nj["id"] = n.id;
        nj["type"] = n.type;
        nj["x"] = n.x;
        nj["y"] = n.y;
        nj["params"] = n.params;
        j["nodes"].push_back(nj);
    }
    j["connections"] = nlohmann::json::array();
    for (const auto& c : connections) {
        nlohmann::json cj;
        cj["from"] = c.from;
        cj["fromOut"] = c.fromOut;
        cj["to"] = c.to;
        cj["toIn"] = c.toIn;
        j["connections"].push_back(cj);
    }
    return j;
}

void NodeGraphEditorPanel::deserializeGraph(const nlohmann::json& j) {
    nodes.clear();
    connections.clear();
    nextNodeId = 1;
    selectedNodeId = -1;
    if (!j.is_object()) return;

    if (j.contains("bakeSize") && j["bakeSize"].is_number_integer()) bakeSize = j["bakeSize"];
    if (j.contains("previewSlot") && j["previewSlot"].is_string()) previewSlot = j["previewSlot"];
    if (j.contains("normalFromHeight") && j["normalFromHeight"].is_boolean()) normalFromHeight = j["normalFromHeight"];

    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (const auto& nj : j["nodes"]) {
            if (!nj.is_object()) continue;
            GNode n;
            n.id = nj.value("id", nextNodeId);
            n.type = nj.value("type", std::string("const"));
            n.x = nj.value("x", 0.0f);
            n.y = nj.value("y", 0.0f);
            n.params = (nj.contains("params") && nj["params"].is_object())
                           ? nj["params"] : defaultParams(n.type);
            nextNodeId = std::max(nextNodeId, n.id + 1);
            nodes.push_back(std::move(n));
        }
    }
    if (j.contains("connections") && j["connections"].is_array()) {
        for (const auto& cj : j["connections"]) {
            if (!cj.is_object()) continue;
            GConnection c;
            c.from = cj.value("from", -1);
            c.fromOut = cj.value("fromOut", 0);
            c.to = cj.value("to", -1);
            c.toIn = cj.value("toIn", 0);
            if (findNode(c.from) && findNode(c.to)) connections.push_back(c);
        }
    }
    previewDirty = true;
}

// ===========================================================================
// Canvas
// ===========================================================================
ImVec2 NodeGraphEditorPanel::screenOfWorld(float wx, float wy) const {
    return ImVec2(canvasOrigin.x + wx * zoom, canvasOrigin.y + wy * zoom);
}

ImVec2 NodeGraphEditorPanel::worldOfScreen(float sx, float sy) const {
    return ImVec2((sx - canvasOrigin.x) / zoom, (sy - canvasOrigin.y) / zoom);
}

void NodeGraphEditorPanel::zoomAt(const ImVec2& screen, float factor) {
    ImVec2 wm = worldOfScreen(screen.x, screen.y);
    zoom = std::clamp(zoom * factor, 0.2f, 4.0f);
    canvasOrigin = ImVec2(screen.x - wm.x * zoom, screen.y - wm.y * zoom);
}

void NodeGraphEditorPanel::fitView() {
    if (nodes.empty()) {
        canvasOrigin = ImVec2(48, 48);
        zoom = 1.0f;
        return;
    }
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto& n : nodes) {
        minX = std::min(minX, n.x);
        minY = std::min(minY, n.y);
        maxX = std::max(maxX, n.x);
        maxY = std::max(maxY, n.y);
    }
    float bw = (maxX - minX) + 220.0f;
    float bh = (maxY - minY) + 140.0f;
    float cw = canvasRectMax.x - canvasRectMin.x;
    float ch = canvasRectMax.y - canvasRectMin.y;
    if (cw < 1 || ch < 1) return;
    float z = std::min(cw / bw, ch / bh);
    zoom = std::clamp(z, 0.2f, 2.0f);
    ImVec2 center((canvasRectMin.x + canvasRectMax.x) * 0.5f,
                  (canvasRectMin.y + canvasRectMax.y) * 0.5f);
    float wcx = (minX + maxX) * 0.5f;
    float wcy = (minY + maxY) * 0.5f;
    canvasOrigin = ImVec2(center.x - wcx * zoom, center.y - wcy * zoom);
}

NodeGraphEditorPanel::NodeLayout NodeGraphEditorPanel::computeLayout(const GNode& n) const {
    const float headerH = 26.0f;
    const float bodyPad = 10.0f;
    const float rowStride = 26.0f;
    float width = n.type == "output" ? 200.0f : 190.0f;

    auto sock = socketsFor(n.type, n.params);
    int nIn = (int)sock.first.size();
    int nOut = (int)sock.second.size();
    int maxRows = std::max(nIn, nOut);

    int pRows = 0;
    if (const NodeInfo* info = nodeInfo(n.type)) {
        if (n.type == "ramp") {
            int stops = n.params.contains("stops") && n.params["stops"].is_array()
                            ? (int)n.params["stops"].size() : 0;
            pRows = stops + 1;
        } else {
            pRows = (int)info->params.size();
        }
    }

    NodeLayout L;
    ImVec2 tl = screenOfWorld(n.x, n.y);
    float startY = tl.y + headerH + bodyPad;
    float bodyTop = startY + maxRows * rowStride;
    float height = (bodyTop - tl.y) + pRows * 24.0f + 12.0f;

    L.min = tl;
    L.max = ImVec2(tl.x + width, tl.y + height);
    L.headerMin = tl;
    L.headerMax = ImVec2(tl.x + width, tl.y + headerH);
    L.bodyTop = bodyTop;
    for (int i = 0; i < nIn; i++) L.inPts.push_back(ImVec2(tl.x, startY + i * rowStride));
    for (int i = 0; i < nOut; i++) L.outPts.push_back(ImVec2(tl.x + width, startY + i * rowStride));
    return L;
}

int NodeGraphEditorPanel::hitTestNode(const ImVec2& pos) const {
    // De ATRÁS hacia delante: los nodos se dibujan en orden, así que el último es el que está
    // ENCIMA. Recorriendo hacia delante, un nodo tapado se llevaba el clic y parecía que el de
    // arriba no respondía.
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        NodeLayout L = computeLayout(*it);
        if (pos.x >= L.min.x && pos.x <= L.max.x && pos.y >= L.min.y && pos.y <= L.max.y) return it->id;
    }
    return -1;
}

void NodeGraphEditorPanel::bringNodeToFront(int nodeId) {
    auto it = std::find_if(nodes.begin(), nodes.end(),
                           [&](const GNode& n) { return n.id == nodeId; });
    if (it == nodes.end() || it + 1 == nodes.end()) return;
    GNode moved = std::move(*it);
    nodes.erase(it);
    nodes.push_back(std::move(moved));   // los ids son estables: conexiones y findNode no se enteran
}

void NodeGraphEditorPanel::drawWire(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                                    ImU32 col, float th) {
    float dx = std::fabs(b.x - a.x) * 0.5f;
    dx = std::clamp(dx, 40.0f, 140.0f);
    ImVec2 c1(a.x + dx, a.y), c2(b.x - dx, b.y);
    dl->AddBezierCubic(a, c1, c2, b, col, th, 32);
}

void NodeGraphEditorPanel::drawConnections(ImDrawList* dl) {
    for (const auto& c : connections) {
        const GNode* from = findNode(c.from);
        const GNode* to = findNode(c.to);
        if (!from || !to) continue;
        NodeLayout Lf = computeLayout(*from);
        NodeLayout Lt = computeLayout(*to);
        if (c.fromOut < 0 || c.fromOut >= (int)Lf.outPts.size()) continue;
        if (c.toIn < 0 || c.toIn >= (int)Lt.inPts.size()) continue;
        ImU32 col = outputKind(from->type, c.fromOut) == 1
                        ? IM_COL32(190, 120, 255, 255)
                        : IM_COL32(120, 200, 255, 255);
        drawWire(dl, Lf.outPts[c.fromOut], Lt.inPts[c.toIn], col, 2.0f);
    }
}

void NodeGraphEditorPanel::renderToolbar() {
    ImGui::TextUnformatted(selectedObjectName.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("##previewSlot", slotByIndex(slotIndex(previewSlot)))) {
        for (int i = 0; i < 5; i++) {
            if (ImGui::Selectable(slotByIndex(i), slotIndex(previewSlot) == i)) {
                previewSlot = slotByIndex(i);
                previewDirty = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Preview");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    std::string sz = std::to_string(bakeSize);
    if (ImGui::BeginCombo("##bakeSize", sz.c_str())) {
        const int sizes[] = {64, 128, 256, 512, 1024};
        for (int s : sizes) {
            if (ImGui::Selectable(std::to_string(s).c_str(), bakeSize == s)) {
                bakeSize = s;
                previewDirty = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Size");

    ImGui::SameLine();
    if (ImGui::Checkbox("Normal from height", &normalFromHeight)) previewDirty = true;

    ImGui::SameLine();
    if (ImGui::Button("Bake Textures")) bakeTextures();

    ImGui::SameLine();
    if (ImGui::Button("Add")) ImGui::OpenPopup("add_node_menu");

    ImGui::SameLine();
    if (ImGui::Button("Fit")) fitView();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        nodes.clear();
        connections.clear();
        nextNodeId = 1;
        selectedNodeId = -1;
        markDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Default")) {
        createDefaultGraph();
        markDirty();
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::InputInt("Seed", &treeSeed);
    ImGui::SameLine();
    if (ImGui::Button("Árbol (corteza)")) createTreeGraph(false);
    ImGui::SameLine();
    if (ImGui::Button("Árbol (copa)")) createTreeGraph(true);

    if (!previewStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.5f, 0.2f, 1), "%s", previewStatus.c_str());
    }
}

void NodeGraphEditorPanel::renderAddMenu() {
    std::string lastCat;
    for (const auto& d : nodeCatalog()) {
        if (d.type == "output" && outputNodeId() >= 0) continue;
        if (d.category != lastCat) {
            if (!lastCat.empty()) ImGui::Separator();
            lastCat = d.category;
            ImGui::TextUnformatted(d.category.c_str());
        }
        if (ImGui::MenuItem(d.label.c_str())) {
            addNodeRaw(d.type, addPosWorld.x, addPosWorld.y);
            markDirty();
        }
    }
}

void NodeGraphEditorPanel::renderPopups() {
    if (ImGui::BeginPopup("node_ctx")) {
        const GNode* n = findNode(ctxNode);
        if (n) {
            if (const NodeInfo* info = nodeInfo(n->type)) ImGui::TextUnformatted(info->label.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate")) duplicateNode(ctxNode);
            if (ImGui::MenuItem("Delete")) removeNode(ctxNode);
            if (ImGui::MenuItem("Disconnect All")) {
                connections.erase(std::remove_if(connections.begin(), connections.end(),
                    [&](const GConnection& c) { return c.from == ctxNode || c.to == ctxNode; }),
                    connections.end());
                markDirty();
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("canvas_ctx")) {
        renderAddMenu();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("add_node_menu")) {
        renderAddMenu();
        ImGui::EndPopup();
    }
}

void NodeGraphEditorPanel::renderCanvas() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float previewH = (previewGL != 0 || !previewStatus.empty()) ? 150.0f : 0.0f;
    float canvasH = avail.y - previewH - 8.0f;
    if (canvasH < 60.0f) canvasH = 60.0f;

    ImGui::BeginChild("##graph_canvas", ImVec2(avail.x, canvasH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    canvasRectMin = origin;
    canvasRectMax = ImVec2(origin.x + size.x, origin.y + size.y);
    addPosWorld = worldOfScreen(canvasRectMin.x + (canvasRectMax.x - canvasRectMin.x) * 0.35f,
                                canvasRectMin.y + (canvasRectMax.y - canvasRectMin.y) * 0.3f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(canvasRectMin, canvasRectMax, IM_COL32(20, 21, 24, 255));
    // Se rellenan con los IsItemHovered/IsItemClicked de los botones de socket de renderNode.
    // nodeId = -1 (no 0): `0 >= 0` sería cierto y arrancaría un cable en cada frame sin clic.
    hoveredSocket  = SocketHit{-1, false, -1};
    clickedSocket  = SocketHit{-1, false, -1};

    // Rejilla
    float stepWorld = 1.0f;
    float gridStep = stepWorld * zoom;
    while (gridStep < 12.0f) { stepWorld *= 5.0f; gridStep = stepWorld * zoom; }
    ImVec2 wTL = worldOfScreen(canvasRectMin.x, canvasRectMin.y);
    ImVec2 wBR = worldOfScreen(canvasRectMax.x, canvasRectMax.y);
    ImU32 gridCol = IM_COL32(58, 60, 68, 140);
    for (float wx = std::floor(wTL.x / stepWorld) * stepWorld; wx <= wBR.x; wx += stepWorld) {
        float sx = screenOfWorld(wx, 0).x;
        dl->AddLine(ImVec2(sx, canvasRectMin.y), ImVec2(sx, canvasRectMax.y), gridCol);
    }
    for (float wy = std::floor(wTL.y / stepWorld) * stepWorld; wy <= wBR.y; wy += stepWorld) {
        float sy = screenOfWorld(0, wy).y;
        dl->AddLine(ImVec2(canvasRectMin.x, sy), ImVec2(canvasRectMax.x, sy), gridCol);
    }

    drawConnections(dl);

    for (auto& n : nodes) renderNode(dl, n);

    // Cable temporal mientras se arrastra
    if (draggingWire) {
        const GNode* from = findNode(dragWireFrom);
        if (from) {
            NodeLayout L = computeLayout(*from);
            if (dragWireOut >= 0 && dragWireOut < (int)L.outPts.size()) {
                drawWire(dl, L.outPts[dragWireOut], ImGui::GetMousePos(),
                         IM_COL32(200, 220, 255, 220), 2.5f);
            }
        }
    }

    handleCanvasInput();
    ImGui::EndChild();
}

void NodeGraphEditorPanel::renderNode(ImDrawList* dl, GNode& n) {
    const NodeInfo* info = nodeInfo(n.type);

    // Completar params con los valores por defecto que falten.
    nlohmann::json def = defaultParams(n.type);
    for (auto it = def.begin(); it != def.end(); ++it) {
        if (!n.params.contains(it.key())) n.params[it.key()] = it.value();
    }

    NodeLayout L = computeLayout(n);
    bool selected = n.id == selectedNodeId;
    std::string title = info ? info->label : n.type;
    ImU32 titleCol = categoryColor(info ? info->category : "");
    dl->AddRectFilled(L.min, L.max, IM_COL32(38, 40, 46, 240), 6.0f);
    dl->AddRect(L.min, L.max, selected ? IM_COL32(255, 190, 80, 255) : IM_COL32(70, 72, 80, 255), 6.0f);
    dl->AddRectFilled(L.headerMin, L.headerMax, titleCol, 4.0f);
    dl->AddText(ImVec2(L.headerMin.x + 8, L.headerMin.y + 4), IM_COL32(255, 255, 255, 255), title.c_str());

    // Cuerpo del nodo: InvisibleButton del tamaño del nodo, registrado ANTES que los sockets y los
    // widgets de parámetros. ImGui da el clic al ÚLTIMO widget registrado que contiene el ratón,
    // así los sockets y DragFloats conservan prioridad y el botón solo captura el arrastre en la
    // zona vacía del nodo. Antes el arrastre dependía de `!ImGui::IsAnyItemActive()` (global): un
    // widget activo en cualquier panel del editor congelaba los nodos, y los DragFloats del propio
    // nodo interceptaban el clic al cuerpo.
    ImGui::PushID(("body" + std::to_string(n.id)).c_str());
    ImGui::SetCursorScreenPos(L.min);
    ImGui::InvisibleButton("##node", ImVec2(L.max.x - L.min.x, L.max.y - L.min.y));
    if (ImGui::IsItemActivated()) {
        clickedBodyNode = n.id;
        ImVec2 wm = worldOfScreen(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
        dragGrabWorld = ImVec2(n.x - wm.x, n.y - wm.y);
    }
    if (ImGui::IsItemActive()) activeBodyNode = n.id;
    ImGui::PopID();

    auto sock = socketsFor(n.type, n.params);
    for (int i = 0; i < (int)L.inPts.size() && i < (int)sock.first.size(); i++) {
        bool hov = hoveredSocket.nodeId == n.id && hoveredSocket.input && hoveredSocket.index == i;
        ImU32 col = sock.first[i].second == 2 ? IM_COL32(255, 200, 80, 255)
                 : sock.first[i].second == 1 ? IM_COL32(190, 120, 255, 255)
                                             : IM_COL32(120, 200, 255, 255);
        dl->AddCircleFilled(L.inPts[i], hov ? 6.0f : 4.5f, col);
        dl->AddCircle(L.inPts[i], hov ? 6.0f : 4.5f, IM_COL32(0, 0, 0, 120), 12, 1.0f);
        dl->AddText(ImVec2(L.inPts[i].x + 10, L.inPts[i].y - 7), IM_COL32(210, 210, 215, 255),
                    sock.first[i].first.c_str());
        ImGui::PushID(("in" + std::to_string(n.id) + "_" + std::to_string(i)).c_str());
        ImGui::SetCursorScreenPos(ImVec2(L.inPts[i].x - 9, L.inPts[i].y - 9));
        ImGui::InvisibleButton("##sock", ImVec2(18, 18));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) clickedSocket = {n.id, true, i};
        if (ImGui::IsItemHovered()) hoveredSocket = {n.id, true, i};
        ImGui::PopID();
    }
    for (int i = 0; i < (int)L.outPts.size() && i < (int)sock.second.size(); i++) {
        bool hov = hoveredSocket.nodeId == n.id && !hoveredSocket.input && hoveredSocket.index == i;
        ImU32 col = sock.second[i].second == 2 ? IM_COL32(255, 200, 80, 255)
                 : sock.second[i].second == 1 ? IM_COL32(190, 120, 255, 255)
                                              : IM_COL32(120, 200, 255, 255);
        dl->AddCircleFilled(L.outPts[i], hov ? 6.0f : 4.5f, col);
        dl->AddCircle(L.outPts[i], hov ? 6.0f : 4.5f, IM_COL32(0, 0, 0, 120), 12, 1.0f);
        float tw = ImGui::CalcTextSize(sock.second[i].first.c_str()).x;
        dl->AddText(ImVec2(L.outPts[i].x - 10 - tw, L.outPts[i].y - 7), IM_COL32(210, 210, 215, 255),
                    sock.second[i].first.c_str());
        ImGui::PushID(("out" + std::to_string(n.id) + "_" + std::to_string(i)).c_str());
        ImGui::SetCursorScreenPos(ImVec2(L.outPts[i].x - 9, L.outPts[i].y - 9));
        ImGui::InvisibleButton("##sock", ImVec2(18, 18));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) clickedSocket = {n.id, false, i};
        if (ImGui::IsItemHovered()) hoveredSocket = {n.id, false, i};
        ImGui::PopID();
    }

    if (info && !info->params.empty()) {
        float py = L.bodyTop;
        float x = L.min.x + 8;
        float iw = L.max.x - L.min.x - 18;
        int pid = n.id * 131 + 7;
        bool changed = false;
        for (const auto& p : info->params) {
            if (p.kind == P_RAMP) {
                if (!n.params.contains("stops") || !n.params["stops"].is_array()) {
                    n.params["stops"] = nlohmann::json::array({nlohmann::json::array({0.0, 0.0}),
                                                               nlohmann::json::array({1.0, 1.0})});
                }
                auto& stops = n.params["stops"];
                int idx = 0;
                while (idx < (int)stops.size()) {
                    auto& s = stops[idx];
                    if (!s.is_array() || s.size() < 2) {
                        stops.erase(stops.begin() + idx);
                        changed = true;
                        continue;
                    }
                    float pos = (float)s[0];
                    float val = (float)s[1];
                    ImGui::PushID(pid++);
                    ImGui::SetCursorScreenPos(ImVec2(x, py));
                    ImGui::SetNextItemWidth(iw * 0.42f);
                    if (ImGui::DragFloat("##p", &pos, 0.005f, 0.0f, 1.0f, "%.2f")) { s[0] = pos; changed = true; }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(iw * 0.42f);
                    if (ImGui::DragFloat("##v", &val, 0.005f, 0.0f, 1.0f, "%.2f")) { s[1] = val; changed = true; }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("-")) {
                        stops.erase(stops.begin() + idx);
                        changed = true;
                        ImGui::PopID();
                        continue;
                    }
                    ImGui::PopID();
                    idx++;
                    py += 24;
                }
                ImGui::PushID(pid++);
                ImGui::SetCursorScreenPos(ImVec2(x, py));
                if (ImGui::SmallButton("+ stop")) {
                    stops.push_back(nlohmann::json::array({0.5f, 0.5f}));
                    changed = true;
                }
                ImGui::PopID();
                py += 24;
                continue;
            }

            ImGui::PushID(pid++);
            ImGui::SetCursorScreenPos(ImVec2(x, py + 2));
            ImGui::TextUnformatted(p.label.c_str());
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(ImVec2(L.min.x + 84, py + 2));
            ImGui::SetNextItemWidth(L.max.x - L.min.x - 96);
            if (p.kind == P_FLOAT) {
                float v = jfloat(n.params, p.key, p.def);
                if (ImGui::DragFloat("##v", &v, 0.01f, p.min, p.max, "%.3f")) { n.params[p.key] = v; changed = true; }
            } else if (p.kind == P_INT) {
                int v = jint(n.params, p.key, (int)p.def);
                if (ImGui::DragInt("##v", &v, 1, (int)p.min, (int)p.max)) { n.params[p.key] = v; changed = true; }
            } else if (p.kind == P_COMBO) {
                int v = jint(n.params, p.key, (int)p.def);
                std::string label2 = (v >= 0 && v < (int)p.options.size()) ? p.options[v] : "";
                if (ImGui::BeginCombo("##v", label2.c_str())) {
                    for (int oi = 0; oi < (int)p.options.size(); oi++) {
                        if (ImGui::Selectable(p.options[oi].c_str(), oi == v)) {
                            n.params[p.key] = oi;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            } else if (p.kind == P_BOOL) {
                bool v = jbool(n.params, p.key, false);
                if (ImGui::Checkbox("##v", &v)) { n.params[p.key] = v; changed = true; }
            }
            ImGui::PopID();
            py += 24;
        }
        if (changed) markDirty();
    }
}

void NodeGraphEditorPanel::handleCanvasInput() {
    ImGuiIO& io = ImGui::GetIO();
    bool hovered = ImGui::IsWindowHovered();
    ImVec2 mouse = io.MousePos;

    if (hovered && !ImGui::IsAnyItemActive() && io.MouseWheel != 0.0f) {
        zoomAt(mouse, 1.0f + io.MouseWheel * 0.12f);
    }
    if (hovered && !ImGui::IsAnyItemActive() && io.MouseDown[ImGuiMouseButton_Middle]) {
        canvasOrigin.x += io.MouseDelta.x;
        canvasOrigin.y += io.MouseDelta.y;
    }

    int hoverNode = hitTestNode(mouse);

    // El hover/clic de sockets y del cuerpo de los nodos lo reportan los InvisibleButtons de
    // renderNode (registrados encima del dibujo, con prioridad de clic). Aquí se consumen. Ya no
    // se depende de `!ImGui::IsAnyItemActive()`: el arrastre lo controla el estado del botón del
    // propio nodo, no widgets de otros paneles.

    // Clic en un socket de salida → arrastrar un cable nuevo.
    if (clickedSocket.nodeId >= 0 && !clickedSocket.input) {
        draggingWire = true;
        dragWireFrom = clickedSocket.nodeId;
        dragWireOut = clickedSocket.index;
    }
    bool clickedAnySocket = clickedSocket.nodeId >= 0;
    clickedSocket = SocketHit{-1, false, -1};

    // Clic en el cuerpo de un nodo → seleccionar, traer al frente y preparar el arrastre.
    if (clickedBodyNode >= 0) {
        selectedNodeId = clickedBodyNode;
        bringNodeToFront(clickedBodyNode);
        draggingNodeId = clickedBodyNode;
    } else if (!clickedAnySocket && io.MouseClicked[ImGuiMouseButton_Left] && hoverNode < 0) {
        selectedNodeId = -1;
    }
    clickedBodyNode = -1;

    if (activeBodyNode >= 0 && io.MouseDown[ImGuiMouseButton_Left]) {
        GNode* n = findNode(activeBodyNode);
        if (n) {
            ImVec2 wm = worldOfScreen(mouse.x, mouse.y);
            n->x = wm.x + dragGrabWorld.x;
            n->y = wm.y + dragGrabWorld.y;
        }
    }

    if (io.MouseReleased[ImGuiMouseButton_Left]) {
        // Al SOLTAR, no por frame: la posición del nodo se serializa en `materialGraph` y
        // guardarla 60 veces por segundo reescribiría el grafo entero durante todo el arrastre.
        // Y con `syncToObject`, no con `markDirty`: mover un nodo cambia dónde SE VE, no lo que
        // el grafo CALCULA — invalidar la preview obligaría a reevaluar 256² por cada arrastre.
        if (draggingNodeId >= 0) {
            syncToObject();
            if (onSceneChanged) onSceneChanged();
        }
        draggingNodeId = -1;
        activeBodyNode = -1;
        if (draggingWire) {
            draggingWire = false;
            if (hoveredSocket.nodeId >= 0 && hoveredSocket.input) {
                GNode* from = findNode(dragWireFrom);
                GNode* to = findNode(hoveredSocket.nodeId);
                if (from && to && from->id != to->id && from->type != "output") {
                    connections.erase(std::remove_if(connections.begin(), connections.end(),
                        [&](const GConnection& c) {
                            return c.to == to->id && c.toIn == hoveredSocket.index;
                        }), connections.end());
                    connections.push_back({from->id, dragWireOut, to->id, hoveredSocket.index});
                    markDirty();
                }
            }
        }
    }

    if (hovered && io.MouseClicked[ImGuiMouseButton_Right]) {
        int h = hitTestNode(mouse);
        if (h >= 0) {
            ctxNode = h;
            ImGui::OpenPopup("node_ctx");
        } else {
            ctxNode = -1;
            addPosWorld = worldOfScreen(mouse.x, mouse.y);
            ImGui::OpenPopup("canvas_ctx");
        }
    }

    if (selectedNodeId >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        removeNode(selectedNodeId);
    }
}

void NodeGraphEditorPanel::renderPreview() {
    if (previewDirty) updatePreview();
    if (previewGL == 0 && previewStatus.empty()) return;

    float h = 150.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float iw = (float)previewWidth;
    float ih = (float)previewHeight;
    float dispH = h - 26.0f;
    if (ih > 0) {
        float dispW = dispH * (iw / ih);
        if (dispW > avail.x * 0.4f) {
            dispW = avail.x * 0.4f;
            dispH = dispW * (ih / iw);
        }
        if (previewGL) {
            ImGui::Image((ImTextureID)(intptr_t)previewGL, ImVec2(dispW, dispH));
        }
    }
    // ESFERA con el material montado, al lado del mapa del slot. El mapa dice cómo es una textura;
    // la esfera dice cómo QUEDA — un normal map en plano es un borrón azul y solo se juzga cuando
    // le da la luz. La pinta el motor con `final.frag`, el mismo shader del viewport, así que no es
    // una aproximación del editor. Muestra el material del objeto: tras un Bake, el resultado.
    if (selectedObject && selectedObject->material) {
        if (Application* app = MotorInstance::getInstance().getApplication()) {
            const int kSphere = 128;
            if (!materialPreviewTarget)
                materialPreviewTarget = std::make_unique<Haruka::Renderer::RenderTarget>(kSphere, kSphere);
            app->renderMaterialPreview(*selectedObject->material, *materialPreviewTarget);
            ImGui::SameLine();
            // V invertida: el origen de un FBO de GL está abajo (ver la nota del inspector).
            ImGui::Image((ImTextureID)(intptr_t)materialPreviewTarget->getColorTextureGL(),
                         ImVec2(dispH, dispH), ImVec2(0, 1), ImVec2(1, 0));
        }
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(("Preview: " + previewSlot).c_str());
    ImGui::TextUnformatted(("Size: " + std::to_string(previewWidth) + "x" + std::to_string(previewHeight)).c_str());
    if (selectedObject && selectedObject->material) {
        int n = 0;
        for (const auto& [slot, path] : selectedObject->material->textures)
            if (!path.empty()) ++n;
        ImGui::TextDisabled("Material: %d/5 slots (Bake para actualizarlo)", n);
    }
    if (!previewStatus.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.2f, 1), "%s", previewStatus.c_str());
    }
    ImGui::EndGroup();
}

void NodeGraphEditorPanel::onImGuiRender() {
    // Revalidar el objeto seleccionado (puede haberse borrado/renombrado).
    if (currentScene && selectedObject) {
        auto sp = currentScene->getObject(selectedObjectName);
        if (!sp || sp.get() != selectedObject) {
            selectedObject = nullptr;
            selectedObjectName.clear();
            nodes.clear();
            connections.clear();
            selectedNodeId = -1;
            draggingNodeId = -1;
            draggingWire = false;
            if (previewTex.id) {
                if (auto* d = Haruka::RHI::device()) d->destroy(previewTex);
                previewTex = {};
            }
            previewGL = 0;
            previewStatus.clear();
        }
    }

    ImGui::SetNextWindowSize(ImVec2(860, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Node Graph Editor")) {
        ImGui::End();
        return;
    }

    if (!selectedObject) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Select an object to edit its material graph");
        ImGui::End();
        return;
    }

    renderToolbar();
    ImGui::Separator();
    renderCanvas();
    renderPreview();
    renderPopups();

    ImGui::End();
}

// ===========================================================================
// Evaluación y bake
// ===========================================================================
bool NodeGraphEditorPanel::buildEngineGraph(Graph& g,
                                            std::map<int, int>& nodeToEngine,
                                            std::vector<SlotLink>& links) {
    nodeToEngine.clear();
    links.clear();
    for (auto& n : nodes) {
        if (n.type == "output") continue;
        std::unique_ptr<Node> en = engineNode(n.type, n.params);
        if (!en) continue;
        int idx = g.addNode(std::move(en));
        nodeToEngine[n.id] = idx;
    }
    for (const auto& c : connections) {
        auto itFrom = nodeToEngine.find(c.from);
        if (itFrom == nodeToEngine.end()) continue;
        auto itTo = nodeToEngine.find(c.to);
        if (itTo == nodeToEngine.end()) {
            // El destino es el Material Output → se registra el link de slot.
            if (c.toIn >= 0 && c.toIn < 5) {
                SlotLink l;
                l.fromEngine = itFrom->second;
                l.fromOut = c.fromOut;
                l.toIn = c.toIn;
                const GNode* fn = findNode(c.from);
                l.kind = fn ? outputKind(fn->type, c.fromOut) : 0;
                links.push_back(l);
            }
        } else {
            g.connect(itFrom->second, c.fromOut, itTo->second, c.toIn);
        }
    }
    return g.compile();
}

void NodeGraphEditorPanel::uploadPreview(const Haruka::Tools::ProcGraph::RGBAImage& img,
                                         int w, int h) {
    previewWidth = w;
    previewHeight = h;
    Haruka::RHI::Device* dev = Haruka::RHI::device();
    if (!dev) {
        previewStatus = "RHI device not ready";
        return;
    }
    previewTex = Haruka::Tools::ProcGraph::createRHIFromRGBA(img);
    if (previewTex.id) {
        previewGL = dev->nativeTexture(previewTex);
    } else {
        previewStatus = "Texture upload failed";
    }
}

void NodeGraphEditorPanel::updatePreview() {
    previewDirty = false;
    previewStatus.clear();
    if (previewTex.id) {
        if (auto* d = Haruka::RHI::device()) d->destroy(previewTex);
        previewTex = {};
    }
    previewGL = 0;
    previewWidth = previewHeight = 0;
    if (!selectedObject) return;

    Graph g;
    std::map<int, int> nodeToEngine;
    std::vector<SlotLink> links;
    if (!buildEngineGraph(g, nodeToEngine, links)) {
        previewStatus = "Graph has cycles or no nodes";
        return;
    }
    int outId = outputNodeId();
    if (outId < 0) {
        previewStatus = "Add a Material Output node";
        return;
    }
    int targetIn = slotIndex(previewSlot);
    const SlotLink* link = nullptr;
    for (const auto& l : links) {
        if (l.toIn == targetIn) { link = &l; break; }
    }
    if (!link) return; // el slot no está conectado

    int size = std::min(bakeSize, 256);
    if (previewSlot == "normal" && normalFromHeight) {
        auto img = Haruka::Tools::ProcGraph::evaluateToNormalMap(
            g, link->fromEngine, link->fromOut, size, size, 0, 0, 1, 1.0f);
        uploadPreview(img, size, size);
    } else {
        int cmap[4] = {0, 0, 0, 0};
        if (link->kind == 1) { cmap[0] = 0; cmap[1] = 1; cmap[2] = 2; cmap[3] = 3; }
        auto img = Haruka::Tools::ProcGraph::evaluateToRGBA(
            g, link->fromEngine, link->fromOut, size, size, 0, 0, 1, cmap);
        uploadPreview(img, size, size);
    }
}

void NodeGraphEditorPanel::bakeTextures() {
    previewStatus.clear();
    if (!selectedObject) return;
    if (!selectedObject->material) {
        selectedObject->material = std::make_shared<Haruka::MaterialComponent>();
        selectedObject->material->name = selectedObject->name + "_Material";
    }

    Graph g;
    std::map<int, int> nodeToEngine;
    std::vector<SlotLink> links;
    if (!buildEngineGraph(g, nodeToEngine, links)) {
        previewStatus = "Bake failed: graph has cycles";
        return;
    }
    if (links.empty()) {
        previewStatus = "Bake: connect something to Material Output first";
        return;
    }

    std::string base = sanitizeName(selectedObject->name);
    fs::path dir = projectPath.empty() ? fs::path("assets/textures")
                                       : fs::path(projectPath) / "assets" / "textures";
    std::error_code ec;
    fs::create_directories(dir, ec);

    bool any = false;
    for (const auto& l : links) {
        const char* slot = slotByIndex(l.toIn);
        Haruka::Tools::ProcGraph::RGBAImage img;
        if (std::string(slot) == "normal" && normalFromHeight) {
            img = Haruka::Tools::ProcGraph::evaluateToNormalMap(
                g, l.fromEngine, l.fromOut, bakeSize, bakeSize, 0, 0, 1, 1.0f);
        } else {
            int cmap[4] = {0, 0, 0, 0};
            if (l.kind == 1) { cmap[0] = 0; cmap[1] = 1; cmap[2] = 2; cmap[3] = 3; }
            img = Haruka::Tools::ProcGraph::evaluateToRGBA(
                g, l.fromEngine, l.fromOut, bakeSize, bakeSize, 0, 0, 1, cmap);
        }
        std::string fname = base + "_" + slot + ".png";
        std::string abs = (dir / fname).string();
        if (Haruka::writePNG(abs, bakeSize, bakeSize, 4, img.data())) {
            selectedObject->material->textures[slot] = "assets/textures/" + fname;
            any = true;
        } else {
            previewStatus = "Bake failed to write " + abs;
        }
    }
    if (any) {
        markDirty();
        if (previewStatus.empty()) previewStatus = "Baked textures to assets/textures/";
    }
}
