// Editor — anfitrión del panel de prefabricados del motor. Ver prefab_panel.h.
#include "panels/prefab_panel.h"
#include "panels/viewport.h"

#include "Haruka/Core/Scene/SceneManager.h"
#include "Haruka/Core/Camera.h"
#include "Haruka/Renderer/MotorInstance.h"
#include "Haruka/Core/Application.h"
#include "Haruka/Core/Components/MeshRendererComponent.h"
#include "Haruka/Core/Components/MaterialComponent.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
/// Gris neutro. El color por MATERIAL es cosa del juego (`materialColor`, que lee materials.json);
/// el editor no lo tiene, e inventarse una segunda paleta sería la divergencia que `preview_render`
/// vino a matar. Aquí se dibuja la FORMA, que es lo que se está editando.
constexpr glm::vec3 kTint{0.78f, 0.76f, 0.72f};
} // namespace

void PrefabPanel::setProjectPath(const std::string& path) {
    if (path == m_projectPath) return;
    releaseGizmo();
    m_projectPath = path;
    m_items.clear();
    m_itemsLoaded = false;
    panel.setPrefabDir(path.empty() ? std::string() : path + "/assets/data/prefabs");
    panel.setGeoProvider([this](const std::string& id) { return geoOf(id); });
}

void PrefabPanel::releaseGizmo() {
    if (viewport) viewport->setPrefabEdit(nullptr, glm::dvec3(0.0));
}

void PrefabPanel::loadItemIndex() {
    m_itemsLoaded = true;                       // una sola vez, aunque no haya nada que leer
    m_items.clear();
    if (m_projectPath.empty()) return;
    const std::string dir = m_projectPath + "/assets/data/items";
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        std::ifstream f(e.path());
        if (!f) continue;
        nlohmann::json j;
        try { f >> j; } catch (const std::exception&) { continue; }
        if (!j.contains("items") || !j["items"].is_array()) continue;
        for (const auto& it : j["items"]) {
            if (!it.is_object()) continue;
            const std::string id = it.value("id", std::string{});
            if (id.empty()) continue;
            ItemLook look;
            look.model = it.value("model", std::string{});
            if (it.contains("size") && it["size"].is_array() && it["size"].size() == 3)
                look.size = glm::vec3(it["size"][0].get<float>(),
                                      it["size"][1].get<float>(),
                                      it["size"][2].get<float>());
            m_items[id] = look;
        }
    }
}

Haruka::Tools::PreviewGeo PrefabPanel::geoOf(const std::string& itemId) const {
    Haruka::Tools::PreviewGeo g;
    auto it = m_items.find(itemId);
    if (it != m_items.end() && !it->second.model.empty()) {
        g = Haruka::Tools::previewModelGeo(it->second.model, kTint);
        if (!g.empty()) return g;               // si el .glb no resuelve, cae a la caja
    }
    // La caja no es un relleno perezoso: la mayoría de las piezas de un montaje (tablones, planchas)
    // SON cajas, y su tamaño real es lo que hace que el conjunto se lea.
    const glm::vec3 size = (it != m_items.end()) ? it->second.size : glm::vec3(0.5f);
    Haruka::Tools::previewAddBox(g, size * 0.5f, kTint);
    return g;
}

void PrefabPanel::setScene(Haruka::Core::Scene::SceneManager* scene) {
    m_scene = scene;
    if (!scene) return;
    // El expansor es del ANFITRIÓN: el motor pone el dato (la entrada, el fichero, la pose) y aquí
    // se decide qué es una pieza en el editor — una malla que se ve y se mueve. El juego instala el
    // suyo, que además le da colisionador y física.
    scene->setPrefabExpander([this](Haruka::Core::Scene::SceneManager& sc, const Haruka::Core::Scene::SceneObject& e) {
        expandEntry(sc, e);
    });
    scene->expandPrefabs();   // una escena ya cargada antes de tener expansor
}

