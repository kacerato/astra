# Luau 0.740 (doc 14): VM, compilador e codegen nativo como bibliotecas estáticas. Sem CLI, testes e web.
set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("${ASTRA_ROOT}/third_party/luau" "${CMAKE_BINARY_DIR}/third_party/luau" EXCLUDE_FROM_ALL SYSTEM)
