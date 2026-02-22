#include "renderer.hpp"
#include "engine.hpp"
#include "bindings.hpp"
#include "camera.hpp"
#include "interpreter.hpp"

extern Scene gScene;
extern ParticleSystem gParticleSystem;
extern CameraManager gCamera;
extern Color BACKGROUND_COLOR;
Renderer gRenderer;


Renderer::Renderer() {}
Renderer::~Renderer() {}

void Renderer::init()
{
    m_postProcessingEnabled = false;
    // Texturas só alocadas quando post-processing for ativado
}

void Renderer::shutdown()
{
    if (m_postProcessingEnabled)
    {
        UnloadRenderTexture(m_sceneTexture);
        UnloadRenderTexture(m_pingPongTexture);
        m_postProcessingEnabled = false;
    }
}

void Renderer::onWindowResize()
{
    if (m_postProcessingEnabled)
    {
        UnloadRenderTexture(m_sceneTexture);
        UnloadRenderTexture(m_pingPongTexture);
        m_sceneTexture    = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        m_pingPongTexture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    }
}

// ============================================================
//  Post-Processing Toggle
// ============================================================

void Renderer::enablePostProcessing(bool enabled)
{
    if (m_postProcessingEnabled == enabled) return;

    m_postProcessingEnabled = enabled;

    if (enabled)
    {
        m_sceneTexture    = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        m_pingPongTexture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    }
    else
    {
        UnloadRenderTexture(m_sceneTexture);
        UnloadRenderTexture(m_pingPongTexture);
        m_sceneTexture    = {};
        m_pingPongTexture = {};
    }
}

bool Renderer::isPostProcessingEnabled() const
{
    return m_postProcessingEnabled;
}

// ============================================================
//  Pass API
// ============================================================

bool Renderer::isValidPass(int passId) const
{
    return passId >= 0 && passId < MAX_PASSES && m_passActive[passId];
}

int Renderer::createPass()
{
    for (int i = 0; i < MAX_PASSES; i++)
    {
        if (!m_passActive[i])
        {
            m_passes[i]     = RenderPass{};
            m_passActive[i] = true;
            return i;
        }
    }
    TraceLog(LOG_ERROR, "Renderer: MAX_PASSES (%d) reached", MAX_PASSES);
    return -1;
}

void Renderer::setPassShader(int passId, int shaderId)
{
    if (!isValidPass(passId)) return;
    m_passes[passId].shaderId = shaderId;
}

void Renderer::setPassClear(int passId, bool clear, Color color)
{
    if (!isValidPass(passId)) return;
    m_passes[passId].shouldClear = clear;
    m_passes[passId].clearColor  = color;
}

void Renderer::removePass(int passId)
{
    if (passId < 0 || passId >= MAX_PASSES) return;
    m_passActive[passId] = false;
    m_passes[passId]     = RenderPass{};
}

void Renderer::clearAllPasses()
{
    for (int i = 0; i < MAX_PASSES; i++)
    {
        m_passActive[i] = false;
        m_passes[i]     = RenderPass{};
    }
}

// ============================================================
//  Render Interno
// ============================================================

void Renderer::drawSceneContent(Interpreter* vm, float dt)
{
    gCamera.begin();
    BindingsDraw::resetDrawCommands();
    vm->update(dt);
    RenderScene();
    gParticleSystem.cleanup();
    gParticleSystem.draw();
    BindingsBox2D::renderDebug();
    gCamera.end();
}

