#pragma once


#include "engine.hpp"
#include "interpreter.hpp"
#include "bindings.hpp"
#include "platform.hpp"
#include <raylib.h>

 

// ============================================================================
// ENUMS
// ============================================================================

enum ScreenScaleMode
{
    SCALE_NONE = 0,      // Sem escala, 1:1 pixel perfect
    SCALE_FIT = 1,       // Mantém aspecto ratio, barras pretas
    SCALE_STRETCH = 2,   // Estica para preencher (distorce)
    SCALE_FILL = 3,      // Preenche tela, corta bordas
    SCALE_LETTERBOX = 4  // Igual ao FIT com barras explícitas
};

// ============================================================================
// CAMERA SHAKE STATE
// ============================================================================

struct CameraShakeState
{
    bool active = false;
    float amplitudeX = 0.0f;
    float amplitudeY = 0.0f;
    float cycles = 0.0f;
    float omega = 0.0f;
    float cyclesLeft = 0.0f;
};

// ============================================================================
// CAMERA MANAGER CLASS
// ============================================================================

class CameraManager
{
public:
    // Construtor e Destrutor
    CameraManager();
    ~CameraManager();


    void setVirtualScreenEnabled(bool enabled);
    bool isVirtualScreenEnabled() const;


    // Inicialização
    void init(int designWidth, int designHeight);
    void update(float deltaTime);

    // Câmera básica
    void setZoom(float zoom);
    void setRotation(float rotation);
    void setTarget(float x, float y);
    void setOffset(float x, float y);
    void centerCamera(float x, float y);

    // Getters
    float getZoom() const;
    float getRotation() const;
    Vector2 getTarget() const;
    Vector2 getOffset() const;
    Camera2D& getCamera();
    const Camera2D& getCamera() const;

    // Screen scaling
    void setScreenScaleMode(ScreenScaleMode mode);
    void setScreenScaleMode(const char* modeStr);
    ScreenScaleMode getScreenScaleMode() const;
    const char* getScreenScaleModeString() const;

    // Design resolution
    void setDesignResolution(int width, int height);
    void getDesignResolution(int& outWidth, int& outHeight) const;

    // Viewport
    Rectangle getViewport() const;
    float getFitScale() const;

    // Camera shake
    void startShake(float amplitudeX, float amplitudeY, float frequency, float durationCycles);
    void stopShake();

    Vector2 getCameraSize() const;
    
    // Helpers
    void onWindowResize();
    void begin();
    void end();

private:
    // Estado interno
    Camera2D camera;
    CameraShakeState shakeState;
    
    Vector2 userOffset;
    Vector2 shakeOffset;
    
    float baseZoom;
    bool targetSet;
    
    ScreenScaleMode scaleMode;
    int designWidth;
    int designHeight;
    
    Rectangle viewport;
    float fitScale;
    bool useVirtualScreen; 

    // Funções privadas
    void recalcViewport();
    void applyCamera();
    void updateShake(float deltaTime);
};


 void registerCameraNatives(Interpreter &vm);