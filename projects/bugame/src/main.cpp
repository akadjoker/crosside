

#include "engine.hpp"
#include "interpreter.hpp"
#include "bindings.hpp"
#include "camera.hpp"
#include "platform.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <exception>
#include <algorithm>
#include <vector>
#include <raylib.h>

extern Scene gScene;
extern ParticleSystem gParticleSystem;
extern CameraManager gCamera;


class FileBuffer
{
public:
    bool load(const char *path)
    {
        data.clear();

        int fileSize = 0;
        unsigned char *fileData = LoadFileData(path, &fileSize);
        if (!fileData || fileSize <= 0)
        {
            if (fileData)
                UnloadFileData(fileData);
            return false;
        }

        data.assign(fileData, fileData + fileSize);
        data.push_back(0); // Keep NUL terminator for C-style loaders.
        UnloadFileData(fileData);
        return true;
    }

    const char *c_str() const
    {
        if (data.empty())
            return nullptr;
        return reinterpret_cast<const char *>(data.data());
    }

    size_t size() const
    {
        if (data.empty())
            return 0;
        return data.size() - 1;
    }

    std::string toString() const
    {
        if (data.empty())
            return "";
        return std::string(reinterpret_cast<const char *>(data.data()), size());
    }

private:
    std::vector<unsigned char> data;
};

struct FileLoaderContext
{
    const char *searchPaths[8];
    int pathCount;
    char fullPath[512];
    FileBuffer fileBuffer;
};

static bool isAbsolutePath(const char *path)
{
    if (!path || !*path)
        return false;
    if (path[0] == '/' || path[0] == '\\')
        return true;
#if defined(_WIN32)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':')
    {
        return true;
    }
#endif
    return false;
}

static void pathDirname(const char *path, char *out, size_t outSize)
{
    if (!out || outSize == 0)
        return;
    out[0] = '\0';

    if (!path || !*path)
    {
        snprintf(out, outSize, ".");
        return;
    }

    const char *slash1 = strrchr(path, '/');
    const char *slash2 = strrchr(path, '\\');
    const char *slash = slash1;
    if (slash2 && (!slash1 || slash2 > slash1))
        slash = slash2;

    if (!slash)
    {
        snprintf(out, outSize, ".");
        return;
    }

    size_t len = (size_t)(slash - path);
    if (len == 0)
    {
        snprintf(out, outSize, "/");
        return;
    }

    if (len >= outSize)
        len = outSize - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

const char *multiPathFileLoader(const char *filename, size_t *outSize, void *userdata)
{
    if (!filename || !outSize || !userdata)
        return nullptr;

    FileLoaderContext *ctx = (FileLoaderContext *)userdata;
    *outSize = 0;

    // If include path starts with '/' (ex: "/scripts/main.bu"), also keep relative form.
    const char *relativeName = filename;
    while (*relativeName == '/' || *relativeName == '\\')
    {
        relativeName++;
    }

    // Absolute filesystem path: try directly.
    if (isAbsolutePath(filename))
    {
        if (ctx->fileBuffer.load(filename))
        {
            *outSize = ctx->fileBuffer.size();
            return ctx->fileBuffer.c_str();
        }
        return nullptr;
    }

    // Relative include: search configured script roots first.
    const char *namePart = (relativeName != filename) ? relativeName : filename;
    for (int i = 0; i < ctx->pathCount; i++)
    {
        snprintf(ctx->fullPath, sizeof(ctx->fullPath), "%s/%s", ctx->searchPaths[i], namePart);
        if (!ctx->fileBuffer.load(ctx->fullPath))
            continue;

        *outSize = ctx->fileBuffer.size();
        return ctx->fileBuffer.c_str();
    }

    // Last fallback: current working directory relative.
    if (ctx->fileBuffer.load(filename))
    {
        *outSize = ctx->fileBuffer.size();
        return ctx->fileBuffer.c_str();
    }
    if (relativeName != filename && ctx->fileBuffer.load(relativeName))
    {
        *outSize = ctx->fileBuffer.size();
        return ctx->fileBuffer.c_str();
    }

    return nullptr;
}

// Helper: load file contents into a string
static std::string loadFile(const char *path, bool quiet = false)
{
    FileBuffer file;
    if (!file.load(path))
    {
        if (!quiet)
        {
            TraceLog(LOG_WARNING, "Could not open file: %s", path ? path : "<null>");
        }
        return "";
    }
    return file.toString();
}

static void showFatalScreen(const std::string &message)
{
    bool createdWindow = false;
    if (!IsWindowReady())
    {
        InitWindow(960, 540, "BuGameEngine - Startup Error");
        createdWindow = true;
    }

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Startup Error", 20, 20, 34, RED);
        DrawText(message.c_str(), 20, 80, 22, RAYWHITE);
        DrawText("Press BACK/ESC or close window to exit.", 20, 500, 20, GRAY);
        EndDrawing();
    }

    if (createdWindow && IsWindowReady())
    {
        CloseWindow();
    }
}

