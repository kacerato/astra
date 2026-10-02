# Spike S-06: benchmark do flecs para o editor (executável de console; no Android roda por adb shell).
add_executable(astra_spike_s06 main.cpp)
target_link_libraries(astra_spike_s06 PRIVATE flecs)
set_target_properties(astra_spike_s06 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(astra_spike_s06 PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(astra_spike_s06 PRIVATE -Wall -Wextra -Werror)
endif()