const Haruka::Game::Prefab::Prefab* PrefabPanel::basePrefab(const std::string& name) {
    auto it = m_baseCache.find(name);
    if (it != m_baseCache.end()) return &it->second;
    Haruka::Game::Prefab::Prefab pf;
    if (!Haruka::Game::Prefab::loadPrefab(Haruka::Game::Prefab::findPrefabFile(m_projectPath + "/assets/data/prefabs", name), pf))
        return nullptr;
    return &(m_baseCache[name] = std::move(pf));
}

void PrefabPanel::expandEntry(Haruka::Core::Scene::SceneManager& scene, const Haruka::Core::Scene::SceneObject& entry) {
    if (!m_itemsLoaded) loadItemIndex();
    const Haruka::Game::Prefab::Prefab* base = basePrefab(entry.prefabName);
    if (!base) {
        // Sin esto, "no se ve" y "el fichero del montaje no está donde creo" son el mismo síntoma.
        std::cout << "[Prefab] NO se pudo cargar "
                  << m_projectPath << "/assets/data/prefabs/" << entry.prefabName << ".{json,glb,gltf}"
                  << " — la entrada queda vacía" << std::endl;
        return;
    }
    Haruka::Game::Prefab::Prefab pf = *base;
    // Lo que le haya pasado a ESTE conjunto (una tabla rota, una pieza movida) se aplica sobre la
    // copia, no sobre el fichero. La regla la pone el motor una sola vez: juego y editor no pueden
    // interpretar un cambio de dos maneras.
    // ⚠️ EL ÍNDICE QUE SE GUARDA ES EL DEL MONTAJE, NO EL DE LA COPIA. `applyPrefabEdits` BORRA las
    // piezas rotas, así que recorrer la copia daría índices corridos y el siguiente cambio se
    // anotaría sobre otra pieza. Se recorre el montaje ENTERO y se salta lo que el conjunto ha
    // perdido — así `i` significa siempre lo mismo, en la escena y en el fichero.
    std::vector<char> gone(base->pieces.size(), 0);
    for (const Haruka::Game::Prefab::PrefabEdit& e : entry.prefabEdits)
        if (e.removed && e.index >= 0 && e.index < (int)gone.size()) gone[e.index] = 1;
    Haruka::Game::Prefab::applyPrefabEdits(pf, entry.prefabEdits);
    // `pf` ya no sirve para recorrer (le faltan piezas): se usa sólo para leer las poses cambiadas.
    size_t k = 0;
    int puestas = 0;
    std::shared_ptr<Haruka::Core::Scene::SceneObject> primera;
    for (size_t i = 0; i < base->pieces.size(); ++i) {
        if (gone[i]) continue;
        const Haruka::Game::Prefab::PrefabPiece& pc = pf.pieces[k++];
        const Haruka::Tools::PreviewGeo g = geoOf(pc.id);
        if (g.empty()) continue;

        // La geometría viene INTERCALADA (pos3+nrm3+col3), que es lo que come el pipeline de las
        // miniaturas; `MeshRendererComponent` la quiere en tres vectores. Se desintercala aquí y no
        // se cambia `PreviewGeo`: su formato lo manda el pipeline que ya existe.
        std::vector<glm::vec3> verts, norms, cols;
        verts.reserve(g.data.size() / 9);
        norms.reserve(g.data.size() / 9);
        cols.reserve(g.data.size() / 9);
        for (size_t k = 0; k + 8 < g.data.size(); k += 9) {
            verts.emplace_back(g.data[k],   g.data[k+1], g.data[k+2]);
            norms.emplace_back(g.data[k+3], g.data[k+4], g.data[k+5]);
            cols .emplace_back(g.data[k+6], g.data[k+7], g.data[k+8]);
        }

        auto obj = std::make_shared<Haruka::Core::Scene::SceneObject>();
        obj->name = entry.name + "/" + std::to_string(i) + " " + pc.id;
        obj->type = "Mesh";
        obj->objectType = Haruka::Tools::classifyObjectType("Mesh");
        // ⚠️ LA MARCA. Sin esto la escena guardaría las 870 tablas y el prefabricado no habría
        // servido de nada: lo que se guarda es la ENTRADA, y estas piezas salen de ella.
        obj->fromPrefab = true;
        obj->prefabName = entry.prefabName;
        obj->prefabOwnerUid   = entry.uid;   // de QUÉ barco es esta tabla (puede haber tres)
        obj->prefabPieceIndex = (int)i;      // y cuál es en el MONTAJE

        // La pose de la pieza es LOCAL al montaje: se compone con la de la entrada.
        obj->position = entry.position + entry.orientation * pc.pos;
        obj->orientation = entry.orientation * pc.rot;
        obj->useOrientation = true;              // cuaternión tal cual: nada de pasar por Euler
        obj->scale = glm::dvec3(1.0);

        obj->meshRenderer = std::make_shared<Haruka::Core::Components::MeshRendererComponent>();
        obj->meshRenderer->setMesh(verts, norms, cols, g.idx);
        obj->material = std::make_shared<Haruka::Core::Components::MaterialComponent>();
        obj->material->name = obj->name + "_Material";
        obj->material->albedo = kTint;
        obj->color = glm::dvec3(kTint);

        scene.addLoadedObject(obj);

        // ── BAJO EL NOMBRE DEL MONTAJE, NO SUELTAS EN LA RAÍZ ───────────────────────────────────
        //
        // La jerarquía dibuja su árbol con `parentIndex`/`childrenIndices`, así que colocar un
        // tanque metía 40 objetos sueltos en la raíz y encontrar cualquier otra cosa se volvía
        // imposible. Colgadas de su entrada, un montaje es UNA línea que se despliega.
        //
        // ⚠️ NO TOCA LAS POSES, y hay que saberlo: el render del motor **no compone** el transform
        // del padre (`parentIndex` no lo lee nadie fuera del editor), así que las piezas siguen en
        // coordenadas de mundo y anidarlas no las mueve. El día que el motor herede transforms, esto
        // hay que revisarlo — las piezas pasarían a estar dos veces desplazadas.
        const auto& objs = scene.getAllObjects();
        const int myIdx = (int)objs.size() - 1;
        int entryIdx = -1;
        for (int k = 0; k < (int)objs.size(); ++k)
            if (objs[k] && objs[k]->uid == entry.uid) { entryIdx = k; break; }
        if (entryIdx >= 0 && myIdx >= 0) {
            objs[myIdx]->parentIndex = entryIdx;
            objs[entryIdx]->childrenIndices.push_back(myIdx);
        }
        ++puestas;
        if (puestas == 1) primera = obj;
    }

    // ── QUÉ SE HA PUESTO, DE VERDAD ─────────────────────────────────────────────────────────────
    // "No se ve" tiene media docena de causas con el mismo aspecto: no se creó ninguna pieza, se
    // crearon sin malla, se crearon lejísimos, o se crearon del tamaño equivocado. Esta línea las
    // separa sin tener que adivinar — y va a la consola del editor, que captura stdout.
    std::cout << "[Prefab] " << entry.prefabName << ": " << puestas << " piezas de "
              << base->pieces.size() << " · entrada en ("
              << entry.position.x << ", " << entry.position.y << ", " << entry.position.z << ")";
    if (primera) {
        std::cout << " · 1a pieza en (" << primera->position.x << ", " << primera->position.y
                  << ", " << primera->position.z << ") con "
                  << (primera->meshRenderer ? primera->meshRenderer->getResidentVertexCount() : -1)
                  << " vertices · residente "
                  << (primera->meshRenderer && primera->meshRenderer->isResident() ? "SI" : "NO");
    }
    std::cout << std::endl;
}

