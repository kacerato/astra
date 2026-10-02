# Build do The Forge 1.63 por CMake (substitui os projetos VS2019/AGDE do upstream).
# Lista de fontes derivada de Examples_3/Unit_Tests/{Android_VS2019,PC_VS2019}/Libraries
# (OS.vcxproj, Renderer.vcxproj e Tests/RendererVulkan.vcxproj). Só Vulkan.
#
# Alvos:
#   tf_core      - Utilities (memória, log, arquivos, threads, tempo, compressão) + partes do OS sem janela/app
#   tf_graphics  - Graphics (Vulkan), ResourceLoader, cliente de recarga de shaders. Depende de tf_core.
#                  É o que a Astra usa (backends/forge); a plataforma Astra fornece janela, ciclo de vida e
#                  os dois símbolos que o renderer pede da camada de janela (gWindow, AndroidAttachToCurrentThread).
#   tf_os        - Camada de aplicação do TF (IApp, UI ImGui, fontes, Lua, input, janela, animação).
#                  No Android depende do native_app_glue do NativeActivity. Só para spikes que usam IApp.
#   tf_renderer  - tf_graphics + tf_os + Renderer/{ParticleSystem,VisibilityBuffer}
#   tf_app_glue  - native_app_glue do NDK (Android; ligado por aplicações IApp)
#
# Funções:
#   astra_fsl_shaders(<alvo> LIST <arquivo.list> OUT_DIR <dir>)
#     Gera e compila shaders FSL para SPIR-V (Shaders/ + CompiledShaders/ em OUT_DIR).
#   astra_tf_runtime_files(<alvo>)
#     Windows: copia DLLs de runtime (AGS, camada de validação) para a pasta do executável.

set(TF_ROOT "${ASTRA_ROOT}/third_party/the-forge")
set(TF_C3 "${TF_ROOT}/Common_3")

if(NOT EXISTS "${TF_C3}/Graphics/Interfaces/IGraphics.h")
    message(FATAL_ERROR "The Forge não encontrado em ${TF_ROOT}. Rode: git submodule update --init third_party/the-forge")
endif()

if(ANDROID)
    set(TF_PLATFORM_ANDROID ON)
elseif(WIN32)
    set(TF_PLATFORM_WINDOWS ON)
else()
    message(FATAL_ERROR "Plataforma ainda não suportada pelo build do The Forge: ${CMAKE_SYSTEM_NAME}")
endif()

# ---------------------------------------------------------------------------
# Fontes
# ---------------------------------------------------------------------------
set(TF_ZSTD_DIR ${TF_C3}/Utilities/ThirdParty/OpenSource/zstd)
set(TF_CORE_SOURCES
    ${TF_C3}/OS/CPUConfig.cpp
    ${TF_C3}/Utilities/FileSystem/FileSystem.c
    ${TF_C3}/Utilities/Log/Log.c
    ${TF_C3}/Utilities/Math/Algorithms.c
    ${TF_C3}/Utilities/Math/StbDs.c
    ${TF_C3}/Utilities/MemoryTracking/MemoryTracking.c
    ${TF_C3}/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.c
    ${TF_C3}/Utilities/ThirdParty/OpenSource/lz4/lz4.c
    ${TF_C3}/Utilities/Threading/ThreadSystem.c
    ${TF_C3}/Utilities/Timer.c
    ${TF_ZSTD_DIR}/common/debug.c
    ${TF_ZSTD_DIR}/common/entropy_common.c
    ${TF_ZSTD_DIR}/common/error_private.c
    ${TF_ZSTD_DIR}/common/fse_decompress.c
    ${TF_ZSTD_DIR}/common/pool.c
    ${TF_ZSTD_DIR}/common/threading.c
    ${TF_ZSTD_DIR}/common/xxhash.c
    ${TF_ZSTD_DIR}/common/zstd_common.c
    ${TF_ZSTD_DIR}/decompress/huf_decompress.c
    ${TF_ZSTD_DIR}/decompress/zstd_ddict.c
    ${TF_ZSTD_DIR}/decompress/zstd_decompress.c
    ${TF_ZSTD_DIR}/decompress/zstd_decompress_block.c
)