// ============================================================
// Global Configuration Variables (set by script)
// ============================================================

int WINDOW_WIDTH = 1280;
int WINDOW_HEIGHT = 720;
std::string WINDOW_TITLE = "BuGameEngine";

bool FULLSCREEN = false;
bool CAN_RESIZE = true;
bool CAN_CLOSE = false;
Color BACKGROUND_COLOR = BLACK;

// ============================================================
// Native Functions for Script Configuration
// ============================================================

int native_set_window_size(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 2)
    {
        Error("set_window_size expects 2 integer arguments (width, height)");
        return 0;
    }
    if (!args[0].isNumber() || !args[1].isNumber())
    {
        Error("set_window_size expects integer arguments (width, height)");
        return 0;
    }

    WINDOW_WIDTH = int(args[0].asNumber());
    WINDOW_HEIGHT = int(args[1].asNumber());

    return 0;
}

int native_set_window_title(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1)
    {
        Error("set_window_title expects 1 string argument (title)");
        return 0;
    }
    if (!args[0].isString())
    {
        Error("set_window_title expects a string argument (title)");
        return 0;
    }

    WINDOW_TITLE = args[0].asStringChars();

    return 0;
}

int native_set_fullscreen(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1)
    {
        Error("set_fullscreen expects 1 boolean argument");
        return 0;
    }

    FULLSCREEN = args[0].asBool();

    return 0;
}

int native_close_window(Interpreter *vm, int argCount, Value *args)
{
    (void)vm;
    (void)argCount;
    (void)args;
    CAN_CLOSE = true;
    return 0;
}

int native_set_window_resizable(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1)
    {
        Error("set_window_resizable expects 1 boolean argument");
        return 0;
    }

    CAN_RESIZE = args[0].asBool();

    return 0;
}

int native_set_log_level(Interpreter *vm, int argCount, Value *args)
{
    if (argCount != 1)
    {
        Error("set_log_level expects 1 integer argument");
        return 0;
    }
    if (!args[0].isNumber())
    {
        Error("set_log_level expects an integer argument");
        return 0;
    }

    int level = int(args[0].asNumber());
    SetTraceLogLevel(level);

    return 0;
}


void FreeResources()
{
}

void onCreate(Interpreter *vm, Process *proc)
{
    Entity *entity = gScene.addEntity(-1, 0, 0, 0);
    proc->userData = entity;
    entity->userData = proc;
    entity->procID = proc->id;
    entity->blueprint = proc->blueprint;
    entity->ready = false;
    entity->layer = 0;
    entity->flags = B_VISIBLE | B_COLLISION;
}

