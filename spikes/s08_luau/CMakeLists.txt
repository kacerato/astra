# Spike S-08: Luau 0.740 (VM, compilador, codegen nativo).
add_library(astra_s08_suite STATIC luau_suite.cpp)
target_include_directories(astra_s08_suite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astra_s08_suite PRIVATE Luau.VM Luau.Compiler Luau.CodeGen)
set_target_properties(astra_s08_suite PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(astra_spike_s08 main.cpp)
target_link_libraries(astra_spike_s08 PRIVATE astra_s08_suite)
set_target_properties(astra_spike_s08 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

foreach(t astra_s08_suite astra_spike_s08)
    if(MSVC)
        target_compile_options(${t} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${t} PRIVATE -Wall -Wextra -Werror)
    endif()
endforeach()