set(TF_APP_SOURCES
    ${TF_C3}/Application/CameraController.cpp
    ${TF_C3}/Application/Profiler/GpuProfiler.cpp
    ${TF_C3}/Application/Profiler/ProfilerBase.cpp
    ${TF_C3}/Application/Screenshot/Screenshot.cpp
    ${TF_C3}/Application/UI/UI.cpp
    ${TF_C3}/Application/Fonts/FontSystem.cpp
    ${TF_C3}/Application/Fonts/stbtt.cpp
    ${TF_C3}/Application/ThirdParty/OpenSource/imgui/imgui.cpp
    ${TF_C3}/Application/ThirdParty/OpenSource/imgui/imgui_demo.cpp
    ${TF_C3}/Application/ThirdParty/OpenSource/imgui/imgui_draw.cpp
    ${TF_C3}/Application/ThirdParty/OpenSource/imgui/imgui_widgets.cpp
    ${TF_C3}/Application/ThirdParty/OpenSource/imgui/imgui_tables.cpp
    ${TF_C3}/Game/Scripting/LuaManager.cpp
    ${TF_C3}/Game/Scripting/LuaManagerImpl.cpp
    ${TF_C3}/Game/Scripting/LuaSystem.cpp
    ${TF_C3}/OS/WindowSystem/WindowSystem.cpp
    ${TF_C3}/Tools/ReloadServer/ReloadClient.cpp
    ${TF_C3}/Resources/AnimationSystem/Animation/AnimatedObject.cpp
    ${TF_C3}/Resources/AnimationSystem/Animation/Animation.cpp
    ${TF_C3}/Resources/AnimationSystem/Animation/Clip.cpp
    ${TF_C3}/Resources/AnimationSystem/Animation/ClipController.cpp
    ${TF_C3}/Resources/AnimationSystem/Animation/ClipMask.cpp
    ${TF_C3}/Resources/AnimationSystem/Animation/Rig.cpp
    ${TF_C3}/Resources/AnimationSystem/Animation/SkeletonBatcher.cpp
)
set(TF_LUA_DIR ${TF_C3}/Game/ThirdParty/OpenSource/lua-5.3.5/src)
foreach(f lapi lauxlib lbaselib lbitlib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc lgc linit
          liolib llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate lstring lstrlib
          ltable ltablib ltm lundump lutf8lib lvm lzio)
    list(APPEND TF_APP_SOURCES ${TF_LUA_DIR}/${f}.c)
endforeach()

if(TF_PLATFORM_ANDROID)
    list(APPEND TF_CORE_SOURCES
        ${TF_C3}/OS/Android/AndroidFileSystem.cpp
        ${TF_C3}/OS/Android/AndroidLog.c
        ${TF_C3}/OS/Android/AndroidThread.c
        ${TF_C3}/OS/Android/AndroidTime.c
        ${TF_C3}/OS/ThirdParty/OpenSource/cpu_features/src/hwcaps.c
        ${TF_C3}/OS/ThirdParty/OpenSource/cpu_features/src/impl_aarch64_linux_or_android.c
        ${TF_C3}/OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_linux_or_android.c
        ${TF_C3}/Utilities/FileSystem/UnixFileSystem.c
    )
    list(APPEND TF_APP_SOURCES
        ${TF_C3}/OS/Android/AndroidBase.cpp
        ${TF_C3}/OS/Android/AndroidInput.cpp
        ${TF_C3}/OS/Android/AndroidWindow.cpp
    )
elseif(TF_PLATFORM_WINDOWS)
    list(APPEND TF_CORE_SOURCES
        ${TF_C3}/OS/Windows/WindowsFileSystem.cpp
        ${TF_C3}/OS/Windows/WindowsLog.c
        ${TF_C3}/OS/Windows/WindowsStackTraceDump.cpp
        ${TF_C3}/OS/Windows/WindowsThread.c
        ${TF_C3}/OS/Windows/WindowsTime.c
        ${TF_C3}/OS/ThirdParty/OpenSource/cpu_features/src/impl_x86_windows.c
    )
    list(APPEND TF_APP_SOURCES
        ${TF_C3}/OS/Windows/WindowsBase.cpp
        ${TF_C3}/OS/Windows/WindowsInput.cpp
        ${TF_C3}/OS/Windows/WindowsWindow.cpp
        ${TF_C3}/OS/ThirdParty/OpenSource/hidapi/windows/hid.c
    )
