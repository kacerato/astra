// Spike S-07: Astra UI (RmlUi 6.3) dentro do app de teste do S-02.
// Independe de GameActivity: o app repassa toque, texto do teclado e pede para mostrar/esconder o teclado.
#pragma once

#include <cstdint>
#include <functional>

struct Renderer;
struct Cmd;

struct UiKeyboardBridge
{
    // UI pediu teclado (campo de texto focado), com o texto atual e a seleção em caracteres.
    std::function<void(const char* textUtf8, int selStart, int selEnd)> show;
    std::function<void()>                                             hide;
};

namespace UiSpike
{
bool init(Renderer* renderer, uint32_t colorFormat, uint32_t depthStencilFormat, const char* fontPath, float dpRatio,
          const UiKeyboardBridge& keyboard);
void exit();

// Uma vez por frame, antes de render: tamanho lógico (orientação que o usuário vê).
void update(uint32_t logicalW, uint32_t logicalH);
// Dentro do passe aberto (cor + depth/stencil com stencil limpo). preRotationDeg: 0/90/180/270 (S-05).
void render(Cmd* cmd, uint32_t frameIndex, uint32_t targetW, uint32_t targetH, uint32_t preRotationDeg);

// Toque (coordenadas da janela, orientação lógica). action: 0 = down, 1 = move, 2 = up, 3 = cancel.
void touch(int action, int64_t pointerId, float x, float y);
// Estado do teclado do sistema (GameTextInput): texto completo, seleção e composição em caracteres.
void textInput(const char* textUtf8, int selStart, int selEnd, int composeStart, int composeEnd);
void editorDone();

// Medições do S-07 (no log, prefixo "S07:").
void runInspectorBenchmark(int fieldCount);
} // namespace UiSpike
