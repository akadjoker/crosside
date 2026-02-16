#include "camera.hpp"

#include <cmath>
#include <cstring>

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

CameraManager gCamera;

int native_set_camera_zoom(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1 )
    {
        Error("set_camera_zoom expects 1 number argument (zoom)");
    }
    float zoom = (float)args[0].asNumber();
    gCamera.setZoom(zoom);

    return 0;
}

int native_set_camera_rotation(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1)
    {
        Error("set_camera_rotation expects 1 number argument (rotation)");
        return 0;
    }
    float rotation = (float)args[0].asNumber();
    gCamera.setRotation(rotation);

    return 0;
}

int native_set_camera_target(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 2)
    {
        Error("set_camera_target expects 2 number arguments (x, y)");
        return 0;
    }
    float x = (float)args[0].asNumber();
    float y = (float)args[1].asNumber();
    gCamera.setTarget(x, y);

    return 0;
}

int native_set_camera_offset(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 2)
    {
        Error("set_camera_offset expects 2 number arguments (offsetX, offsetY)");
        return 0;
    }
    float offsetX = (float)args[0].asNumber();
    float offsetY = (float)args[1].asNumber();
    gCamera.setOffset(offsetX, offsetY);

    return 0;
}

int native_get_camera_zoom(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("get_camera_zoom expects no arguments");
        vm->pushDouble(1.0);
        return 1;
    }
    vm->pushDouble(gCamera.getZoom());

    return 1;
}

int native_get_camera_rotation(Interpreter *vm, int argCount, Value *args)
{

    if (argCount != 0)
    {
        Error("get_camera_rotation expects no arguments");
        vm->pushDouble(0.0);
        return 1;
    }
    vm->pushDouble(gCamera.getRotation());
    return 1;
}

int native_get_camera_target(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("get_camera_target expects no arguments");
        vm->pushDouble(0.0);
        vm->pushDouble(0.0);
        return 2;
    }
    Vector2 target = gCamera.getTarget();
    vm->pushDouble(target.x);
    vm->pushDouble(target.y);

    return 2;
}

int native_get_camera_offset(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("get_camera_offset expects no arguments");
        vm->pushDouble(0.0);
        vm->pushDouble(0.0);
        return 2;
    }
    Vector2 offset = gCamera.getOffset();
    vm->pushDouble(offset.x);
    vm->pushDouble(offset.y);

    return 2;
}

int native_get_camera_x(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("get_camera_x expects no arguments");
        vm->pushDouble(0.0);
        return 1;
    }
    vm->pushDouble(gCamera.getTarget().x);
    
    return 1;
}

int native_get_camera_y(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("get_camera_y expects no arguments");
        vm->pushDouble(0.0);
        return 1;
    }
    vm->pushDouble(gCamera.getTarget().y);

    return 1;
}

 

int native_set_design_resolution(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 2)
    {
        Error("set_design_resolution expects 2 number arguments (width, height)");
        return 0;
    }
    int width = (int)args[0].asNumber();
    int height = (int)args[1].asNumber();
    gCamera.setDesignResolution(width, height);

    return 0;
}
 
int native_get_viewport(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("get_viewport expects no arguments");
        vm->pushDouble(0.0);
        vm->pushDouble(0.0);
        vm->pushDouble(0.0);
        vm->pushDouble(0.0);    
    }
    Rectangle vp = gCamera.getViewport();
    vm->pushDouble(vp.x);
    vm->pushDouble(vp.y);
    vm->pushDouble(vp.width);
    vm->pushDouble(vp.height);

    return 4;
}
int native_get_fit_scale(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("get_fit_scale expects no arguments");
        vm->pushDouble(1.0);
        return 1;
    }
    vm->pushDouble(gCamera.getFitScale());

    return 1;
}
 
int native_start_camera_shake(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 4)
    {
        Error("start_camera_shake expects 4 arguments ( ampX, ampY, freq, duration)");
    }

    gCamera.startShake(
        (float)args[0].asNumber(), // ampX
        (float)args[1].asNumber(), // ampY
        (float)args[2].asNumber(), // freq
        (float)args[3].asNumber()  // duration
    );

    return 0;
}
int native_stop_camera_shake(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 0)
    {
        Error("stop_camera_shake expects no arguments");
    }
    gCamera.stopShake();

    return 0;
}