void onStart(Interpreter *vm, Process *proc)
{

    double x = proc->privates[0].asNumber();
    double y = proc->privates[1].asNumber();
    int z = (int)proc->privates[2].asNumber();
    int graph = proc->privates[3].asInt();
    int angle = proc->privates[4].asInt();
    int size = proc->privates[5].asInt();
    int flags = proc->privates[6].asInt();
    int id = proc->privates[7].asInt();
    int father = proc->privates[8].asInt();
    double red = 1.0;
    if (proc->privates[(int)PrivateIndex::iGREEN].isInt())
        red = proc->privates[9].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iGREEN].isNumber())
        red = proc->privates[9].asNumber();

    double green = 1.0;
    if (proc->privates[(int)PrivateIndex::iGREEN].isInt())
        green = proc->privates[10].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iGREEN].isNumber())
        green = proc->privates[10].asNumber();
    double blue = 1.0;
    if (proc->privates[(int)PrivateIndex::iBLUE].isInt())
        blue = proc->privates[11].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iBLUE].isNumber())
        blue = proc->privates[11].asNumber();
    double alpha = 1.0;
    if (proc->privates[(int)PrivateIndex::iALPHA].isInt())
        alpha = proc->privates[12].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iALPHA].isNumber())
        alpha = proc->privates[12].asNumber();

    // Info("Create process: ID:%d  Layer:%d  angle:%d  Size:%d   FLAGS: %d X:%f Y:%f  FATHER:%d  GRAPH:%d", id, z, angle, size,  flags, x, y, father, graph);

    Entity *entity = (Entity *)proc->userData;
    if (!entity)
    {
        // Warning("Process %d has no associated entity!", proc->id);
        return;
    }
    int safeLayer = z;
    if (safeLayer < 0 || safeLayer >= MAX_LAYERS)
        safeLayer = 0;

    if (entity->layer != safeLayer)
    {

        gScene.moveEntityToLayer(entity, safeLayer);
    }

    entity->graph = graph;
    entity->procID = proc->id;
    entity->setPosition(x, y);
    entity->setAngle(angle);
    entity->setSize(size);
    entity->color.r = (uint8)(red * 255.0);
    entity->color.g = (uint8)(green * 255.0);
    entity->color.b = (uint8)(blue * 255.0);
    entity->color.a = (uint8)(alpha * 255.0);
    entity->flags = B_VISIBLE | B_COLLISION;

    entity->ready = true;
}
void onUpdate(Interpreter *vm, Process *proc, float dt)
{

    Entity *entity = (Entity *)proc->userData;
    if (!entity)
    {
        // Warning("Process %d has no associated entity!", proc->id);
        return;
    }
    if (!entity->ready)
        return;

    double x = proc->privates[0].asNumber();
    double y = proc->privates[1].asNumber();
    int z = proc->privates[2].asInt();
    int graph = proc->privates[3].asInt();
    int angle = proc->privates[4].asInt();
    int size = proc->privates[5].asInt();
    int flags = proc->privates[6].asInt();
    int id = proc->privates[7].asInt();
    int father = proc->privates[8].asInt();
    double red = 1.0;
    if (proc->privates[(int)PrivateIndex::iGREEN].isInt())
        red = proc->privates[9].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iGREEN].isNumber())
        red = proc->privates[9].asNumber();

    double green = 1.0;
    if (proc->privates[(int)PrivateIndex::iGREEN].isInt())
        green = proc->privates[10].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iGREEN].isNumber())
        green = proc->privates[10].asNumber();
    double blue = 1.0;
    if (proc->privates[(int)PrivateIndex::iBLUE].isInt())
        blue = proc->privates[11].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iBLUE].isNumber())
        blue = proc->privates[11].asNumber();
    double alpha = 1.0;
    if (proc->privates[(int)PrivateIndex::iALPHA].isInt())
        alpha = proc->privates[12].asInt() / 255.0;
    else if (proc->privates[(int)PrivateIndex::iALPHA].isNumber())
        alpha = proc->privates[12].asNumber();

    int safeLayer = z;
    if (safeLayer < 0 || safeLayer >= MAX_LAYERS)
        safeLayer = 0;

    if (entity->layer != safeLayer)
    {
        gScene.moveEntityToLayer(entity, safeLayer);
    }
    entity->graph = graph;
    entity->setPosition(x, y);
    entity->setAngle(angle);
    entity->setSize(size);
    entity->color.r = (uint8)(red * 255.0);
    entity->color.g = (uint8)(green * 255.0);
    entity->color.b = (uint8)(blue * 255.0);
    entity->color.a = (uint8)(alpha * 255.0);

    // proc->privates[0] = vm->makeDouble(entity->x);
    // proc->privates[1] = vm->makeDouble(entity->y);
    // proc->privates[4] = vm->makeInt(entity->angle);
    // proc->privates[5] = vm->makeInt(entity->size);
    // proc->privates[6] = vm->makeInt(flags);
}
void onDestroy(Interpreter *vm, Process *proc, int exitCode)
{
    (void)vm;
    (void)exitCode;
    //BindingsBox2D::onProcessDestroy(proc);

    //   Info("Destroy process: %d with exit code %d", proc->id, exitCode);
    Entity *entity = (Entity *)proc->userData;
    if (entity && proc->userData)
    {
        gScene.removeEntity(entity);
        proc->userData = nullptr;
    }
}
void onRender(Interpreter *vm, Process *proc)
{
}

