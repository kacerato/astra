# Spike S-07: Astra UI (RmlUi 6.3) sobre o The Forge. Biblioteca usada pelo app de teste do S-02.
add_library(astra_s07_ui STATIC forge_render_interface.cpp ui_spike.cpp)
target_include_directories(astra_s07_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astra_s07_ui PUBLIC tf_graphics PRIVATE rmlui_core)
set_target_properties(astra_s07_ui PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(astra_s07_ui PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(astra_s07_ui PRIVATE -Wall -Wextra -Werror -Wno-missing-field-initializers)
endif()

# Shaders da UI: o app que usa a biblioteca chama astra_s07_ui_shaders(<alvo> <assets>).
set(S07_SHADER_LIST "${CMAKE_CURRENT_SOURCE_DIR}/shaders/rmlui.list" CACHE INTERNAL "")
function(astra_s07_ui_shaders target assets_dir)
    astra_fsl_shaders(${target} LIST "${S07_SHADER_LIST}" OUT_DIR "${assets_dir}")
endfunction()