int native_set_screen_scale_mode(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1)
    {
        Error("set_screen_scale_mode expects 1 number argument (scaleMode)");
        return 0;
    }
    int mode = (int)args[0].asNumber();
    gCamera.setScreenScaleMode((ScreenScaleMode)mode);
    return 0;
}

int native_set_virtual_screen_enabled(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1)
    {
        Error("set_virtual_screen_enabled expects 1 boolean argument (enabled)");
        return 0;
    }
    bool enabled = args[0].asBool();
    gCamera.setVirtualScreenEnabled(enabled);
    return 0;
}


void registerCameraNatives(Interpreter &vm)
{
    vm.registerNative("set_camera_zoom", native_set_camera_zoom, 1);
    vm.registerNative("set_camera_rotation", native_set_camera_rotation, 1);
    vm.registerNative("set_camera_target", native_set_camera_target, 2);
    vm.registerNative("set_camera_offset", native_set_camera_offset, 2);
    vm.registerNative("get_camera_zoom", native_get_camera_zoom, 0);
    vm.registerNative("get_camera_rotation", native_get_camera_rotation, 0);
    vm.registerNative("get_camera_target", native_get_camera_target, 0);
    vm.registerNative("get_camera_offset", native_get_camera_offset, 0);
    vm.registerNative("get_camera_x", native_get_camera_x, 0);
    vm.registerNative("get_camera_y", native_get_camera_y, 0);
    
    vm.registerNative("get_viewport", native_get_viewport, 0);
    vm.registerNative("get_fit_scale", native_get_fit_scale, 0);
    
    vm.registerNative("start_camera_shake", native_start_camera_shake, 4);
    vm.registerNative("stop_camera_shake", native_stop_camera_shake, 0);
    vm.registerNative("set_screen_scale_mode", native_set_screen_scale_mode, 1);
    vm.registerNative("set_design_resolution", native_set_design_resolution, 2);
    vm.registerNative("set_virtual_screen_enabled", native_set_virtual_screen_enabled, 1);
}

CameraManager::CameraManager()
    : camera{0}, shakeState{0}, userOffset{0, 0}, shakeOffset{0, 0}, baseZoom(1.0f), targetSet(false), scaleMode(SCALE_NONE), designWidth(1280), designHeight(720), viewport{0, 0, 1280, 720}, fitScale(1.0f), useVirtualScreen(true) // ⭐ Padrão: usar virtual screen
{
}

void CameraManager::setVirtualScreenEnabled(bool enabled)
{
    useVirtualScreen = enabled;
    recalcViewport();
    applyCamera();
}

bool CameraManager::isVirtualScreenEnabled() const
{
    return useVirtualScreen;
}

void CameraManager::recalcViewport()
{
    const float winW = (float)std::max(1, GetScreenWidth());
    const float winH = (float)std::max(1, GetScreenHeight());

 
    if (!useVirtualScreen)
    {
        fitScale = 1.0f;
        viewport = {0.0f, 0.0f, winW, winH};
        return;
    }

 
    float sx = winW / (float)designWidth;
    float sy = winH / (float)designHeight;

    switch (scaleMode)
    {
    case SCALE_NONE:
    {
        // NONE com virtual screen = mostrar design resolution sem escala
        fitScale = 1.0f;
        viewport = {0.0f, 0.0f, (float)designWidth, (float)designHeight};
        break;
    }

    case SCALE_FIT:
    {
        fitScale = std::min(sx, sy);
        if (fitScale <= 0.0f)
            fitScale = 1.0f;

        float vpW = (float)designWidth * fitScale;
        float vpH = (float)designHeight * fitScale;
        viewport = {
            floorf((winW - vpW) * 0.5f),
            floorf((winH - vpH) * 0.5f),
            vpW,
            vpH};
        break;
    }

    case SCALE_STRETCH:
    {
        // Estica virtual screen para preencher janela
        fitScale = 1.0f;
        viewport = {0.0f, 0.0f, winW, winH};
        break;
    }

    case SCALE_FILL:
    {
        fitScale = std::max(sx, sy);
        if (fitScale <= 0.0f)
            fitScale = 1.0f;

        float vpW = (float)designWidth * fitScale;
        float vpH = (float)designHeight * fitScale;
        viewport = {
            floorf((winW - vpW) * 0.5f),
            floorf((winH - vpH) * 0.5f),
            vpW,
            vpH};
        break;
    }

    case SCALE_LETTERBOX:
    {
        fitScale = std::min(sx, sy);
        if (fitScale <= 0.0f)
            fitScale = 1.0f;

        float vpW = (float)designWidth * fitScale;
        float vpH = (float)designHeight * fitScale;
        viewport = {
            floorf((winW - vpW) * 0.5f),
            floorf((winH - vpH) * 0.5f),
            vpW,
            vpH};
        break;
    }
    }
}

