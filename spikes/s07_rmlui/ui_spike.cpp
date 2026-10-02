// Spike S-07: Astra UI (RmlUi 6.3) no app de teste. Ver ui_spike.h.

#include "ui_spike.h"

#include "forge_render_interface.h"

#include "Common_3/Utilities/Interfaces/ILog.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/TextInputContext.h>
#include <RmlUi/Core/TextInputHandler.h>

#include <chrono>
#include <string>

namespace
{
using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

class SystemInterface final : public Rml::SystemInterface
{
public:
    Clock::time_point start = Clock::now();
    UiKeyboardBridge  keyboard;

    double GetElapsedTime() override { return std::chrono::duration<double>(Clock::now() - start).count(); }
    bool   LogMessage(Rml::Log::Type type, const Rml::String& message) override
    {
        LOGF(type <= Rml::Log::LT_WARNING ? eWARNING : eINFO, "S07 RmlUi: %s", message.c_str());
        return true;
    }
};

// Ponte IME: o teclado do sistema (GameTextInput) é a fonte da verdade enquanto um campo está focado.
class TextInputHandler final : public Rml::TextInputHandler
{
public:
    Rml::TextInputContext* active = nullptr;
    int                    charCount = 0;          // caracteres no campo, segundo o último estado aplicado
    bool                   pendingShow = false;    // OnActivate roda antes de o foco mudar: abre o teclado no update

    void OnActivate(Rml::TextInputContext* ctx) override;
    void OnDeactivate(Rml::TextInputContext* ctx) override
    {
        if (ctx == active)
        {
            active = nullptr;
            if (sSystem.keyboard.hide)
                sSystem.keyboard.hide();
        }
    }
    void OnDestroy(Rml::TextInputContext* ctx) override
    {
        if (ctx == active)
            active = nullptr;
    }

    static SystemInterface sSystem;
};
SystemInterface TextInputHandler::sSystem;

int utf8Chars(const std::string& s)
{
    int n = 0;
    for (unsigned char c : s)
        n += (c & 0xC0) != 0x80;
    return n;
}

ForgeRenderInterface gRenderer;
TextInputHandler     gTextInput;
Rml::Context*        gContext = nullptr;
Rml::ElementDocument* gPanel = nullptr;

// Rolagem inercial: posição ao soltar e checagem depois de 1 s.
float             gScrollAtRelease = 0.0f;
Clock::time_point gReleaseTime;
bool              gWatchInertia = false;

// Custo de CPU da UI por frame (update + render), resumido a cada 10 s.
double            gUiMsAccum = 0.0;
uint32_t          gUiFrames = 0;
Clock::time_point gReportTime = Clock::now();

void TextInputHandler::OnActivate(Rml::TextInputContext* ctx)
{
    active = ctx;
    pendingShow = true;
}

// Chamado no update, quando o elemento focado já é o campo: entrega ao teclado o texto e a seleção atuais.
void showKeyboardForFocusedField()
{
    gTextInput.pendingShow = false;
    Rml::Element* focus = gContext ? gContext->GetFocusElement() : nullptr;
    if (!gTextInput.active || !focus)
        return;
    const std::string value = focus->GetAttribute<Rml::String>("value", "");
    gTextInput.charCount = utf8Chars(value);
    int start = 0, end = 0;
    gTextInput.active->GetSelectionRange(start, end);
    LOGF(eINFO, "S07: campo focado com '%s' (seleção [%d,%d)); abrindo teclado", value.c_str(), start, end);
    if (TextInputHandler::sSystem.keyboard.show)
        TextInputHandler::sSystem.keyboard.show(value.c_str(), start, end);
}

// Tokens da Astra 2 (doc 17 §3) como variáveis RCSS (recurso da RmlUi 6.3).
const char* kTokens = R"(
body {
    --bg-canvas: #0B0C0E; --bg-panel: #15171B; --bg-raised: #1B1E23; --bg-field: #2A2F36;
    --line-default: #343A42; --text-primary: #F2F4F7; --text-secondary: #9AA3AE; --text-tertiary: #7D8692;
    --accent: #CAFB04; --text-on-accent: #0B0C0E; --row: 40dp;
    font-family: AstraUI; font-size: 13dp; color: var(--text-primary);
}
)";

const char* kPanelRml = R"(<rml>
<head>
<style>
%TOKENS%
body { left: 0; top: 0; width: 360dp; height: 100%; background-color: var(--bg-panel); border-right: 1dp var(--line-default); }
#cabecalho { display: block; height: 44dp; padding: 12dp 16dp; background-color: var(--bg-raised); }
#titulo { font-size: 11dp; letter-spacing: 1dp; color: var(--text-secondary); }
#play { position: absolute; right: 16dp; top: 8dp; width: 64dp; height: 28dp; line-height: 28dp; text-align: center;
        border-radius: 6dp; background-color: var(--accent); color: var(--text-on-accent); }
