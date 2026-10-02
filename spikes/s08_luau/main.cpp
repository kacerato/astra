// Spike S-08: executável de console (PC e adb shell).
#include "luau_suite.h"

#include <cstdio>

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    return runLuauSuite([](const char* line) { std::printf("%s\n", line); }) == 0 ? 0 : 1;
}