void PrefabPanel::syncEditsFromScene() {
    if (!m_scene) return;

    // Umbrales: por debajo de esto es ruido de coma flotante al componer poses, no una edición. Un
    // milímetro y ~0,06 grados — cualquier tirón de gizmo los pasa de sobra.
    constexpr double kPosEps = 1.0e-3;
    constexpr double kRotEps = 1.0e-6;   // sobre 1 - |dot(q1,q2)|

    const auto& objs = m_scene->getAllObjects();

    // ⚠️ SALIDA TEMPRANA: sin entradas de prefabricado esto no tiene nada que hacer, y se llama CADA
    // FRAME. Recorrer la escena entera y construir dos mapas para descubrir que no hay ningún montaje
    // colocado es trabajo puro por frame en la escena más común: la que no tiene prefabricados.
    bool hayEntradas = false;
    for (const auto& o : objs)
        if (o && !o->prefabName.empty() && !o->fromPrefab) { hayEntradas = true; break; }
    if (!hayEntradas) return;

    // Índice de piezas vivas por (dueño, índice de pieza). Lo que NO aparezca y el montaje sí tenga,
    // es una pieza que ya no está: se anota `removed`. Así "borrar el objeto en la jerarquía" es
    // exactamente "romper esa tabla", sin inventar un comando nuevo.
    std::unordered_map<uint64_t, std::unordered_map<int, const Haruka::Core::Scene::SceneObject*>> vivas;
    for (const auto& o : objs)
        if (o && o->fromPrefab && o->prefabOwnerUid && o->prefabPieceIndex >= 0)
            vivas[o->prefabOwnerUid][o->prefabPieceIndex] = o.get();

    bool changed = false;
    for (const auto& eo : objs) {
        if (!eo || eo->prefabName.empty() || eo->fromPrefab) continue;
        const Haruka::Game::Prefab::Prefab* base = basePrefab(eo->prefabName);
        if (!base) continue;
        Haruka::Core::Scene::SceneObject& entry = *eo;
        const glm::dquat entryRot = entry.useOrientation ? entry.orientation : glm::dquat(entry.rotation);
        const glm::dquat invRot   = glm::inverse(entryRot);
        auto& mias = vivas[entry.uid];

        // ── MOVER LA ENTRADA MUEVE EL MONTAJE ENTERO ────────────────────────────────────────────
        //
        // ⚠️ SIN ESTO, ARRASTRAR LA ENTRADA CON EL GIZMO ANOTABA UN CAMBIO POR PIEZA. Las piezas
        // están en coordenadas de MUNDO (el render del motor no compone el transform del padre), así
        // que al mover la entrada todas quedan "desviadas" respecto a su pose teórica y esta misma
        // pasada las escribiría como 870 ediciones — justo lo contrario de lo que el prefabricado
        // viene a hacer, y encima congelando el montaje en el fichero de escena.
        //
        // Así que la entrada se comporta como el ASA del conjunto: lo que se le haga se le hace a
        // sus piezas, y lo que quede después de eso es lo que de verdad se ha tocado pieza a pieza.
        {
            auto& last = m_lastEntryPose[entry.uid];
            if (!last.valid) { last.pos = entry.position; last.rot = entryRot; last.valid = true; }
            const glm::dvec3 dPos = glm::dvec3(entry.position) - last.pos;
            const glm::dquat dRot = entryRot * glm::inverse(last.rot);
            const bool movida = glm::length(dPos) > 1.0e-9 ||
                                1.0 - std::abs(glm::dot(entryRot, last.rot)) > 1.0e-12;
            if (movida) {
                for (auto& kv : mias) {
                    Haruka::Core::Scene::SceneObject& po = const_cast<Haruka::Core::Scene::SceneObject&>(*kv.second);
                    const glm::dvec3 rel = glm::dvec3(po.position) - last.pos;
                    po.position = last.pos + dPos + dRot * rel;   // gira alrededor de la entrada
                    const glm::dquat cur = po.useOrientation ? po.orientation : glm::dquat(po.rotation);
                    po.orientation = dRot * cur;
                    po.useOrientation = true;
                }
                last.pos = entry.position;
                last.rot = entryRot;
            }
        }

        // ⚠️ SI NO HAY NI UNA PIEZA, NO ES QUE ESTÉN TODAS ROTAS: ES QUE NO SE EXPANDIÓ.
        //
        // `expandPrefabs` marca la entrada como expandida ANTES de llamar al expansor, y el expansor
        // puede no poner nada (el `.json` del montaje no está porque el proyecto abierto es otro, o
        // el item no resuelve). Sin esta guarda, abrir una escena en el proyecto equivocado y
        // guardarla escribiría `removed` en LAS 870 PIEZAS — el montaje quedaría vacío para siempre
        // y sin un solo error por delante.
        if (mias.empty() && !base->pieces.empty()) continue;

        std::vector<Haruka::Game::Prefab::PrefabEdit> nuevos;
        for (size_t i = 0; i < base->pieces.size(); ++i) {
            const Haruka::Game::Prefab::PrefabPiece& pc = base->pieces[i];
            auto it = mias.find((int)i);
            if (it == mias.end()) {
                // No está en la escena. Si la entrada ya no se ha expandido nunca, no es que falte:
                // es que no se ha puesto. Sólo cuenta como rota si el conjunto SÍ está expandido.
                if (entry.prefabExpanded) {
                    Haruka::Game::Prefab::PrefabEdit e; e.index = (int)i; e.removed = true;
                    nuevos.push_back(e);
                }
                continue;
            }
            const Haruka::Core::Scene::SceneObject& po = *it->second;
            // La pose que le TOCARÍA si no la hubiera tocado nadie.
            const glm::dvec3 espPos = entry.position + entryRot * pc.pos;
            const glm::dquat espRot = entryRot * pc.rot;
            const glm::dquat curRot = po.useOrientation ? po.orientation : glm::dquat(po.rotation);

            Haruka::Game::Prefab::PrefabEdit e; e.index = (int)i;
            if (glm::length(glm::dvec3(po.position) - espPos) > kPosEps) {
                e.hasPos = true;
                // De vuelta al marco del MONTAJE: el fichero no lleva coordenadas de mundo.
                e.pos = invRot * (glm::dvec3(po.position) - entry.position);
            }
            if (1.0 - std::abs(glm::dot(curRot, espRot)) > kRotEps) {
                e.hasRot = true;
                e.rot = invRot * curRot;
            }
            if (e.hasPos || e.hasRot) nuevos.push_back(e);
        }

        // ⚠️ SE CONSERVAN LOS CAMBIOS QUE ESTA PASADA NO PUEDE VER. Una pieza SUSTITUIDA (`item`) no
        // deja rastro en la pose, así que recalcular la lista a pelo la borraría en silencio. Se
        // arrastran los `item` de los que ya había para los índices que siguen vivos.
        for (const Haruka::Game::Prefab::PrefabEdit& viejo : entry.prefabEdits) {
            if (viejo.item.empty()) continue;
            auto it = std::find_if(nuevos.begin(), nuevos.end(),
                                   [&](const Haruka::Game::Prefab::PrefabEdit& n) { return n.index == viejo.index; });
            if (it != nuevos.end()) it->item = viejo.item;
            else if (mias.count(viejo.index)) nuevos.push_back(viejo);
        }

        // ¿Ha cambiado algo de verdad? Si no, no se toca nada: marcar la escena como sucia cada
        // frame haría que el editor pidiera guardar sin que nadie hubiera hecho nada.
        auto igual = [](const Haruka::Game::Prefab::PrefabEdit& a, const Haruka::Game::Prefab::PrefabEdit& b) {
            return a.index == b.index && a.removed == b.removed && a.item == b.item
                && a.hasPos == b.hasPos && a.hasRot == b.hasRot
                && (!a.hasPos || glm::length(a.pos - b.pos) < 1.0e-9)
                && (!a.hasRot || 1.0 - std::abs(glm::dot(a.rot, b.rot)) < 1.0e-12);
        };
        bool distinto = nuevos.size() != entry.prefabEdits.size();
        for (size_t i = 0; !distinto && i < nuevos.size(); ++i)
            distinto = !igual(nuevos[i], entry.prefabEdits[i]);
        if (distinto) { entry.prefabEdits = std::move(nuevos); changed = true; }
    }

    if (changed && m_onChanged) m_onChanged();
}