#play:active { background-color: #E1FF5C; }
#cartao { display: block; margin: 12dp; padding: 12dp; border-radius: 10dp; background-color: var(--bg-raised);
          overflow: hidden; }
#faixa { display: block; height: 8dp; margin: -12dp -12dp 8dp -12dp; background-color: var(--accent); transform: rotate(-3deg); }
input.text { display: block; width: 100%; height: 32dp; padding: 6dp 8dp; box-sizing: border-box; border-radius: 6dp;
             background-color: var(--bg-field); color: var(--text-primary); }
input.text:focus { border: 2dp var(--accent); }
#lista { display: block; position: absolute; top: 200dp; bottom: 0; left: 0; right: 0; overflow-y: auto; }
.linha { display: block; height: var(--row); line-height: var(--row); padding: 0 16dp; border-bottom: 1dp #24282E; }
.linha:hover { background-color: rgba(202, 251, 4, 0.10); }
scrollbarvertical { width: 4dp; }
scrollbarvertical sliderbar { background-color: var(--line-default); border-radius: 2dp; }
</style>
</head>
<body>
<div id="cabecalho"><span id="titulo">HIERARQUIA</span><div id="play">PLAY</div></div>
<div id="cartao"><div id="faixa"></div>Nome do objeto<br/><input type="text" class="text" id="nome" value="Porta"/></div>
<div id="lista">%ROWS%</div>
</body>
</rml>)";

std::string replaceAll(std::string s, const std::string& from, const std::string& to)
{
    for (size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + to.size()))
        s.replace(p, from.size(), to);
    return s;
}
} // namespace

