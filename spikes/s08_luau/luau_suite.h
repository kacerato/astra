// Spike S-08: suíte Luau compartilhada entre o executável de console e o app Android (S-02).
#pragma once

using LuauSuiteLog = void (*)(const char* line);

// Roda todos os itens do S-08 e devolve o número de falhas.
int runLuauSuite(LuauSuiteLog log);
