# flecs 4.1.6 (núcleo: engine/world, doc 03). Build enxuto: só o que a Astra usa (C++ API, sistemas, pipeline,
# log). REST/HTTP/script/stats ficam fora; o Explorer (REST) entra só em debug quando for necessário.
set(FLECS_DIR "${ASTRA_ROOT}/third_party/flecs/distr")
add_library(flecs STATIC "${FLECS_DIR}/flecs.c")
target_include_directories(flecs SYSTEM PUBLIC "${FLECS_DIR}")
target_compile_definitions(flecs PUBLIC flecs_STATIC FLECS_CUSTOM_BUILD FLECS_CPP FLECS_SYSTEM FLECS_PIPELINE FLECS_LOG)
set_target_properties(flecs PROPERTIES C_STANDARD 99)
if(MSVC)
    target_compile_options(flecs PRIVATE /W0)
else()
    target_compile_options(flecs PRIVATE -w)
endif()
