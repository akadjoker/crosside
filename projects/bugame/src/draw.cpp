#include "bindings.hpp"
#include "engine.hpp"
#include "render.hpp"
#include <raylib.h>
#include <string>
#include <vector>

extern GraphLib gGraphLib;
extern Scene gScene;

namespace BindingsDraw
{

    Color currentColor = WHITE;
    int layer = 0;
    bool screen= false;

    static std::vector<Font> loadedFonts;
    enum class DrawCommandType
    {
        Line,
        Point,
        Text,
        FontText,
        FontTextRotate,
        Circle,
        Rectangle,
        RotatedRectangle,
        RotatedRectangleEx,
        LineEx,
        Triangle,
        Graph,
        GraphEx,
        ClipBegin,
        ClipEnd
    };

    struct LineCmd { int x1, y1, x2, y2; };
    struct PointCmd { int x, y; };
    struct TextCmd { std::string text; int x, y, size; };
    struct FontTextCmd { std::string text; int x, y, size; float spacing; int fontId; };
    struct FontTextRotateCmd { std::string text; int x, y, size; float rotation, spacing, pivotX, pivotY; int fontId; };
    struct CircleCmd { int x, y, radius; bool fill; };
    struct RectangleCmd { int x, y, width, height; bool fill; };
    struct RotatedRectangleCmd { int x, y, width, height; float rotation; bool fill; };
    struct RotatedRectangleExCmd { int x, y, width, height; float rotation; bool fill; float originX, originY; };
    struct LineExCmd { int x1, y1, x2, y2; float thickness; };
    struct TriangleCmd { int x1, y1, x2, y2, x3, y3; bool fill; };
    struct GraphCmd { int graphId; int x, y; };
    struct GraphExCmd { int graphId; int x, y; float rotation, sizeX, sizeY; bool flipX, flipY; };
    struct ClipCmd { int x, y, width, height; };

    struct DrawCommand
    {
        DrawCommandType type = DrawCommandType::Line;
        Color color = WHITE;
        std::string text;
        int x1 = 0;
        int y1 = 0;
        int x2 = 0;
        int y2 = 0;
        int x3 = 0;
        int y3 = 0;
        int width = 0;
        int height = 0;
        int radius = 0;
        int size = 0;
        int graphId = -1;
        int fontId = -1;
        float rotation = 0.0f;
        float spacing = 0.0f;
        float pivotX = 0.0f;
        float pivotY = 0.0f;
        float sizeX = 100.0f;
        float sizeY = 100.0f;
        float thickness = 1.0f;
        float originX = 0.0f;
        float originY = 0.0f;
        bool fill = false;
        bool flipX = false;
        bool flipY = false;
    };

    static std::vector<DrawCommand> screenCommands;
    static int activeClipDepth = 0;

