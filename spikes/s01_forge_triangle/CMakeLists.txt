# Spike S-01: The Forge 1.63 por CMake, Vulkan no Android (NDK r28+, 16 KB) e no Windows.

set(S01_ASSETS "${CMAKE_CURRENT_BINARY_DIR}/assets")

if(ANDROID)
    add_library(astra_spike_s01 SHARED main.cpp)
    target_link_libraries(astra_spike_s01 PRIVATE tf_renderer tf_app_glue)
    # Mesmo efeito do "-u ANativeActivity_onCreate" do Android.arm64.v8a.props do TF:
    # mantém o ponto de entrada do native_app_glue dentro da .so.
    target_link_options(astra_spike_s01 PRIVATE "-u" "ANativeActivity_onCreate")
    # Nome e pasta esperados pelo empacotamento Gradle (android/build.gradle.kts: jniLibs/<abi>).
    set_target_properties(astra_spike_s01 PROPERTIES
        OUTPUT_NAME "ForgeGame"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/jniLibs/${ANDROID_ABI}")
    # Camada de validação Vulkan embutida no TF (SDK 1.3.275), só em Debug. Ela foi compilada com a STL
    # compartilhada e precisa do libc++_shared.so ao lado (a .so do app usa c++_static).
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(TARGET astra_spike_s01 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TF_C3}/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Android/libVkLayer_khronos_validation.so"
                "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-android/libc++_shared.so"
                "${CMAKE_BINARY_DIR}/jniLibs/${ANDROID_ABI}/"
            VERBATIM)
    endif()
    set(S01_PATH_STATEMENT "${CMAKE_CURRENT_SOURCE_DIR}/assets/PathStatement.Android.txt")
    set(S01_GPU_DATA "${TF_C3}/OS/Android/android_gpu.data")
    set(S01_GPU_DATA_DST "${S01_ASSETS}/Other/gpu.data")
else()
    add_executable(astra_spike_s01 WIN32 main.cpp)
    target_link_libraries(astra_spike_s01 PRIVATE tf_renderer)
    set_target_properties(astra_spike_s01 PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${S01_ASSETS}")
    # DEFINE_APPLICATION_MAIN do TF no Windows: main() com subsistema WIN32 e exports do Agility SDK
    # do D3D12 (inofensivos com Vulkan; mesmo valor do TF_Shared.props).
    target_link_options(astra_spike_s01 PRIVATE /ENTRY:mainCRTStartup)
    target_compile_definitions(astra_spike_s01 PRIVATE D3D12_AGILITY_SDK_VERSION=715)
    set(S01_PATH_STATEMENT "${CMAKE_CURRENT_SOURCE_DIR}/assets/PathStatement.Windows.txt")
    set(S01_GPU_DATA "${TF_C3}/OS/Windows/pc_gpu.data")
    set(S01_GPU_DATA_DST "${S01_ASSETS}/gpu.data")
endif()

target_include_directories(astra_spike_s01 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# C++17: as macros de entrada do TF usam u8"" como const char* (inválido em C++20). O código Astra (fora dos
# spikes) não usa DEFINE_APPLICATION_MAIN.
set_target_properties(astra_spike_s01 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# Conteúdo empacotado: shaders compilados, textura gerada, gpu.data/gpu.cfg e PathStatement.
astra_fsl_shaders(astra_spike_s01 LIST "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders.list" OUT_DIR "${S01_ASSETS}")

add_custom_command(
    OUTPUT "${S01_ASSETS}/Textures/astra_checker.ktx"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${S01_ASSETS}/Textures"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/make_texture.py" "${S01_ASSETS}/Textures/astra_checker.ktx"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/make_texture.py"
    COMMENT "Gerando astra_checker.ktx"
    VERBATIM)

add_custom_command(
    OUTPUT "${S01_ASSETS}/PathStatement.txt" "${S01_GPU_DATA_DST}" "${S01_ASSETS}/GPUCfg/gpu.cfg"
    COMMAND ${CMAKE_COMMAND} -E copy "${S01_PATH_STATEMENT}" "${S01_ASSETS}/PathStatement.txt"
    COMMAND ${CMAKE_COMMAND} -E copy "${S01_GPU_DATA}" "${S01_GPU_DATA_DST}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/gpu.cfg" "${S01_ASSETS}/GPUCfg/gpu.cfg"
    DEPENDS "${S01_PATH_STATEMENT}" "${S01_GPU_DATA}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/gpu.cfg"
    VERBATIM)

add_custom_target(astra_spike_s01_assets DEPENDS
    "${S01_ASSETS}/Textures/astra_checker.ktx" "${S01_ASSETS}/PathStatement.txt" "${S01_GPU_DATA_DST}"
    "${S01_ASSETS}/GPUCfg/gpu.cfg")
add_dependencies(astra_spike_s01 astra_spike_s01_assets)
astra_tf_runtime_files(astra_spike_s01)
