/**
 * @file scene_model.h
 * @brief La escena de un motor descrita de forma GENÉRICA: nodos con propiedades tipadas.
 *
 * Es lo que permite al núcleo dibujar una jerarquía y un inspector para un motor que no conoce
 * (Unreal, O3DE, el motor simulado): el backend traduce su escena a esto y aplica los cambios que
 * vuelven. Un backend con paneles propios (Haruka, que edita `SceneObject` directamente) no lo
 * necesita y devuelve nullptr en `IEngineBackend::genericScene()`.
 *
 * ⚠️ PUEDE SER ASÍNCRONO. En un motor remoto `setProperty` sale por un socket y el cambio vuelve
 * después, con otra `revision()`. Por eso `nodes()` devuelve una COPIA (la escena puede cambiar en
 * otro hilo mientras el panel la recorre) y el inspector no da por hecho que el valor ya cambió.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SceneProperty {
    std::string name;
    /// "float" | "int" | "bool" | "string" | "vec3" | "color"
    std::string type;
    std::vector<double> num;   ///< float/int/bool: num[0]; vec3/color: num[0..2]
    std::string str;           ///< string
    bool readOnly = false;

    bool operator==(const SceneProperty& o) const {
        return name == o.name && type == o.type && num == o.num && str == o.str;
    }
    bool operator!=(const SceneProperty& o) const { return !(*this == o); }
};

struct SceneNode {
    std::string id;
    std::string name;
    std::string type;
    std::string parent;   ///< id del padre; vacío = raíz
    std::vector<SceneProperty> properties;
};

class ISceneModel {
public:
    virtual ~ISceneModel() = default;
    /// Sube cada vez que cambia algo de la escena (nodos, valores o selección).
    virtual uint64_t revision() const = 0;
    virtual std::vector<SceneNode> nodes() const = 0;
    virtual std::string selected() const = 0;
    virtual void select(const std::string& nodeId) = 0;
    /// Pide el cambio; puede aplicarse más tarde (ver la cabecera).
    virtual void setProperty(const std::string& nodeId, const SceneProperty& value) = 0;
};