int main(int argc, char *argv[])
{
#if defined(PLATFORM_ANDROID)
    SetTraceLogLevel(LOG_INFO);
#else
    SetTraceLogLevel(LOG_WARNING);
#endif

    Interpreter vm;

    VMHooks hooks;
    hooks.onStart = onStart;
    hooks.onUpdate = onUpdate;
    hooks.onDestroy = onDestroy;
    hooks.onRender = onRender;
    hooks.onCreate = onCreate;

    vm.registerAll();
    vm.setHooks(hooks);

    Bindings::registerAll(vm);
    BindingsEase::registerAll(vm);
    registerCameraNatives(vm);
    vm.registerNative("set_window_size", native_set_window_size, 2);
    vm.registerNative("set_window_title", native_set_window_title, 1);
    vm.registerNative("set_fullscreen", native_set_fullscreen, 1);
    vm.registerNative("set_window_resizable", native_set_window_resizable, 1);
    vm.registerNative("close_window", native_close_window, 0);
    vm.registerNative("set_log_level", native_set_log_level, 1);

    FileLoaderContext ctx{};

    const char *scriptFile = nullptr;
    std::string code;
    if (argc > 1)
        scriptFile = argv[1];

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE.c_str());
    SetExitKey(KEY_NULL); // Disable default ESC exit from Raylib.
    InitSound();

    if (scriptFile != nullptr)
    {
        code = loadFile(scriptFile, false);
        if (code.empty())
        {
            std::string msg = "Could not load script: ";
            msg += (scriptFile ? scriptFile : "<null>");
            TraceLog(LOG_ERROR, "%s", msg.c_str());
            showFatalScreen(msg);
            return 1;
        }
    }

    if (code.empty())
    {
        static const char *defaultCandidates[] = {
#ifdef __EMSCRIPTEN__
            "/scripts/main.bu",
            "/main.bu",
#endif
            "scripts/main.bu",
            "./scripts/main.bu",
            "main.bu",
            "../scripts/main.bu",
        };

        for (const char *candidate : defaultCandidates)
        {
            code = loadFile(candidate, true);
            if (!code.empty())
            {
                scriptFile = candidate;
                break;
            }
        }

        if (code.empty())
        {
            std::string msg = "No script file specified and no default found.";
            TraceLog(LOG_ERROR, "%s", msg.c_str());
            showFatalScreen(msg);
            return 1;
        }
    }
    TraceLog(LOG_INFO, "Using script: %s", scriptFile ? scriptFile : "<none>");

    // Build include search paths anchored to the loaded main script location.
    char scriptPathAbs[512] = {0};
    if (scriptFile && isAbsolutePath(scriptFile))
    {
        snprintf(scriptPathAbs, sizeof(scriptPathAbs), "%s", scriptFile);
    }
    else if (scriptFile)
    {
        snprintf(scriptPathAbs, sizeof(scriptPathAbs), "%s/%s", GetWorkingDirectory(), scriptFile);
    }
    else
    {
        snprintf(scriptPathAbs, sizeof(scriptPathAbs), "%s", GetWorkingDirectory());
    }

    char scriptDir[512] = {0};
    char scriptParentDir[512] = {0};
    pathDirname(scriptPathAbs, scriptDir, sizeof(scriptDir));
    pathDirname(scriptDir, scriptParentDir, sizeof(scriptParentDir));

    ctx.pathCount = 0;
    auto addSearchPath = [&](const char *p)
    {
        if (!p || !*p || ctx.pathCount >= 8)
            return;
        for (int i = 0; i < ctx.pathCount; ++i)
        {
            if (strcmp(ctx.searchPaths[i], p) == 0)
                return;
        }
        ctx.searchPaths[ctx.pathCount++] = p;
    };

    addSearchPath(scriptDir);
    addSearchPath(scriptParentDir);
    addSearchPath("/scripts");
    addSearchPath("scripts");
    addSearchPath("./scripts");
    addSearchPath("../scripts");
    addSearchPath(".");

    vm.setFileLoader(multiPathFileLoader, &ctx);

    InitScene();
    gCamera.init(WINDOW_WIDTH, WINDOW_HEIGHT);
    gCamera.setScreenScaleMode(SCALE_NONE);
    gCamera.setVirtualScreenEnabled(false);

    bool scriptOk = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try
    {
        scriptOk = vm.run(code.c_str(), false);
    }
    catch (const std::exception &e)
    {
        std::string msg = "Script exception while loading: ";
        msg += e.what();
        Error("%s", msg.c_str());
        showFatalScreen(msg);
        CloseWindow();
        return 1;
    }
    catch (...)
    {
        std::string msg = "Unknown C++ exception while loading script.";
        Error("%s", msg.c_str());
        showFatalScreen(msg);
        CloseWindow();
        return 1;
    }