namespace UiSpike
{
bool init(Renderer* renderer, uint32_t colorFormat, uint32_t depthStencilFormat, const char* fontPath, float dpRatio,
          const UiKeyboardBridge& keyboard)
{
    if (!gRenderer.init(renderer, colorFormat, depthStencilFormat))
    {
        LOGF(eERROR, "S07: RenderInterface não inicializou");
        return false;
    }
    TextInputHandler::sSystem.keyboard = keyboard;
    Rml::SetSystemInterface(&TextInputHandler::sSystem);
    Rml::SetRenderInterface(&gRenderer);
    Rml::SetTextInputHandler(&gTextInput);
    Rml::Initialise();
    if (!Rml::LoadFontFace(fontPath, "AstraUI", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, true))
    {
        LOGF(eERROR, "S07: fonte '%s' não carregou", fontPath);
        return false;
    }
    gContext = Rml::CreateContext("astra", Rml::Vector2i(1, 1));
    gContext->SetDensityIndependentPixelRatio(dpRatio);

    std::string rows;
    for (int i = 0; i < 200; ++i)
        rows += "<div class=\"linha\">Entidade " + std::to_string(i) + "</div>";
    std::string rml = replaceAll(replaceAll(kPanelRml, "%TOKENS%", kTokens), "%ROWS%", rows);
    auto t0 = Clock::now();
    gPanel = gContext->LoadDocumentFromMemory(rml, "painel.rml");
    if (!gPanel)
        return false;
    gPanel->Show();
    LOGF(eINFO, "S07: painel carregado em %.2f ms (dp ratio %.2f, fonte %s)", msSince(t0), dpRatio, fontPath);
    return true;
}

void exit()
{
    Rml::Shutdown();
    gRenderer.exit();
    gContext = nullptr;
    gPanel = nullptr;
}

void update(uint32_t logicalW, uint32_t logicalH)
{
    if (!gContext)
        return;
    if (gContext->GetDimensions() != Rml::Vector2i((int)logicalW, (int)logicalH))
        gContext->SetDimensions(Rml::Vector2i((int)logicalW, (int)logicalH));
    auto t0 = Clock::now();
    gContext->Update();
    gUiMsAccum += msSince(t0);
    if (gTextInput.pendingShow)
        showKeyboardForFocusedField();

    if (gWatchInertia && msSince(gReleaseTime) > 1000.0)
    {
        gWatchInertia = false;
        const float now = gPanel->GetElementById("lista")->GetScrollTop();
        LOGF(eINFO, "S07: rolagem inercial: %.0f px depois de soltar o dedo (posição %.0f -> %.0f)", now - gScrollAtRelease,
             gScrollAtRelease, now);
    }
}

void render(Cmd* cmd, uint32_t frameIndex, uint32_t targetW, uint32_t targetH, uint32_t preRotationDeg)
{
    if (!gContext)
        return;
    auto t0 = Clock::now();
    gRenderer.beginFrame(cmd, frameIndex, targetW, targetH, preRotationDeg);
    gContext->Render();
    gRenderer.endFrame();
    gUiMsAccum += msSince(t0);
    ++gUiFrames;
    if (msSince(gReportTime) >= 10000.0)
    {
        LOGF(eINFO, "S07: UI na CPU %.3f ms/frame (update + render, média de %u frames), %u draws, %u operações de máscara",
             gUiMsAccum / gUiFrames, gUiFrames, gRenderer.drawsLastFrame(), gRenderer.maskOpsLastFrame());
        gUiMsAccum = 0.0;
        gUiFrames = 0;
        gReportTime = Clock::now();
    }
}

void touch(int action, int64_t pointerId, float x, float y)
{
    if (!gContext)
        return;
    Rml::TouchList touches = { Rml::Touch{ (Rml::TouchId)pointerId, Rml::Vector2f(x, y) } };
    switch (action)
    {
    case 0:
        gContext->ProcessTouchStart(touches, 0);
        break;
    case 1:
        gContext->ProcessTouchMove(touches, 0);
        break;
    case 2:
        gContext->ProcessTouchEnd(touches, 0);
        gScrollAtRelease = gPanel->GetElementById("lista")->GetScrollTop();
        gReleaseTime = Clock::now();
        gWatchInertia = true;
        break;
    default:
        gContext->ProcessTouchCancel(touches);
        break;
    }
}

void textInput(const char* textUtf8, int selStart, int selEnd, int composeStart, int composeEnd)
{
    Rml::TextInputContext* ctx = gTextInput.active;
    if (!ctx)
        return;
    const std::string text = textUtf8;
    ctx->SetText(text, 0, gTextInput.charCount);
    gTextInput.charCount = utf8Chars(text);
    if (composeStart >= 0 && composeEnd > composeStart)
        ctx->SetCompositionRange(composeStart, composeEnd);
    else
        ctx->SetCompositionRange(0, 0);
    ctx->SetSelectionRange(selStart, selEnd);
    LOGF(eINFO, "S07: campo recebeu '%s' (seleção [%d,%d), composição [%d,%d))", text.c_str(), selStart, selEnd, composeStart,
         composeEnd);
}

void editorDone()
{
    if (Rml::Element* focus = gContext ? gContext->GetFocusElement() : nullptr)
    {
        LOGF(eINFO, "S07: Concluir; valor final do campo '%s'", focus->GetAttribute<Rml::String>("value", "").c_str());
        focus->Blur();
    }
}

void runInspectorBenchmark(int fieldCount)
{
    // Inspector gerado: cartões de componente com linhas rótulo + campo (doc 18 §6).
    std::string rows;
    for (int i = 0; i < fieldCount; ++i)
    {
        if (i % 12 == 0)
            rows += "<div class=\"cartao\"><div class=\"cab\">COMPONENTE " + std::to_string(i / 12) + "</div>";
        rows += "<div class=\"prop\"><span class=\"rotulo\">Propriedade " + std::to_string(i) +
                "</span><input type=\"text\" class=\"valor\" id=\"p" + std::to_string(i) + "\" value=\"" + std::to_string(i * 0.25) +
                "\"/></div>";
        if (i % 12 == 11 || i == fieldCount - 1)
            rows += "</div>";
    }
    std::string rml = std::string("<rml><head><style>") + kTokens + R"(
body { left: 0; top: 0; width: 340dp; height: 100%; background-color: var(--bg-panel); overflow-y: auto; }
.cartao { display: block; margin: 8dp; border-radius: 10dp; background-color: var(--bg-raised); padding-bottom: 4dp; }
.cab { display: block; height: 32dp; line-height: 32dp; padding: 0 12dp; font-size: 11dp; color: var(--text-secondary); }
.prop { display: flex; height: var(--row); align-items: center; padding: 0 12dp; }
.rotulo { flex: 1; color: var(--text-secondary); }
.valor { width: 120dp; height: 28dp; padding: 4dp 8dp; border-radius: 6dp; background-color: var(--bg-field); color: var(--text-primary); }
</style></head><body>)" + rows + "</body></rml>";

    auto                  t0 = Clock::now();
    Rml::ElementDocument* doc = gContext->LoadDocumentFromMemory(rml, "inspector.rml");
    const double          loadMs = msSince(t0);
    t0 = Clock::now();
    doc->Show();
    gContext->Update(); // estilo + layout
    const double layoutMs = msSince(t0);

    // Atualizar um campo (o que acontece ao arrastar um valor): set + update.
    double updateMs = 0.0;
    for (int i = 0; i < 60; ++i)
    {
        Rml::Element* field = doc->GetElementById("p" + std::to_string(i));
        t0 = Clock::now();
        field->SetAttribute("value", std::to_string(i * 1.5));
        gContext->Update();
        updateMs += msSince(t0);
    }
    LOGF(eINFO, "S07: Inspector %d campos: carregar %.2f ms + estilo/layout %.2f ms = %.2f ms (orçamento 16 ms); atualizar 1 campo %.3f ms (orçamento 1 ms)",
         fieldCount, loadMs, layoutMs, loadMs + layoutMs, updateMs / 60.0);
    doc->Close();
    gContext->Update();
}
} // namespace UiSpike
