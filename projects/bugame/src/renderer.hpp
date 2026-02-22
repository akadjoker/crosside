#pragma once

#include <vector>
#include <raylib.h>

class Interpreter;
 

struct RenderPass
{
    int   shaderId   = -1;
    bool  shouldClear = false;
    Color clearColor  = { 0, 0, 0, 0 };
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void init();
    void shutdown();
    void onWindowResize();
    void renderFrame(Interpreter* vm, float dt);

    // Post-Processing toggle
    void enablePostProcessing(bool enabled);
    bool isPostProcessingEnabled() const;

    // Pass API (devolve index, -1 se falhar)
    int  createPass();
    void setPassShader(int passId, int shaderId);
    void setPassClear(int passId, bool clear, Color color = {0, 0, 0, 0});
    void removePass(int passId);
    void clearAllPasses();

private:
    void drawSceneContent(Interpreter* vm, float dt);
    void drawPostProcessingPath();
    bool isValidPass(int passId) const;

    // State
    bool m_postProcessingEnabled = false;

    // Textures (só existem quando post-processing está ativo)
    RenderTexture2D m_sceneTexture    = {};
    RenderTexture2D m_pingPongTexture = {};

    // Pass pool
    static constexpr int MAX_PASSES = 16;
    RenderPass m_passes[MAX_PASSES]      = {};
    bool       m_passActive[MAX_PASSES]  = {};
};