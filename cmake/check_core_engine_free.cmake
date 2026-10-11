# Comprueba que el NÚCLEO del editor no sabe de ningún motor (cmake -DSRC=<src> -P este fichero).
#
# Núcleo = todo src/ menos src/backends/. Falla el build si un fichero del núcleo:
#   * incluye una cabecera de haruka-cpp ("core/…", "renderer/…", "world/…", …),
#   * nombra el namespace Haruka:: (tipos del motor),
#   * incluye algo de un backend (los backends se CARGAN al arrancar, nunca se enlazan).
#
# El núcleo ya no tiene las cabeceras del motor en su ruta de includes (no enlaza el motor), así que
# un include del motor ya no compilaría; esto sigue cazando lo que SÍ compilaría: nombrar
# `Haruka::` en un comentario-guía que alguien convierta en código, o una ruta relativa a backends/.

if(NOT SRC)
    message(FATAL_ERROR "check_core_engine_free: falta -DSRC=<directorio src>")
endif()

set(_engine_dirs "core|renderer|world|rhi|game|physics|io|tools|audio|network|database")
file(GLOB_RECURSE _files "${SRC}/*.h" "${SRC}/*.hpp" "${SRC}/*.cpp")

set(_violations "")
foreach(_f IN LISTS _files)
    file(RELATIVE_PATH _rel "${SRC}" "${_f}")
    if(_rel MATCHES "^backends/")
        continue()
    endif()
    file(STRINGS "${_f}" _lines)
    set(_n 0)
    foreach(_l IN LISTS _lines)
        math(EXPR _n "${_n} + 1")
        set(_bad "")
        if(_l MATCHES "^[ \t]*#[ \t]*include[ \t]*[\"<](${_engine_dirs})/")
            set(_bad "cabecera del motor")
        elseif(_l MATCHES "Haruka::")
            set(_bad "tipo del motor (Haruka::)")
        elseif(_l MATCHES "^[ \t]*#[ \t]*include[ \t]*\"backends/")
            set(_bad "cabecera de un backend")
        endif()
        if(_bad)
            string(STRIP "${_l}" _ls)
            string(APPEND _violations "  src/${_rel}:${_n}: ${_bad}: ${_ls}\n")
        endif()
    endforeach()
endforeach()

if(_violations)
    message(FATAL_ERROR "El núcleo del editor depende del motor (ver src/backend/engine_backend.h):\n${_violations}")
endif()
list(LENGTH _files _count)
message(STATUS "Núcleo del editor sin dependencias del motor (${_count} ficheros mirados)")