void CameraManager::applyCamera()
{
   
    if (!useVirtualScreen)
    {
        camera.zoom = baseZoom;
        float winW = (float)GetScreenWidth();
        float winH = (float)GetScreenHeight();
        camera.offset.x = winW * 0.5f + userOffset.x + shakeOffset.x;
        camera.offset.y = winH * 0.5f + userOffset.y + shakeOffset.y;
        return;
    }

 
    switch (scaleMode)
    {
    case SCALE_NONE:
    {
        camera.zoom = baseZoom;
        camera.offset.x = (float)designWidth * 0.5f + userOffset.x + shakeOffset.x;
        camera.offset.y = (float)designHeight * 0.5f + userOffset.y + shakeOffset.y;
        break;
    }

    case SCALE_STRETCH:
    {
        camera.zoom = baseZoom;
        camera.offset.x = viewport.width * 0.5f + userOffset.x + shakeOffset.x;
        camera.offset.y = viewport.height * 0.5f + userOffset.y + shakeOffset.y;
        break;
    }

    default: // SCALE_FIT, SCALE_FILL, SCALE_LETTERBOX
    {
        camera.zoom = baseZoom * fitScale;
        camera.offset.x = viewport.x + viewport.width * 0.5f + userOffset.x + shakeOffset.x;
        camera.offset.y = viewport.y + viewport.height * 0.5f + userOffset.y + shakeOffset.y;
        break;
    }
    }
}

Vector2 CameraManager::getCameraSize() const
{
    if (useVirtualScreen)
    {
        return {(float)designWidth, (float)designHeight};
    }
    else
    {
        return {(float)GetScreenWidth(), (float)GetScreenHeight()};
    }
}

CameraManager::~CameraManager()
{
}

