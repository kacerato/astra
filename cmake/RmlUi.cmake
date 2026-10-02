# RmlUi 6.3 + FreeType 2.14.3 (doc 13 e 16: Astra UI para editor e jogos). Spike S-07.
# FreeType sem dependências opcionais (zlib, bzip2, png, harfbuzz, brotli).
foreach(dep ZLIB BZIP2 PNG HARFBUZZ BROTLI)
    set(FT_DISABLE_${dep} TRUE CACHE BOOL "" FORCE)
endforeach()
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory("${ASTRA_ROOT}/third_party/freetype" "${CMAKE_BINARY_DIR}/third_party/freetype" EXCLUDE_FROM_ALL SYSTEM)

# A RmlUi chama find_package(Freetype): o redirecionamento do CMake (>= 3.24, o mesmo do FetchContent) entrega o
# alvo compilado aqui em vez de procurar uma instalação no sistema.
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/freetype-config.cmake"
    "if(NOT TARGET Freetype::Freetype)\n  add_library(Freetype::Freetype ALIAS freetype)\nendif()\nset(FREETYPE_VERSION_STRING 2.14.3)\nset(Freetype_FOUND TRUE)\n")

set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
set(RMLUI_COMPILER_OPTIONS OFF CACHE BOOL "" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
add_subdirectory("${ASTRA_ROOT}/third_party/rmlui" "${CMAKE_BINARY_DIR}/third_party/rmlui" EXCLUDE_FROM_ALL SYSTEM)
foreach(t freetype rmlui_core rmlui_debugger)
    if(TARGET ${t})
        if(MSVC)
            target_compile_options(${t} PRIVATE /W0 /utf-8)
        else()
            target_compile_options(${t} PRIVATE -w)
        endif()
    endif()
endforeach()