#else
    scriptOk = vm.run(code.c_str(), false);
#endif

    if (!scriptOk)
    {
        Error("Failed to execute script: %s", scriptFile);
        std::string msg = "Failed to execute script: ";
        msg += (scriptFile ? scriptFile : "<null>");
        showFatalScreen(msg);
        CloseWindow();
        return 1;
    }

    uint32 flags = 0;
    if (FULLSCREEN)
        flags |= FLAG_FULLSCREEN_MODE;
    if (CAN_RESIZE)
        flags |= FLAG_WINDOW_RESIZABLE;

    SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    SetWindowTitle(WINDOW_TITLE.c_str());
    SetWindowState(flags);
    SetTargetFPS(60);


    // gCamera.setZoom(1.0f);
    
    
    // gCamera.setVirtualScreenEnabled(true);
    // gCamera.setScreenScaleMode(SCALE_FIT);   

 

     //gCamera.setDesignResolution(30 * 24, 20 * 24); // Configurar resolução de design para 720x480 (30x20 tiles de 24px)
    // gCamera.setScreenScaleMode(SCALE_STRETCH);  // Usar modo FIT para manter aspecto ratio e mostrar barras pretas se necessário
    // gCamera.setVirtualScreenEnabled(true); // Ativar virtual screen para usar a resolução de design

    while (!CAN_CLOSE && vm.getTotalAliveProcesses() > 0)
    {
        if (WindowShouldClose())
        {
            CAN_CLOSE = true;
        }

        if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_X))
        {
            CAN_CLOSE = true;
        }
 
        //   // Mudar modo de escala
        // if (IsKeyPressed(KEY_F1)) gCamera.setScreenScaleMode(SCALE_NONE);
        // if (IsKeyPressed(KEY_F2)) gCamera.setScreenScaleMode(SCALE_FIT);
        // if (IsKeyPressed(KEY_F3)) gCamera.setScreenScaleMode(SCALE_STRETCH);
        // if (IsKeyPressed(KEY_F4)) gCamera.setScreenScaleMode(SCALE_FILL);



       
        float dt = GetFrameTime();
        BindingsInput::update();
        gCamera.update(dt);
        UpdateFade(dt);
        gScene.updateCollision();

         

        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);
        gCamera.begin();
        gParticleSystem.update(dt);
        BindingsDraw::resetDrawCommands();
        vm.update(dt);
        RenderScene();
        gParticleSystem.cleanup();
        gParticleSystem.draw();
        BindingsBox2D::renderDebug();
        gCamera.end();

        BindingsDraw::RenderScreenCommands();
        BindingsInput::drawVirtualKeys();
 

        DrawFade();

       // DrawText(TextFormat("FPS: %d Processes: %d", GetFPS(), vm.getTotalAliveProcesses()), 10, 10, 20, WHITE);
        EndDrawing();
    }
    BindingsMessage::clearAllMessages();
    gParticleSystem.clear();
    BindingsBox2D::shutdownPhysics();
    BindingsDraw::unloadFonts();
 
    DestroySound();
 
    DestroyScene();

    CloseWindow();
    return 0;
}