void PrefabPanel::onImGuiRender() {
    if (!m_itemsLoaded) loadItemIndex();
    // Antes que nada: lo que se haya hecho a las piezas se anota en su entrada. Si se hiciera
    // después del panel, un "Colocar" del mismo frame vería una entrada a medio actualizar.
    syncEditsFromScene();
    panel.onImGuiRender();

    // ── COLOCAR: UNA ENTRADA, NO N OBJETOS ──────────────────────────────────────────────────────
    // La ventana del panel ya se cerró (el del motor hace su `End`), así que esto va en la suya.
    if (m_scene && panel.isOpen()) {
        if (ImGui::Begin("Prefabricado · escena")) {
            ImGui::TextWrapped("Colocar mete UNA entrada en la escena. Las piezas las pone el "
                               "expansor y no se guardan: el fichero de escena queda con una línea "
                               "por montaje, no con una por pieza.");
            if (ImGui::Button("Colocar en la escena")) {
                // ── DELANTE DE LA CÁMARA, NO EN EL ORIGEN DEL MUNDO ─────────────────────────────
                //
                // ⚠️ Colocaba en (0,0,0) y con un planeta cargado eso es el CENTRO DEL PLANETA: el
                // montaje aparecía "en el espacio", a miles de kilómetros de donde estabas mirando,
                // y había que ir a buscarlo. Lo mismo que hace el juego al colocar
                // (`spawnPrefab(..., distance)`): delante de quien mira.
                //
                // La distancia sale del TAMAÑO del montaje, no de un número fijo: un tanque de 6 m
                // y un casco de 40 no se encuadran a la misma distancia, y a 10 m de un barco lo que
                // ves es una plancha.
                glm::dvec3 pos = panel.origin();
                double radio = 1.0;
                if (const Haruka::Game::Prefab::Prefab* base = basePrefab(panel.openName()))
                    for (const Haruka::Game::Prefab::PrefabPiece& pc : base->pieces)
                        radio = std::max(radio, glm::length(pc.pos));
                if (Haruka::Core::Application* app = Haruka::Renderer::MotorInstance::getInstance().getApplication())
                    if (Haruka::Core::Camera* cam = app->getCamera())
                        pos = glm::dvec3(cam->position)
                            + glm::dvec3(cam->getFront()) * std::max(radio * 3.0, 5.0);

                std::cout << "[Prefab] colocar '" << panel.openName() << "' radio " << radio
                          << " m -> (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                auto entry = std::make_shared<Haruka::Core::Scene::SceneObject>();
                entry->name = panel.openName() + "_" +
                              std::to_string(m_scene->getAllObjects().size());
                entry->type = "Prefab";
                entry->objectType = Haruka::Tools::classifyObjectType("Prefab");
                entry->prefabName = panel.openName();
                entry->position = pos;
                entry->useOrientation = true;
                m_scene->addLoadedObject(entry);
                m_scene->expandPrefabs();
                if (m_onChanged) m_onChanged();
            }
        }
        ImGui::End();
    }

    // ⚠️ SE REENGANCHA CADA FRAME, Y NO ES DERROCHE. El panel devuelve un puntero DENTRO de su vector
    // de piezas: añadir o borrar una lo realoja, y guardarse ese puntero de un frame para otro es
    // justo el fallo que el panel documenta. Preguntarlo cada frame lo hace imposible por
    // construcción — y cuando no hay selección, `nullptr` devuelve el gizmo al objeto de la escena.
    if (viewport) viewport->setPrefabEdit(panel.selectedPiece(), panel.origin());
}
