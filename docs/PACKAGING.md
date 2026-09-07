# Empaquetado e instalación

## Construir el .rpm

```bash
./build.sh rpm
sudo dnf install packages/haruka-editor-*.rpm
```

`rpm` implica `release`. El paquete sale en `packages/` (y `dist/` sigue siendo el layout
portable, sin mezclar). Para desinstalar: `sudo dnf remove haruka-editor`.

Requisitos de la máquina que EMPAQUETA: `rpm-build`. Las dependencias de ejecución (SDL3,
assimp, OpenAL, GTK3…) no se declaran a mano: rpm las deduce del ELF, así que se mantienen
solas cuando cambian las del motor.

`cpack -G RPM` empaqueta el árbol **ya compilado**; no compila desde fuente dentro de
`mock`. Es deliberado: el motor se trae con `FetchContent`, que necesita red, y mock/COPR
construyen sin ella. Para publicar en COPR haría falta un `.spec` con el motor vendorizado
en el tarball — otro trabajo, no este.

## Dónde queda instalado

| Ruta | Qué |
|---|---|
| `/usr/bin/haruka-editor` | lanzador (hace `exec` del binario real) |
| `/usr/lib64/haruka-editor/HarukaEditor` | el editor |
| `/usr/lib64/haruka-editor/lib/libHarukaEngine.so` | motor con el que se compiló el editor |
| `/usr/lib64/haruka-editor/assets/shaders/` | shaders SPIR-V |
| `/usr/share/applications/haruka-editor.desktop` | entrada de menú |
| `/usr/share/icons/hicolor/scalable/apps/haruka-editor.svg` | icono |
| `/usr/share/mime/packages/haruka-editor.xml` | tipo MIME de los `.hrk` |
| `~/.config/haruka-editor/imgui.ini` | layout de paneles, por usuario |

El ejecutable **no** va en `/usr/bin` porque busca sus assets junto al binario
(`SDL_GetBasePath`) y lleva su propia `libHarukaEngine.so`; ese trío viaja siempre junto y
con la misma forma relativa (`exe` · `lib/` · `assets/`), que es lo que resuelve el
`INSTALL_RPATH` (`$ORIGIN:$ORIGIN/lib`). El lanzador de `/usr/bin` hace `exec`, así que
`/proc/self/exe` sigue apuntando al binario real y los assets se resuelven solos.

`libHarukaEngine.so` es una librería **privada**: el `.spec` filtra sus `Provides`/`Requires`
para no anunciarla al resto del sistema ni exigirse un soname que ningún repo sirve.

## El motor, y por qué el paquete no lo lleva

Aquí van dos motores distintos y conviene no mezclarlos:

- **El del editor** (`libHarukaEngine.so`): con el que está enlazado el binario, dibuja el
  viewport. Va dentro del paquete, no es opcional.
- **El SDK de motor**: el árbol de fuentes contra el que compilan *los proyectos del
  usuario*. Ese **no** viene en el paquete y se resuelve en tiempo de ejecución.

Antes esa segunda ruta era `-DHARUKA_ENGINE_ROOT="<ruta del build>"`: el checkout de quien
compilaba quedaba dentro del binario. En el equipo de desarrollo cuela; en una máquina donde
se instale el rpm, apunta a un directorio que no existe y el fallo sale tarde y mal ("Nuevo
proyecto" crea una carpeta vacía sin `CMakeLists`, y "Compilar" se pone a bajar el motor de
GitHub).

Ahora lo resuelve `EngineSdk` ([src/engine_sdk.h](../src/engine_sdk.h)) al arrancar, en este
orden — el primero que exista gana:

1. `$HARUKA_ENGINE_ROOT` — override por lanzamiento
2. `~/.config/haruka-editor/engine.json`, campo `"root"` — elección del usuario
3. `/usr/share/haruka-editor/engine` — SDK instalado por un paquete
4. `<dir del ejecutable>/engine`, `../engine` — layout portable (zip)
5. la ruta de compilación — solo en builds de desarrollo (`../haruka-cpp`)

Para apuntar a un motor cualquiera:

```bash
# por lanzamiento
HARUKA_ENGINE_ROOT=/ruta/al/motor haruka-editor

# permanente
mkdir -p ~/.config/haruka-editor
echo '{"root": "/ruta/al/motor"}' > ~/.config/haruka-editor/engine.json

# o para todos los usuarios de la máquina
sudo ln -s /ruta/al/motor /usr/share/haruka-editor/engine
```

Si no encuentra ninguno, el editor arranca igual (escenas, viewport, paneles) y avisa por
consola con la lista de rutas miradas; lo que no funciona es crear ni compilar proyectos.

## Usar OTRO motor: el descriptor

El editor no conoce ningún motor concreto. Lee `haruka-engine.json` de la raíz del SDK y con
eso ya sabe crear, compilar y exportar. Plantilla comentada en
[packaging/haruka-engine.json](../packaging/haruka-engine.json), instalada en
`/usr/share/doc/haruka-editor/`:

```json
{
  "api": 1,
  "name": "mi-motor",
  "version": "2.0",
  "template": "plantilla-proyecto",
  "build": { "script": "compilar.sh", "cmakeEngineVar": "MI_MOTOR_ROOT" },
  "runtime": { "binary": "build/mi-runtime" }
}
```

| Campo | Para qué lo usa el editor | Por defecto |
|---|---|---|
| `template` | carpeta que copia al crear un proyecto | `template` |
| `build.script` | script que ejecuta "Compilar proyecto" | `build.sh` |
| `build.cmakeEngineVar` | `-D<var>="<raíz>"` que le pasa a ese script | `HARUKA_ENGINE_LOCAL` |
| `runtime.binary` | binario que copia junto al juego exportado | `build/HarukaEngine` |

Todos los campos son opcionales y las rutas son relativas a la raíz del motor. Lo que falte
se queda con el valor por defecto, que son las convenciones de `haruka-cpp`: un motor **sin**
descriptor sigue funcionando igual que antes. Los que declaran ruta (`template`,
`runtime.binary`) solo se aceptan si existen en disco — un descriptor puede prometer un
runtime que ese checkout aún no ha compilado.

Lo que el descriptor **todavía no** cubre: el editor sigue enlazado contra `HarukaEngine` en
tiempo de compilación (escena, componentes, render del viewport). El contrato cubre el ciclo
de proyecto — crear, compilar, exportar —, no el runtime del propio editor.

## Layout portable (zip)

`./build.sh release` sigue dejando en `dist/` el layout de siempre, todo junto bajo el
prefijo. El layout lo decide `-DHARUKA_INSTALL_LAYOUT=portable|system`; `build.sh` reconfigura
limpio si cambia, porque las rutas de instalación se compilan dentro del binario.