    static void enqueueScreenCommand(DrawCommandType type, Color color, const LineCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x1; cmd.y1 = p.y1; cmd.x2 = p.x2; cmd.y2 = p.y2;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const PointCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x; cmd.y1 = p.y;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const TextCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.text = p.text; cmd.x1 = p.x; cmd.y1 = p.y; cmd.size = p.size;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const FontTextCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.text = p.text; cmd.x1 = p.x; cmd.y1 = p.y; cmd.size = p.size; cmd.spacing = p.spacing; cmd.fontId = p.fontId;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const FontTextRotateCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.text = p.text; cmd.x1 = p.x; cmd.y1 = p.y; cmd.size = p.size;
        cmd.rotation = p.rotation; cmd.spacing = p.spacing; cmd.pivotX = p.pivotX; cmd.pivotY = p.pivotY; cmd.fontId = p.fontId;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const CircleCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x; cmd.y1 = p.y; cmd.radius = p.radius; cmd.fill = p.fill;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const RectangleCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x; cmd.y1 = p.y; cmd.width = p.width; cmd.height = p.height; cmd.fill = p.fill;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const RotatedRectangleCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x; cmd.y1 = p.y; cmd.width = p.width; cmd.height = p.height; cmd.rotation = p.rotation; cmd.fill = p.fill;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const RotatedRectangleExCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x; cmd.y1 = p.y; cmd.width = p.width; cmd.height = p.height; cmd.rotation = p.rotation; cmd.fill = p.fill;
        cmd.originX = p.originX; cmd.originY = p.originY;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const LineExCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x1; cmd.y1 = p.y1; cmd.x2 = p.x2; cmd.y2 = p.y2; cmd.thickness = p.thickness;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const TriangleCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x1; cmd.y1 = p.y1; cmd.x2 = p.x2; cmd.y2 = p.y2; cmd.x3 = p.x3; cmd.y3 = p.y3; cmd.fill = p.fill;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const GraphCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.graphId = p.graphId; cmd.x1 = p.x; cmd.y1 = p.y;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const GraphExCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.graphId = p.graphId; cmd.x1 = p.x; cmd.y1 = p.y; cmd.rotation = p.rotation;
        cmd.sizeX = p.sizeX; cmd.sizeY = p.sizeY; cmd.flipX = p.flipX; cmd.flipY = p.flipY;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color, const ClipCmd &p)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        cmd.x1 = p.x; cmd.y1 = p.y; cmd.width = p.width; cmd.height = p.height;
        screenCommands.push_back(std::move(cmd));
    }
    static void enqueueScreenCommand(DrawCommandType type, Color color)
    {
        DrawCommand cmd;
        cmd.type = type; cmd.color = color;
        screenCommands.push_back(std::move(cmd));
    }

    static void draw_font_buffered(const DrawCommand &cmd)
    {
        if (cmd.fontId < 0 || cmd.fontId >= (int)loadedFonts.size())
        {
            DrawTextEx(GetFontDefault(), cmd.text.c_str(), {(float)cmd.x1, (float)cmd.y1}, (float)cmd.size, cmd.spacing, cmd.color);
            return;
        }
        Font &font = loadedFonts[cmd.fontId];
        DrawTextEx(font, cmd.text.c_str(), {(float)cmd.x1, (float)cmd.y1}, (float)cmd.size, cmd.spacing, cmd.color);
    }

    static void draw_font_rotate_buffered(const DrawCommand &cmd)
    {
        if (cmd.fontId < 0 || cmd.fontId >= (int)loadedFonts.size())
        {
            DrawTextPro(GetFontDefault(), cmd.text.c_str(), {(float)cmd.x1, (float)cmd.y1}, {cmd.pivotX, cmd.pivotY}, cmd.rotation, (float)cmd.size, cmd.spacing, cmd.color);
            return;
        }
        Font &font = loadedFonts[cmd.fontId];
        DrawTextPro(font, cmd.text.c_str(), {(float)cmd.x1, (float)cmd.y1}, {cmd.pivotX, cmd.pivotY}, cmd.rotation, (float)cmd.size, cmd.spacing, cmd.color);
    }

    static void renderCommand(const DrawCommand &cmd)
    {
        switch (cmd.type)
        {
        case DrawCommandType::Line:
            DrawLine(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.color);
            break;
        case DrawCommandType::Point:
            DrawPixel(cmd.x1, cmd.y1, cmd.color);
            break;
        case DrawCommandType::Text:
            DrawText(cmd.text.c_str(), cmd.x1, cmd.y1, cmd.size, cmd.color);
            break;
        case DrawCommandType::FontText:
            draw_font_buffered(cmd);
            break;
        case DrawCommandType::FontTextRotate:
            draw_font_rotate_buffered(cmd);
            break;
        case DrawCommandType::Circle:
            if (cmd.fill)
                DrawCircle(cmd.x1, cmd.y1, cmd.radius, cmd.color);
            else
                DrawCircleLines(cmd.x1, cmd.y1, cmd.radius, cmd.color);
            break;
        case DrawCommandType::Rectangle:
            if (cmd.fill)
                DrawRectangle(cmd.x1, cmd.y1, cmd.width, cmd.height, cmd.color);
            else
                DrawRectangleLines(cmd.x1, cmd.y1, cmd.width, cmd.height, cmd.color);
            break;
        case DrawCommandType::RotatedRectangle:
            DrawRectanglePro({(float)cmd.x1, (float)cmd.y1, (float)cmd.width, (float)cmd.height},
                             {(float)cmd.width / 2, (float)cmd.height / 2}, cmd.rotation, cmd.color);
            break;
        case DrawCommandType::RotatedRectangleEx:
            DrawRectanglePro({(float)cmd.x1, (float)cmd.y1, (float)cmd.width, (float)cmd.height},
                             {cmd.originX, cmd.originY}, cmd.rotation, cmd.color);
            break;
        case DrawCommandType::LineEx:
            DrawLineEx({(float)cmd.x1, (float)cmd.y1}, {(float)cmd.x2, (float)cmd.y2}, cmd.thickness, cmd.color);
            break;
        case DrawCommandType::Triangle:
            if (cmd.fill)
                DrawTriangle({(float)cmd.x1, (float)cmd.y1}, {(float)cmd.x2, (float)cmd.y2}, {(float)cmd.x3, (float)cmd.y3}, cmd.color);
            else
                DrawTriangleLines({(float)cmd.x1, (float)cmd.y1}, {(float)cmd.x2, (float)cmd.y2}, {(float)cmd.x3, (float)cmd.y3}, cmd.color);
            break;
        case DrawCommandType::Graph:
        {
            Graph *graph = gGraphLib.getGraph(cmd.graphId);
            if (!graph)
                break;
            Texture2D *tex = gGraphLib.getTexture(graph->texture);
            if (!tex)
                break;
            DrawTextureRec(*tex, graph->clip, {(float)cmd.x1, (float)cmd.y1}, cmd.color);
            break;
        }
        case DrawCommandType::GraphEx:
        {
            Graph *graph = gGraphLib.getGraph(cmd.graphId);
            if (!graph)
                break;
            Texture2D *tex = gGraphLib.getTexture(graph->texture);
            if (!tex)
                break;

            if (cmd.rotation == 0.0f && cmd.sizeX == 100.0f && cmd.sizeY == 100.0f && !cmd.flipX && !cmd.flipY)
            {
                DrawTextureRec(*tex, graph->clip, {(float)cmd.x1, (float)cmd.y1}, cmd.color);
            }
            else
            {
                int pivotX = (int)(graph->clip.width / 2);
                int pivotY = (int)(graph->clip.height / 2);
                RenderTexturePivotRotateSizeXY(*tex, pivotX, pivotY, graph->clip,
                                               (float)cmd.x1, (float)cmd.y1, cmd.rotation, cmd.sizeX, cmd.sizeY,
                                               cmd.flipX, cmd.flipY, cmd.color);
            }
            break;
        }
        case DrawCommandType::ClipBegin:
            BeginScissorMode(cmd.x1, cmd.y1, cmd.width, cmd.height);
            activeClipDepth += 1;
            break;
        case DrawCommandType::ClipEnd:
            if (activeClipDepth > 0)
            {
                EndScissorMode();
                activeClipDepth -= 1;
            }
            break;
        }
    }

    static void draw_font_impl(String *text, int x, int y, int size, float spacing, Color color, int fontId)
    {
        if (fontId < 0 || fontId >= (int)loadedFonts.size())
        {
            DrawTextEx(GetFontDefault(), text->chars(), {(float)x, (float)y}, (float)size, spacing, color);
            return;
        }
        Font &font = loadedFonts[fontId];
        DrawTextEx(font, text->chars(), {(float)x, (float)y}, (float)size, spacing, color);
    }

    static void draw_font_rotate_impl(String *text, int x, int y, int size, float rotation, float spacing, float pivot_x, float pivot_y, Color color, int fontId)
    {
        
        if (fontId < 0 || fontId >= (int)loadedFonts.size())
        {
            DrawTextPro(GetFontDefault(), text->chars(), {(float)x, (float)y}, {pivot_x, pivot_y}, rotation, (float)size, spacing, color);
            return;
        }
        Font &font = loadedFonts[fontId];
        DrawTextPro(font, text->chars(), {(float)x, (float)y}, {pivot_x, pivot_y}, rotation, (float)size, spacing, color);
    }

    // === Native functions ===

    static int native_set_draw_layer(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("set_draw_layer expects 1 argument (layer)");
            return 0;
        }
       
        layer = args[0].asInt();
        if (layer < 0 || layer >= MAX_LAYERS)
        {
            Error("set_draw_layer: layer out of bounds");
            layer = 0;
        }
        return 0;
    }

    static int native_set_draw_screen(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("set_draw_screen expects 1 argument (bool)");
            return 0;
        }
       
        screen = args[0].asBool();
        return 0;
    }

    static int native_line(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 4)
        {
            Error("draw_line expects 4 arguments (x1, y1, x2, y2)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("draw_line expects 4 number arguments (x1, y1, x2, y2)");
            return 0;
        }
        

        int x1 = (int)args[0].asNumber();
        int y1 = (int)args[1].asNumber();
        int x2 = (int)args[2].asNumber();
        int y2 = (int)args[3].asNumber();

        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::Line, currentColor, LineCmd{x1, y1, x2, y2});
            return 0;
        }
        Layer &l = gScene.layers[layer];
        x1 -= l.scroll_x;
        y1 -= l.scroll_y;
        x2 -= l.scroll_x;
        y2 -= l.scroll_y;
        DrawLine(x1, y1, x2, y2, currentColor);
        return 0;
    }

    static int native_point(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("draw_point expects 2 arguments (x, y)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("draw_point expects 2 number arguments (x, y)");
            return 0;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::Point, currentColor, PointCmd{x, y});
            return 0;
        }
        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;
        DrawPixel(x, y, currentColor);
        return 0;
    }

    static int native_text(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 4)
        {
            Error("draw_text expects 4 arguments (text, x, y, size)");
            return 0;
        }
        if (!args[0].isString() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("draw_text expects 1 string and 3 number arguments (text, x, y, size)");
            return 0;
        }

        String *text = args[0].asString();
        int x = (int)args[1].asNumber();
        int y = (int)args[2].asNumber();
        int size = (int)args[3].asNumber();
        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::Text, currentColor, TextCmd{std::string(text->chars()), x, y, size});
            return 0;
        }
        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;
        DrawText(text->chars(), x, y, size, currentColor);
        return 0;
    }

    static int native_draw_font(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 6)
        {
            Error("draw_font expects 6 arguments (text, x, y, size, spacing, fontId)");
            return 0;
        }

        String *text = args[0].asString();
        int x = (int)args[1].asNumber();
        int y = (int)args[2].asNumber();
        int size = (int)args[3].asNumber();
        float spacing = (float)args[4].asNumber();
        int fontId = args[5].asInt();

        if (screen)
        {
            enqueueScreenCommand(
                DrawCommandType::FontText,
                currentColor,
                FontTextCmd{std::string(text->chars()), x, y, size, spacing, fontId});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;
        draw_font_impl(text, x, y, size, spacing, currentColor, fontId);
        return 0;
    }

    static int native_draw_font_rotate(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 9)
        {
            Error("draw_font_rotate expects 9 arguments (text, x, y, size, rotation, spacing, pivot_x, pivot_y, fontId)");
            return 0;
        }

        String *text = args[0].asString();
        int x = (int)args[1].asNumber();
        int y = (int)args[2].asNumber();
        int size = (int)args[3].asNumber();
        float rotation = (float)args[4].asNumber();
        float spacing = (float)args[5].asNumber();
        float pivot_x = (float)args[6].asNumber();
        float pivot_y = (float)args[7].asNumber();
        int fontId = args[8].asInt();

        if (screen)
        {
            enqueueScreenCommand(
                DrawCommandType::FontTextRotate,
                currentColor,
                FontTextRotateCmd{std::string(text->chars()), x, y, size, rotation, spacing, pivot_x, pivot_y, fontId});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;
        draw_font_rotate_impl(text, x, y, size, rotation, spacing, pivot_x, pivot_y, currentColor, fontId);
        return 0;
    }

    static int native_circle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 4)
        {
            Error("draw_circle expects 4 arguments (centerX, centerY, radius, fill)");
            return 0;
        }

        int centerX = (int)args[0].asNumber();
        int centerY = (int)args[1].asNumber();
        int radius = (int)args[2].asNumber();
        bool fill = (int)args[3].asNumber() != 0;

        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::Circle, currentColor, CircleCmd{centerX, centerY, radius, fill});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        centerX -= l.scroll_x;
        centerY -= l.scroll_y;

        if (fill)
            DrawCircle(centerX, centerY, radius, currentColor);
        else
            DrawCircleLines(centerX, centerY, radius, currentColor);
        return 0;
    }

    static int native_rectangle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 5)
        {
            Error("draw_rectangle expects 5 arguments (x, y, width, height, fill)");
            return 0;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        int width = (int)args[2].asNumber();
        int height = (int)args[3].asNumber();
        bool fill = args[4].asBool();
        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::Rectangle, currentColor, RectangleCmd{x, y, width, height, fill});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;

        if (fill)
            DrawRectangle(x, y, width, height, currentColor);
        else
            DrawRectangleLines(x, y, width, height, currentColor);
        return 0;
    }

    //draw_rotated_rectangle
    static int native_rotated_rectangle_ex(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 8)
        {
            Error("draw_rotated_rectangle_ex expects 8 arguments (x, y, width, height, rotation, fill, center_x, center_y)");
            return 0;
        }
  

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        int width = (int)args[2].asNumber();
        int height = (int)args[3].asNumber();
        float rotation = (float)args[4].asNumber();
        bool fill = args[5].asBool();
        Vector2 origin = {(float)args[6].asNumber(), (float)args[7].asNumber()};

        if (screen)
        {
            enqueueScreenCommand(
                DrawCommandType::RotatedRectangleEx,
                currentColor,
                RotatedRectangleExCmd{x, y, width, height, rotation, fill, origin.x, origin.y});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;

        if (fill)
            DrawRectanglePro({(float)x, (float)y, (float)width, (float)height}, origin, rotation, currentColor);
        else
            DrawRectanglePro({(float)x, (float)y, (float)width, (float)height}, origin, rotation, currentColor);
        return 0;
    }

    static int native_rotated_rectangle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 6)
        {
            Error("draw_rotated_rectangle expects 6 arguments (x, y, width, height, rotation, fill)");
            return 0;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        int width = (int)args[2].asNumber();
        int height = (int)args[3].asNumber();
        float rotation = (float)args[4].asNumber();
        bool fill = args[5].asBool();

        if (screen)
        {
            enqueueScreenCommand(
                DrawCommandType::RotatedRectangle,
                currentColor,
                RotatedRectangleCmd{x, y, width, height, rotation, fill});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;

        if (fill)
            DrawRectanglePro({(float)x, (float)y, (float)width, (float)height}, {(float)width / 2, (float)height / 2}, rotation, currentColor);
        else
            DrawRectanglePro({(float)x, (float)y, (float)width, (float)height}, {(float)width / 2, (float)height / 2}, rotation, currentColor);
        return 0;
    }

    static int native_line_ex(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 5)
        {
            Error("draw_line_ex expects 5 arguments (x1, y1, x2, y2, thickness)");
            return 0;
        }

        int x1 = (int)args[0].asNumber();
        int y1 = (int)args[1].asNumber();
        int x2 = (int)args[2].asNumber();
        int y2 = (int)args[3].asNumber();
        float thickness = (float)args[4].asNumber();

        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::LineEx, currentColor, LineExCmd{x1, y1, x2, y2, thickness});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x1 -= l.scroll_x;
        y1 -= l.scroll_y;
        x2 -= l.scroll_x;
        y2 -= l.scroll_y;
        DrawLineEx({(float)x1, (float)y1}, {(float)x2, (float)y2}, thickness, currentColor);
        return 0;
    }

    static int native_triangle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 7)
        {
            Error("draw_triangle expects 7 arguments (x1, y1, x2, y2, x3, y3, fill)");
            return 0;
        }

        Vector2 v1 = {(float)args[0].asNumber(), (float)args[1].asNumber()};
        Vector2 v2 = {(float)args[2].asNumber(), (float)args[3].asNumber()};
        Vector2 v3 = {(float)args[4].asNumber(), (float)args[5].asNumber()};
        bool fill = args[6].asBool();

        if (screen)
        {
            enqueueScreenCommand(
                DrawCommandType::Triangle,
                currentColor,
                TriangleCmd{(int)v1.x, (int)v1.y, (int)v2.x, (int)v2.y, (int)v3.x, (int)v3.y, fill});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        v1.x -= l.scroll_x;
        v1.y -= l.scroll_y;
        v2.x -= l.scroll_x;
        v2.y -= l.scroll_y;
        v3.x -= l.scroll_x;
        v3.y -= l.scroll_y;

        if (fill)
            DrawTriangle(v1, v2, v3, currentColor);
        else
            DrawTriangleLines(v1, v2, v3, currentColor);
        return 0;
    }

    static int native_draw_graph(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("draw_graph expects 3 arguments (graphId, x, y)");
            return 0;
        }

        int graphId = (int)args[0].asNumber();
        float x = (float)args[1].asNumber();
        float y = (float)args[2].asNumber();

        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::Graph, currentColor, GraphCmd{graphId, (int)x, (int)y});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;

        Graph *graph = gGraphLib.getGraph(graphId);
        if (!graph) return 0;
        Texture2D *tex = gGraphLib.getTexture(graph->texture);
        if (!tex) return 0;
        DrawTextureRec(*tex, graph->clip, {x, y}, currentColor);
        return 0;
    }

    static int native_draw_graph_ex(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 8)
        {
            Error("draw_graph_ex expects 8 arguments (graphId, x, y, angle, sizeX, sizeY, flipX, flipY)");
            return 0;
        }

        int graphId = (int)args[0].asNumber();
        float x = (float)args[1].asNumber();
        float y = (float)args[2].asNumber();
        float angle = (float)args[3].asNumber();
        float sizeX = (float)args[4].asNumber();
        float sizeY = (float)args[5].asNumber();
        bool flipX = args[6].asBool();
        bool flipY = args[7].asBool();

        if (screen)
        {
            enqueueScreenCommand(
                DrawCommandType::GraphEx,
                currentColor,
                GraphExCmd{graphId, (int)x, (int)y, angle, sizeX, sizeY, flipX, flipY});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;

        Graph *graph = gGraphLib.getGraph(graphId);
        if (!graph) return 0;
        Texture2D *tex = gGraphLib.getTexture(graph->texture);
        if (!tex) return 0;

        if (angle == 0 && sizeX == 100 && sizeY == 100 && !flipX && !flipY)
        {
            DrawTextureRec(*tex, graph->clip, {x, y}, currentColor);
        }
        else
        {
            int pivotX = (int)(graph->clip.width / 2);
            int pivotY = (int)(graph->clip.height / 2);
            RenderTexturePivotRotateSizeXY(*tex, pivotX, pivotY, graph->clip,
                                          x, y, angle, sizeX, sizeY,
                                          flipX, flipY, currentColor);
        }
        return 0;
    }

    static int native_set_color(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("set_color expects 3 arguments (red, green, blue)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("set_color expects 3 number arguments (red, green, blue)");
            return 0;
        }
        currentColor.r = (int)args[0].asNumber();
        currentColor.g = (int)args[1].asNumber();
        currentColor.b = (int)args[2].asNumber();
        return 0;
    }

    static int native_set_alpha(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("set_alpha expects 1 argument (alpha)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_alpha expects a number argument (alpha)");
            return 0;
        }
        currentColor.a = (int)args[0].asNumber();
        return 0;
    }

    static int native_load_font(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isString())
        {
            Error("load_font expects 1 string argument (path)");
            vm->pushInt(-1);
            return 1;
        }

        const char *path = args[0].asStringChars();
        Font font = LoadFont(path);
        if (font.texture.id <= 0)
        {
            Error("Failed to load font from path: %s", path);
            vm->pushInt(-1);
            return 1;
        }

        loadedFonts.push_back(font);
        vm->pushInt((int)loadedFonts.size() - 1);
        return 1;
    }

    int native_start_fade(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("start_fade expects 2 arguments (targetAlpha, speed)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("start_fade expects 2 number arguments (targetAlpha, speed)");
            return 0;
        }

        float targetAlpha = (float)args[0].asNumber();
        float speed = (float)args[1].asNumber();
        StartFade(targetAlpha, speed, currentColor);
        return 0;
    }

    int native_is_fade_complete(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("is_fade_complete expects 0 arguments");
            vm->pushBool(false);
            return 1;
        }
        vm->pushBool(IsFadeComplete());
        return 1;
    }

    int native_get_fade_progress(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_fade_progress expects 0 arguments");
            vm->pushDouble(0.0);
            return 1;
        }
        vm->pushDouble(GetFadeProgress());
        return 1;
    }

    int native_fade_in(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("fade_in expects 1 number argument (speed)");
            return 0;
        }
        FadeIn((float)args[0].asNumber(), currentColor);
        return 0;
    }

    int native_fade_out(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("fade_out expects 1 number argument (speed)");
            return 0;
        }
        FadeOut((float)args[0].asNumber(), currentColor);
        return 0;
    }

    int native_draw_fps(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("draw_fps expects 2 arguments (x, y)");
            return 0;
        }
        DrawFPS((int)args[0].asNumber(), (int)args[1].asNumber());
        return 0;
    }

    static int native_clip_begin(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 4)
        {
            Error("clip_begin expects 4 arguments (x, y, width, height)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("clip_begin expects 4 number arguments (x, y, width, height)");
            return 0;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        int width = (int)args[2].asNumber();
        int height = (int)args[3].asNumber();

        if (width <= 0 || height <= 0)
        {
            return 0;
        }

        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::ClipBegin, currentColor, ClipCmd{x, y, width, height});
            return 0;
        }

        Layer &l = gScene.layers[layer];
        x -= l.scroll_x;
        y -= l.scroll_y;
        BeginScissorMode(x, y, width, height);
        activeClipDepth += 1;
        return 0;
    }

    static int native_clip_end(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("clip_end expects 0 arguments");
            return 0;
        }

        if (screen)
        {
            enqueueScreenCommand(DrawCommandType::ClipEnd, currentColor);
            return 0;
        }

        if (activeClipDepth > 0)
        {
            EndScissorMode();
            activeClipDepth -= 1;
        }
        return 0;
    }

    static int native_get_text_width(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isString() || !args[1].isNumber())
        {
            Error("get_text_width expects 2 arguments (text, size)");
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt(MeasureText(args[0].asStringChars(), (int)args[1].asNumber()));
        return 1;
    }

    static int native_get_font_text_width(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 4 || !args[0].isString() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("get_font_text_width expects 4 arguments (text, size, spacing, fontId)");
            vm->pushInt(0);
            return 1;
        }

        int fontId = (int)args[3].asNumber();
        Font font = GetFontDefault();
        if (fontId >= 0 && fontId < (int)loadedFonts.size())
            font = loadedFonts[fontId];

        Vector2 measure = MeasureTextEx(font, args[0].asStringChars(), (float)args[1].asNumber(), (float)args[2].asNumber());
        vm->pushInt((int)measure.x);
        return 1;
    }

    static int native_get_graph_width(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_graph_width expects 1 argument (graphId)");
            vm->pushInt(0);
            return 1;
        }
        Graph *g = gGraphLib.getGraph((int)args[0].asNumber());
        vm->pushInt(g ? g->width : 0);
        return 1;
    }

    static int native_get_graph_height(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("get_graph_height expects 1 argument (graphId)");
            vm->pushInt(0);
            return 1;
        }
        Graph *g = gGraphLib.getGraph((int)args[0].asNumber());
        vm->pushInt(g ? g->height : 0);
        return 1;
    }

    // === Structs ===

    static void color_ctor(Interpreter *vm, void *buffer, int argc, Value *args)
    {
        Color *v = (Color *)buffer;
        v->r = (uint8)args[0].asNumber();
        v->g = (uint8)args[1].asNumber();
        v->b = (uint8)args[2].asNumber();
        v->a = (uint8)args[3].asNumber();
    }

    void registerColor(Interpreter &vm)
    {
        auto *color = vm.registerNativeStruct(
            "Color",
            sizeof(Color),
            color_ctor,
            nullptr);

        vm.addStructField(color, "r", offsetof(Color, r), FieldType::BYTE);
        vm.addStructField(color, "g", offsetof(Color, g), FieldType::BYTE);
        vm.addStructField(color, "b", offsetof(Color, b), FieldType::BYTE);
        vm.addStructField(color, "a", offsetof(Color, a), FieldType::BYTE);
    }

    static void vector2_ctor(Interpreter *vm, void *buffer, int argc, Value *args)
    {
        Vector2 *vec = (Vector2 *)buffer;
        vec->x = args[0].asNumber();
        vec->y = args[1].asNumber();
    }

    void registerVector2(Interpreter &vm)
    {
        auto *vec2 = vm.registerNativeStruct(
            "Vec2",
            sizeof(Vector2),
            vector2_ctor,
            nullptr);

        vm.addStructField(vec2, "x", offsetof(Vector2, x), FieldType::FLOAT);
        vm.addStructField(vec2, "y", offsetof(Vector2, y), FieldType::FLOAT);
    }

    void resetDrawCommands()
    {
        screenCommands.clear();
        while (activeClipDepth > 0)
        {
            EndScissorMode();
            activeClipDepth -= 1;
        }
    }

    void RenderWorldCommands()
    {
        // Reserved for future world command buffering.
    }

    void RenderScreenCommands()
    {
        for (size_t i = 0; i < screenCommands.size(); i++)
        {
            renderCommand(screenCommands[i]);
        }
        while (activeClipDepth > 0)
        {
            EndScissorMode();
            activeClipDepth -= 1;
        }
        screenCommands.clear();
    }

    // === Registration ===

    void registerAll(Interpreter &vm)
    {
        vm.registerNative("draw_line", native_line, 4);
        vm.registerNative("draw_circle", native_circle, 4);
        vm.registerNative("draw_point", native_point, 2);
        vm.registerNative("draw_text", native_text, 4);
        vm.registerNative("draw_font", native_draw_font, 6);
        vm.registerNative("draw_font_rotate", native_draw_font_rotate, 9);
        vm.registerNative("draw_rectangle", native_rectangle, 5);
        vm.registerNative("draw_triangle", native_triangle, 7);
        vm.registerNative("draw_graph", native_draw_graph, 3);
        vm.registerNative("draw_graph_ex", native_draw_graph_ex, 8);

        vm.registerNative("draw_line_ex", native_line_ex, 5);
        vm.registerNative("draw_rotated_rectangle", native_rotated_rectangle, 6);
        vm.registerNative("draw_rotated_rectangle_ex", native_rotated_rectangle_ex, 8);
        
        vm.registerNative("set_draw_layer", native_set_draw_layer, 1);
        vm.registerNative("set_draw_screen", native_set_draw_screen, 1);

        vm.registerNative("get_text_width", native_get_text_width, 2);
        vm.registerNative("get_font_text_width", native_get_font_text_width, 4);
        vm.registerNative("get_graph_width", native_get_graph_width, 1);
        vm.registerNative("get_graph_height", native_get_graph_height, 1);

        vm.registerNative("set_color", native_set_color, 3);
        vm.registerNative("set_alpha", native_set_alpha, 1);

        vm.registerNative("draw_fps", native_draw_fps, 2);
        vm.registerNative("clip_begin", native_clip_begin, 4);
        vm.registerNative("clip_end", native_clip_end, 0);
        vm.registerNative("set_clip_rect", native_clip_begin, 4);
        vm.registerNative("clear_clip_rect", native_clip_end, 0);

        vm.registerNative("start_fade", native_start_fade, 2);
        vm.registerNative("is_fade_complete", native_is_fade_complete, 0);
        vm.registerNative("fade_in", native_fade_in, 1);
        vm.registerNative("fade_out", native_fade_out, 1);
        vm.registerNative("get_fade_progress", native_get_fade_progress, 0);

        vm.registerNative("load_font", native_load_font, 1);

        registerColor(vm);
        registerVector2(vm);
    }

    void unloadFonts()
    {
        for (size_t i = 0; i < loadedFonts.size(); i++)
        {
            UnloadFont(loadedFonts[i]);
        }
        loadedFonts.clear();
    }

}