void Renderer::drawPostProcessingPath()
{
    // Recolher passes ativos em ordem de criação
    int active[MAX_PASSES];
    int activeCount = 0;
    for (int i = 0; i < MAX_PASSES; i++)
        if (m_passActive[i]) active[activeCount++] = i;

    BeginDrawing();
    ClearBackground(BLACK);

    if (activeCount == 0)
    {
        // Post-processing ativo mas sem passes — desenha textura direta
        Rectangle src  = { 0, 0,  (float)m_sceneTexture.texture.width,
                                  -(float)m_sceneTexture.texture.height };
        Rectangle dst  = { 0, 0,  (float)GetScreenWidth(),
                                   (float)GetScreenHeight() };
        DrawTexturePro(m_sceneTexture.texture, src, dst, {0, 0}, 0.0f, WHITE);
    }
    else
    {
        RenderTexture2D* source = &m_sceneTexture;
        RenderTexture2D* dest   = &m_pingPongTexture;

        for (int i = 0; i < activeCount; i++)
        {
            const RenderPass& pass = m_passes[active[i]];
            bool isLast = (i == activeCount - 1);

            if (!isLast) BeginTextureMode(*dest);
            if (pass.shouldClear) ClearBackground(pass.clearColor);

            Shader* shader = BindingsDraw::getLoadedShader(pass.shaderId);
            if (shader) BeginShaderMode(*shader);

            Rectangle src = { 0, 0,  (float)source->texture.width,
                                     -(float)source->texture.height };
            Rectangle dst = isLast
                ? Rectangle{ 0, 0, (float)GetScreenWidth(),  (float)GetScreenHeight() }
                : Rectangle{ 0, 0, (float)dest->texture.width, (float)dest->texture.height };

            DrawTexturePro(source->texture, src, dst, {0, 0}, 0.0f, WHITE);

            if (shader) EndShaderMode();
            if (!isLast) { EndTextureMode(); std::swap(source, dest); }
        }
    }

    // UI sempre por cima, no ecrã final
    BindingsDraw::RenderScreenCommands();
    BindingsInput::drawVirtualKeys();
    DrawFade();
    EndDrawing();
}

// ============================================================
//  Frame Principal
// ============================================================

void Renderer::renderFrame(Interpreter* vm, float dt)
{
    if (m_postProcessingEnabled)
    {
        BeginTextureMode(m_sceneTexture);
            ClearBackground(BACKGROUND_COLOR);
            drawSceneContent(vm, dt);
        EndTextureMode();

        drawPostProcessingPath();
    }
    else
    {
        // Default: render direto, zero overhead
        BeginDrawing();
            ClearBackground(BACKGROUND_COLOR);
            drawSceneContent(vm, dt);
            BindingsDraw::RenderScreenCommands();
            BindingsInput::drawVirtualKeys();
            DrawFade();
        EndDrawing();
    }
}

// ============================================================
//  Bindings
// ============================================================

namespace BindingsPostProcessing
{
    int native_enable_post_processing(Interpreter* vm, int argCount, Value* args)
    {
        if (argCount != 1) { Error("enable_post_processing(bool)"); return 0; }
        gRenderer.enablePostProcessing(args[0].asBool());
        return 0;
    }

    int native_is_post_processing_enabled(Interpreter* vm, int argCount, Value* args)
    {
        vm->pushBool(gRenderer.isPostProcessingEnabled());
        return 1;
    }

    int native_create_pass(Interpreter* vm, int argCount, Value* args)
    {
        vm->pushInt(gRenderer.createPass());
        return 1;
    }

    int native_set_pass_shader(Interpreter* vm, int argCount, Value* args)
    {
        if (argCount != 2) { Error("set_pass_shader(passId, shaderId)"); return 0; }
        gRenderer.setPassShader((int)args[0].asNumber(), (int)args[1].asNumber());
        return 0;
    }

    int native_set_pass_clear(Interpreter* vm, int argCount, Value* args)
    {
        // set_pass_clear(passId, bool)
        // set_pass_clear(passId, bool, r, g, b)
        if (argCount < 2) { Error("set_pass_clear(passId, bool, [r,g,b])"); return 0; }
        Color c = {0, 0, 0, 0};
        if (argCount >= 5)
            c = { (unsigned char)args[2].asNumber(),
                  (unsigned char)args[3].asNumber(),
                  (unsigned char)args[4].asNumber(), 255 };
        gRenderer.setPassClear((int)args[0].asNumber(), args[1].asBool(), c);
        return 0;
    }

    int native_remove_pass(Interpreter* vm, int argCount, Value* args)
    {
        if (argCount != 1) { Error("remove_pass(passId)"); return 0; }
        gRenderer.removePass((int)args[0].asNumber());
        return 0;
    }

    int native_clear_all_passes(Interpreter* vm, int argCount, Value* args)
    {
        gRenderer.clearAllPasses();
        return 0;
    }

    void registerAll(Interpreter& vm)
    {
        vm.registerNative("enable_post_processing",     native_enable_post_processing,1);
        vm.registerNative("is_post_processing_enabled", native_is_post_processing_enabled,0);
        vm.registerNative("create_pass",                native_create_pass,0);
        vm.registerNative("set_pass_shader",            native_set_pass_shader,2);
        vm.registerNative("set_pass_clear",             native_set_pass_clear,5);
        vm.registerNative("remove_pass",                native_remove_pass,1);
        vm.registerNative("clear_all_passes",           native_clear_all_passes,0);
    }
}
 