# Spike S-02: renderer do The Forge (tf_graphics) com GameActivity e loop próprio, sem IApp nem o glue do TF.
if(NOT ANDROID)
    return()
endif()

# GameActivity 4.4.2 (androidx.games:games-activity), headers e lib estática copiados do prefab do AAR.
set(GAMES_ACTIVITY_DIR "${ASTRA_ROOT}/third_party/games-activity")
add_library(games_activity STATIC IMPORTED)
set_target_properties(games_activity PROPERTIES
    IMPORTED_LOCATION "${GAMES_ACTIVITY_DIR}/lib/${ANDROID_ABI}/libgame-activity_static.a"
    INTERFACE_INCLUDE_DIRECTORIES "${GAMES_ACTIVITY_DIR}/include")

set(S02_ASSETS "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(S01_DIR "${ASTRA_ROOT}/spikes/s01_forge_triangle")

add_library(astra_spike_s02 SHARED main.cpp forge_android_shim.cpp)
target_include_directories(astra_spike_s02 PRIVATE "${S01_DIR}")
target_link_libraries(astra_spike_s02 PRIVATE tf_graphics games_activity glslang glslang-default-resource-limits astra_s08_suite astra_s07_ui android log)
target_include_directories(astra_spike_s02 SYSTEM PRIVATE "${ASTRA_ROOT}/third_party/glslang")
# Pontos de entrada achados por dlsym/JNI a partir da lib estática do GameActivity.
target_link_options(astra_spike_s02 PRIVATE
    "-Wl,--undefined=GameActivity_onCreate"
    "-Wl,--undefined=Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")
set_target_properties(astra_spike_s02 PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jniLibs-s02/${ANDROID_ABI}")
# -Wno-missing-field-initializers: a API do TF usa inicialização agregada parcial por padrão ({ a, b }).
target_compile_options(astra_spike_s02 PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET astra_spike_s02 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TF_C3}/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Android/libVkLayer_khronos_validation.so"
            "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-android/libc++_shared.so"
            "${CMAKE_BINARY_DIR}/jniLibs-s02/${ANDROID_ABI}/"
        VERBATIM)
endif()

# Mesmos shaders, textura e configuração do S-01.
astra_fsl_shaders(astra_spike_s02 LIST "${S01_DIR}/shaders/shaders.list" OUT_DIR "${S02_ASSETS}")
astra_s07_ui_shaders(astra_spike_s02 "${S02_ASSETS}")
add_custom_command(
    OUTPUT "${S02_ASSETS}/Textures/astra_checker.ktx" "${S02_ASSETS}/PathStatement.txt"
           "${S02_ASSETS}/Other/gpu.data" "${S02_ASSETS}/GPUCfg/gpu.cfg"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${S02_ASSETS}/Textures"
    COMMAND ${Python3_EXECUTABLE} "${S01_DIR}/make_texture.py" "${S02_ASSETS}/Textures/astra_checker.ktx"
    COMMAND ${CMAKE_COMMAND} -E copy "${S01_DIR}/assets/PathStatement.Android.txt" "${S02_ASSETS}/PathStatement.txt"
    COMMAND ${CMAKE_COMMAND} -E copy "${TF_C3}/OS/Android/android_gpu.data" "${S02_ASSETS}/Other/gpu.data"
    COMMAND ${CMAKE_COMMAND} -E copy "${S01_DIR}/assets/gpu.cfg" "${S02_ASSETS}/GPUCfg/gpu.cfg"
    DEPENDS "${S01_DIR}/make_texture.py" "${S01_DIR}/assets/PathStatement.Android.txt" "${S01_DIR}/assets/gpu.cfg"
    VERBATIM)
# S-04: o GLSL à mão vai como texto (compilado no aparelho) e como SPIR-V compilado no PC pelo glslangValidator
# embutido no TF (mesmo alvo spirv1.0 do FSL).
set(S02_GLSL_OUT "${S02_ASSETS}/Other/GLSL")
set(S02_GLSL_OUTPUTS)
foreach(stage vert frag)
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/glsl/triangle.${stage}")
    add_custom_command(
        OUTPUT "${S02_GLSL_OUT}/triangle.${stage}" "${S02_GLSL_OUT}/triangle.${stage}.spv"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${S02_GLSL_OUT}"
        COMMAND ${CMAKE_COMMAND} -E copy "${src}" "${S02_GLSL_OUT}/triangle.${stage}"
        COMMAND "${TF_GLSLANG_DIR}/glslangValidator" -V --target-env spirv1.0 "${src}" -o "${S02_GLSL_OUT}/triangle.${stage}.spv"
        DEPENDS "${src}"
        VERBATIM)
    list(APPEND S02_GLSL_OUTPUTS "${S02_GLSL_OUT}/triangle.${stage}" "${S02_GLSL_OUT}/triangle.${stage}.spv")
endforeach()
add_custom_target(astra_spike_s02_assets DEPENDS "${S02_ASSETS}/Textures/astra_checker.ktx" ${S02_GLSL_OUTPUTS})
add_dependencies(astra_spike_s02 astra_spike_s02_assets)