endif()

set(TF_GRAPHICS_SOURCES
    ${TF_C3}/Graphics/GraphicsConfig.cpp
    ${TF_C3}/Graphics/Vulkan/Vulkan.c
    ${TF_C3}/Graphics/Vulkan/VulkanRaytracing.c
    ${TF_C3}/Graphics/Vulkan/Vulkan_Cxx.cpp
    ${TF_C3}/Resources/ResourceLoader/ResourceLoader.cpp
    ${TF_C3}/Tools/Network/Network.c
)

set(TF_RENDERER_EXTRA_SOURCES
    ${TF_C3}/Renderer/ParticleSystem/ParticleSystem.cpp
    ${TF_C3}/Renderer/VisibilityBuffer/VisibilityBuffer.cpp
)

# ---------------------------------------------------------------------------
# Opções comuns (equivalentes a Examples_3/Build_Props/VS/TF_Shared.props)
# ---------------------------------------------------------------------------
add_library(tf_config INTERFACE)
# Recarga de shaders do TF desligada em todo o build (patch em Config.h): depende da camada de app (UI, input).
target_compile_definitions(tf_config INTERFACE ASTRA_FORGE_NO_RELOAD_SHADER)
# SYSTEM: avisos dos headers do TF não contam contra o -Werror do código Astra.
target_include_directories(tf_config SYSTEM INTERFACE "${TF_C3}/..")

if(TF_PLATFORM_ANDROID)
    target_compile_definitions(tf_config INTERFACE ANDROID_ARM_NEON)
    target_include_directories(tf_config SYSTEM INTERFACE "${TF_C3}/OS/ThirdParty/OpenSource/agdk/include")
elseif(TF_PLATFORM_WINDOWS)
    target_compile_definitions(tf_config INTERFACE
        FORGE_EXPLICIT_RENDERER_API FORGE_EXPLICIT_RENDERER_API_VULKAN UNICODE _UNICODE)
endif()