void CameraManager::init(int width, int height)
{
    designWidth = width;
    designHeight = height;

    camera.target = {(float)width * 0.5f, (float)height * 0.5f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    recalcViewport();
    applyCamera();
}

void CameraManager::update(float deltaTime)
{
    updateShake(deltaTime);
    applyCamera();
}

// ============================================================================
// CÂMERA BÁSICA
// ============================================================================

void CameraManager::setZoom(float zoom)
{
    baseZoom = (zoom <= 0.0f) ? 0.001f : zoom;
    applyCamera();
}

void CameraManager::setRotation(float rotation)
{
    camera.rotation = rotation;
}

void CameraManager::setTarget(float x, float y)
{
    camera.target.x = x;
    camera.target.y = y;
    targetSet = true;
}

void CameraManager::setOffset(float x, float y)
{
    userOffset.x = x;
    userOffset.y = y;
    applyCamera();
}

void CameraManager::centerCamera(float x, float y)
{
    camera.target.x = x;
    camera.target.y = y;
    userOffset = {0.0f, 0.0f};
    targetSet = true;
    applyCamera();
}

// ============================================================================
// GETTERS
// ============================================================================

float CameraManager::getZoom() const
{
    return camera.zoom;
}

float CameraManager::getRotation() const
{
    return camera.rotation;
}

Vector2 CameraManager::getTarget() const
{
    return camera.target;
}

Vector2 CameraManager::getOffset() const
{
    return userOffset;
}

Camera2D &CameraManager::getCamera()
{
    return camera;
}

const Camera2D &CameraManager::getCamera() const
{
    return camera;
}

// ============================================================================
// SCREEN SCALING
// ============================================================================

void CameraManager::setScreenScaleMode(ScreenScaleMode mode)
{
    scaleMode = mode;
    recalcViewport();
    applyCamera();
    float winW = (float)GetScreenWidth();
    float winH = (float)GetScreenHeight();
    Info("Screen size: %f x %f", winW, winH);
}

void CameraManager::setScreenScaleMode(const char *modeStr)
{
    if (strcmp(modeStr, "none") == 0 || strcmp(modeStr, "NONE") == 0)
        scaleMode = SCALE_NONE;
    else if (strcmp(modeStr, "fit") == 0 || strcmp(modeStr, "FIT") == 0)
        scaleMode = SCALE_FIT;
    else if (strcmp(modeStr, "stretch") == 0 || strcmp(modeStr, "STRETCH") == 0)
        scaleMode = SCALE_STRETCH;
    else if (strcmp(modeStr, "fill") == 0 || strcmp(modeStr, "FILL") == 0)
        scaleMode = SCALE_FILL;
    else if (strcmp(modeStr, "letterbox") == 0 || strcmp(modeStr, "LETTERBOX") == 0)
        scaleMode = SCALE_LETTERBOX;

    recalcViewport();
    applyCamera();
}

ScreenScaleMode CameraManager::getScreenScaleMode() const
{
    return scaleMode;
}

const char *CameraManager::getScreenScaleModeString() const
{
    switch (scaleMode)
    {
    case SCALE_NONE:
        return "none";
    case SCALE_FIT:
        return "fit";
    case SCALE_STRETCH:
        return "stretch";
    case SCALE_FILL:
        return "fill";
    case SCALE_LETTERBOX:
        return "letterbox";
    default:
        return "unknown";
    }
}

// ============================================================================
// DESIGN RESOLUTION
// ============================================================================

void CameraManager::setDesignResolution(int width, int height)
{
    designWidth = std::max(1, width);
    designHeight = std::max(1, height);

    
    camera.target = {(float)width * 0.5f, (float)height * 0.5f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    
    recalcViewport();
    applyCamera();
}

void CameraManager::getDesignResolution(int &outWidth, int &outHeight) const
{
    outWidth = designWidth;
    outHeight = designHeight;
}

// ============================================================================
// VIEWPORT
// ============================================================================

Rectangle CameraManager::getViewport() const
{
    return viewport;
}

float CameraManager::getFitScale() const
{
    return fitScale;
}

// ============================================================================
// CAMERA SHAKE
// ============================================================================

void CameraManager::startShake(float amplitudeX, float amplitudeY, float frequency, float durationCycles)
{
    if (frequency <= 0.0f || durationCycles <= 0.0f)
    {
        stopShake();
        return;
    }

    shakeState.active = true;
    shakeState.amplitudeX = amplitudeX;
    shakeState.amplitudeY = amplitudeY;
    shakeState.cycles = durationCycles;
    shakeState.omega = frequency * 2.0f * PI;
    shakeState.cyclesLeft = durationCycles;
}

void CameraManager::stopShake()
{
    shakeState.active = false;
    shakeState.cyclesLeft = 0.0f;
    shakeOffset = {0.0f, 0.0f};
    applyCamera();
}

// ============================================================================
// HELPERS
// ============================================================================

void CameraManager::onWindowResize()
{
    recalcViewport();
    applyCamera();
}

void CameraManager::begin()
{
    BeginMode2D(camera);
}

void CameraManager::end()
{
    EndMode2D();
}

void CameraManager::updateShake(float deltaTime)
{
    if (!shakeState.active)
    {
        shakeOffset = {0.0f, 0.0f};
        return;
    }

    shakeState.cyclesLeft -= 1.0f;

    if (shakeState.cyclesLeft > 0.0f && shakeState.cycles > 0.0f)
    {
        float frac = shakeState.cyclesLeft / shakeState.cycles;
        float v = frac * frac * cosf((1.0f - frac) * shakeState.omega);

        float signX = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
        float signY = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;

        shakeOffset.x = shakeState.amplitudeX * signX * v;
        shakeOffset.y = shakeState.amplitudeY * signY * v;
    }
    else
    {
        shakeState.active = false;
        shakeOffset = {0.0f, 0.0f};
    }
}