# Terceiros: avisos silenciados (AGENTS.md / doc 04 §12). Exceções/RTTI ficam no padrão do
# compilador (doc 03: Luau e RmlUi precisam delas no código Astra).
function(_tf_quiet target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W0 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
    set_target_properties(${target} PROPERTIES C_STANDARD 11 CXX_STANDARD 17 CXX_EXTENSIONS OFF)
endfunction()

add_library(tf_core STATIC ${TF_CORE_SOURCES})
target_link_libraries(tf_core PUBLIC tf_config)
_tf_quiet(tf_core)

add_library(tf_graphics STATIC ${TF_GRAPHICS_SOURCES})
target_link_libraries(tf_graphics PUBLIC tf_core)
_tf_quiet(tf_graphics)

add_library(tf_os STATIC ${TF_APP_SOURCES})
target_link_libraries(tf_os PUBLIC tf_graphics)
_tf_quiet(tf_os)

add_library(tf_renderer STATIC ${TF_RENDERER_EXTRA_SOURCES})
target_link_libraries(tf_renderer PUBLIC tf_os)
_tf_quiet(tf_renderer)

if(TF_PLATFORM_ANDROID)
    set(TF_AGDK_LIBS "${TF_C3}/OS/ThirdParty/OpenSource/agdk/libs/arm64-v8a_cpp_static_Release")
    target_link_libraries(tf_core PUBLIC android log atomic)
    # Swappy (frame pacing) é chamado pelo Vulkan.c.
    target_link_libraries(tf_graphics PUBLIC "${TF_AGDK_LIBS}/libswappy_static.a")
    # Camada de app do TF: glue do NativeActivity, Memory Advice (AndroidBase) e Paddleboat (AndroidInput).
    target_include_directories(tf_os PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")
    target_link_libraries(tf_os PUBLIC
        "${TF_AGDK_LIBS}/libmemory_advice_static.a"
        "${TF_AGDK_LIBS}/libpaddleboat_static.a")

    add_library(tf_app_glue STATIC "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
    target_include_directories(tf_app_glue PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")
    _tf_quiet(tf_app_glue)
elseif(TF_PLATFORM_WINDOWS)
    target_link_libraries(tf_core PUBLIC dbghelp shlwapi)
    target_link_libraries(tf_graphics PUBLIC ws2_32)
    target_link_libraries(tf_os PUBLIC setupapi xinput winmm)
    # Extensões de fabricante usadas pelo Vulkan.c/Vulkan_Cxx.cpp no Windows (como no upstream).
    set(TF_GFX_3P "${TF_C3}/Graphics/ThirdParty/OpenSource")
    target_link_libraries(tf_graphics PUBLIC "${TF_GFX_3P}/ags/ags_lib/lib/amd_ags_x64.lib" "${TF_GFX_3P}/nvapi/amd64/nvapi64.lib")
    # DLLs que precisam ficar ao lado do executável: AGS e a camada de validação Vulkan embutida.
    set(TF_WINDOWS_RUNTIME_FILES
        "${TF_GFX_3P}/ags/ags_lib/lib/amd_ags_x64.dll"
        "${TF_GFX_3P}/VulkanSDK/bin/Win32/VkLayer_khronos_validation.dll"
        "${TF_GFX_3P}/VulkanSDK/bin/Win32/VkLayer_khronos_validation.json")
endif()

# Copia os arquivos de runtime do TF para a pasta do executável (Windows).
function(astra_tf_runtime_files target)
    if(TF_PLATFORM_WINDOWS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TF_WINDOWS_RUNTIME_FILES} "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Shaders FSL → SPIR-V (toolchain Python do TF + glslangValidator do VulkanSDK embutido)
# ---------------------------------------------------------------------------
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(TF_GLSLANG_DIR "${TF_C3}/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Win32")
if(NOT CMAKE_HOST_WIN32)
    set(TF_GLSLANG_DIR "${TF_C3}/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Linux")
endif()

if(TF_PLATFORM_ANDROID)
    set(TF_FSL_LANGUAGE ANDROID_VULKAN)
else()
    set(TF_FSL_LANGUAGE VULKAN)
endif()

function(astra_fsl_shaders target)
    cmake_parse_arguments(ARG "" "LIST;OUT_DIR" "" ${ARGN})
    get_filename_component(list_name "${ARG_LIST}" NAME_WE)
    get_filename_component(list_dir "${ARG_LIST}" DIRECTORY)
    file(GLOB_RECURSE fsl_deps CONFIGURE_DEPENDS "${list_dir}/*.fsl" "${list_dir}/*.h" "${ARG_LIST}")
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/fsl/${list_name}.stamp")
    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND ${CMAKE_COMMAND} -E env
                "FSL_COMPILER_VK=${TF_GLSLANG_DIR}" "FSL_COMPILER_LINUX_VK=${TF_GLSLANG_DIR}"
                ${Python3_EXECUTABLE} "${TF_C3}/Tools/ForgeShadingLanguage/fsl.py"
                "${ARG_LIST}"
                -d "${ARG_OUT_DIR}/Shaders"
                -b "${ARG_OUT_DIR}/CompiledShaders"
                -i "${CMAKE_CURRENT_BINARY_DIR}/fsl"
                -l ${TF_FSL_LANGUAGE}
                --compile $<$<CONFIG:Debug>:--debug>
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS ${fsl_deps}
        COMMENT "FSL ${list_name} -> ${TF_FSL_LANGUAGE}"
        VERBATIM
        COMMAND_EXPAND_LISTS) # em Release o $<...:--debug> fica vazio e não pode virar argumento ""
    add_custom_target(${target}_fsl_${list_name} DEPENDS "${stamp}")
    add_dependencies(${target} ${target}_fsl_${list_name})
endfunction()
