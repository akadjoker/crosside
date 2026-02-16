#include "bindings.hpp"
#include "engine.hpp"
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include <string>

extern GraphLib gGraphLib;

namespace BindingsImage
{
    struct ScriptImage
    {
        Image image{};
        int bpp{4};
    };

    static NativeClassDef *gImageClassDef = nullptr;

    static bool has_image_data(const ScriptImage *img)
    {
        return img && img->image.data && img->image.width > 0 && img->image.height > 0;
    }

    static void free_image_data(ScriptImage *img)
    {
        if (!img || !img->image.data)
            return;
        UnloadImage(img->image);
        img->image = {};
    }

    static int format_from_bpp(int bpp)
    {
        switch (bpp)
        {
        case 1:
            return PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        case 2:
            return PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
        case 3:
            return PIXELFORMAT_UNCOMPRESSED_R8G8B8;
        case 4:
            return PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        default:
            return -1;
        }
    }

    static int bpp_from_format(int format)
    {
        switch (format)
        {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
            return 1;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            return 2;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
            return 3;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            return 4;
        default:
            return 4;
        }
    }

    static bool make_blank_image(ScriptImage *dst, int width, int height, int bpp)
    {
        if (!dst || width <= 0 || height <= 0)
            return false;

        int targetFormat = format_from_bpp(bpp);
        if (targetFormat < 0)
            return false;

        Image img = GenImageColor(width, height, {0, 0, 0, 0});
        if (!img.data)
            return false;

        if (img.format != targetFormat)
        {
            ImageFormat(&img, targetFormat);
        }

        free_image_data(dst);
        dst->image = img;
        dst->bpp = bpp;
        return true;
    }

    static bool load_image_from_file(ScriptImage *dst, const char *path)
    {
        if (!dst || !path)
            return false;
        Image img = LoadImage(path);
        if (!img.data)
            return false;

        free_image_data(dst);
        dst->image = img;
        dst->bpp = bpp_from_format(img.format);
        return true;
    }

    static bool parse_color_args(Value *args, int argCount, int start, Color *outColor)
    {
        if (!outColor || start >= argCount)
            return false;

        int remain = argCount - start;
        if (remain == 1 && args[start].isNativeStructInstance())
        {
            NativeStructInstance *inst = args[start].asNativeStructInstance();
            if (!inst || !inst->data)
                return false;
            *outColor = *(Color *)inst->data;
            return true;
        }

        if ((remain == 3 || remain == 4) &&
            args[start + 0].isNumber() &&
            args[start + 1].isNumber() &&
            args[start + 2].isNumber())
        {
            outColor->r = (unsigned char)(int)args[start + 0].asNumber();
            outColor->g = (unsigned char)(int)args[start + 1].asNumber();
            outColor->b = (unsigned char)(int)args[start + 2].asNumber();
            outColor->a = (unsigned char)(remain == 4 ? (int)args[start + 3].asNumber() : 255);
            return true;
        }

        return false;
    }

    static int add_graph_from_image(const Image &image, const char *name)
    {
        if (!image.data || image.width <= 0 || image.height <= 0)
            return -1;

        Texture2D tex = LoadTextureFromImage(image);
        if (tex.id == 0)
            return -1;

        Graph g = {};
        g.id = (int)gGraphLib.graphs.size();
        g.texture = (int)gGraphLib.textures.size();
        g.width = tex.width;
        g.height = tex.height;
        g.clip = {0, 0, (float)tex.width, (float)tex.height};
        strncpy(g.name, (name && name[0]) ? name : "image", MAXNAME - 1);
        g.name[MAXNAME - 1] = '\0';
        g.points.push_back({(float)tex.width * 0.5f, (float)tex.height * 0.5f});

        gGraphLib.graphs.push_back(g);
        gGraphLib.textures.push_back(tex);
        return g.id;
    }

    static bool replace_graph_texture(int graphId, const Image &image, bool forceFullClip)
    {
        if (!image.data || image.width <= 0 || image.height <= 0)
            return false;
        if (graphId < 0 || graphId >= (int)gGraphLib.graphs.size())
            return false;

        Graph &g = gGraphLib.graphs[graphId];
        int texId = g.texture;
        if (texId < 0 || texId >= (int)gGraphLib.textures.size())
            return false;

        Texture2D &oldTex = gGraphLib.textures[texId];
        int oldW = oldTex.width;
        int oldH = oldTex.height;

        Texture2D newTex = LoadTextureFromImage(image);
        if (newTex.id == 0)
            return false;

        UnloadTexture(oldTex);
        oldTex = newTex;

        bool wasFullTexture = ((int)g.clip.x == 0 && (int)g.clip.y == 0 &&
                               (int)g.clip.width == oldW &&
                               (int)g.clip.height == oldH);

        if (forceFullClip || wasFullTexture)
        {
            g.width = newTex.width;
            g.height = newTex.height;
            g.clip = {0, 0, (float)newTex.width, (float)newTex.height};
            Vector2 center = {(float)newTex.width * 0.5f, (float)newTex.height * 0.5f};
            if (g.points.empty())
                g.points.push_back(center);
            else
                g.points[0] = center;
        }

        return true;
    }

    static ScriptImage *as_script_image(Value value)
    {
        if (!value.isNativeClassInstance())
            return nullptr;

        NativeClassInstance *inst = value.asNativeClassInstance();
        if (!inst || !inst->klass || inst->klass != gImageClassDef)
            return nullptr;

        return (ScriptImage *)inst->userData;
    }

    static int push_image_instance(Interpreter *vm, ScriptImage *img)
    {
        if (!vm || !img || !gImageClassDef)
        {
            if (vm)
                vm->pushNil();
            return 1;
        }

        Value literal = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = literal.asNativeClassInstance();
        instance->klass = gImageClassDef;
        instance->userData = (void *)img;
        vm->push(literal);
        return 1;
    }

    static void *native_image_ctor(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("Image expects 2 number arguments (width, height)");
            return nullptr;
        }

        int width = (int)args[0].asNumber();
        int height = (int)args[1].asNumber();

        ScriptImage *img = new ScriptImage();
        if (!make_blank_image(img, width, height, 4))
        {
            delete img;
            Error("Failed to create Image(%d, %d)", width, height);
            return nullptr;
        }
        return img;
    }

    static void native_image_dtor(Interpreter *vm, void *instance)
    {
        (void)vm;
        ScriptImage *img = (ScriptImage *)instance;
        if (!img)
            return;
        free_image_data(img);
        delete img;
    }

    static int native_image_get_width(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        ScriptImage *img = (ScriptImage *)data;
        if (argCount != 0)
        {
            Error("Image.get_width expects no arguments");
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt(has_image_data(img) ? img->image.width : 0);
        return 1;
    }

    static int native_image_get_height(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        ScriptImage *img = (ScriptImage *)data;
        if (argCount != 0)
        {
            Error("Image.get_height expects no arguments");
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt(has_image_data(img) ? img->image.height : 0);
        return 1;
    }

    static int native_image_get_bpp(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        ScriptImage *img = (ScriptImage *)data;
        if (argCount != 0)
        {
            Error("Image.get_bpp expects no arguments");
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt(has_image_data(img) ? img->bpp : 0);
        return 1;
    }

    static int native_image_set_pixel(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.set_pixel called on invalid image");
            return 0;
        }

        if (!(argCount == 3 || argCount == 5 || argCount == 6))
        {
            Error("Image.set_pixel expects (x, y, color) or (x, y, r, g, b[, a])");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("Image.set_pixel expects numeric x and y");
            return 0;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        if (x < 0 || y < 0 || x >= img->image.width || y >= img->image.height)
        {
            return 0;
        }

        Color c = WHITE;
        if (!parse_color_args(args, argCount, 2, &c))
        {
            Error("Image.set_pixel color expects Color or r,g,b[,a]");
            return 0;
        }

        ImageDrawPixel(&img->image, x, y, c);
        return 0;
    }

    static int native_image_get_pixel(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("Image.get_pixel expects 2 number arguments (x, y)");
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            return 4;
        }

        if (!has_image_data(img))
        {
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            return 4;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        if (x < 0 || y < 0 || x >= img->image.width || y >= img->image.height)
        {
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            return 4;
        }

        Color c = GetImageColor(img->image, x, y);
        vm->pushInt((int)c.r);
        vm->pushInt((int)c.g);
        vm->pushInt((int)c.b);
        vm->pushInt((int)c.a);
        return 4;
    }

    static int native_image_fill(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.fill called on invalid image");
            return 0;
        }

        if (!(argCount == 1 || argCount == 3 || argCount == 4))
        {
            Error("Image.fill expects Color or r,g,b[,a]");
            return 0;
        }

        Color c = WHITE;
        if (!parse_color_args(args, argCount, 0, &c))
        {
            Error("Image.fill color expects Color or r,g,b[,a]");
            return 0;
        }

        ImageClearBackground(&img->image, c);
        return 0;
    }

    static int native_image_resize(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.resize called on invalid image");
            return 0;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("Image.resize expects 2 number arguments (width, height)");
            return 0;
        }

        int w = (int)args[0].asNumber();
        int h = (int)args[1].asNumber();
        if (w <= 0 || h <= 0)
        {
            Error("Image.resize expects positive width and height");
            return 0;
        }

        ImageResize(&img->image, w, h);
        img->bpp = bpp_from_format(img->image.format);
        return 0;
    }

    static int native_image_resize_nn(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.resize_nn called on invalid image");
            return 0;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("Image.resize_nn expects 2 number arguments (width, height)");
            return 0;
        }

        int w = (int)args[0].asNumber();
        int h = (int)args[1].asNumber();
        if (w <= 0 || h <= 0)
        {
            Error("Image.resize_nn expects positive width and height");
            return 0;
        }

        ImageResizeNN(&img->image, w, h);
        img->bpp = bpp_from_format(img->image.format);
        return 0;
    }

    static int native_image_flip_horizontal(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.flip_horizontal called on invalid image");
            return 0;
        }
        if (argCount != 0)
        {
            Error("Image.flip_horizontal expects no arguments");
            return 0;
        }
        ImageFlipHorizontal(&img->image);
        return 0;
    }

    static int native_image_flip_vertical(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.flip_vertical called on invalid image");
            return 0;
        }
        if (argCount != 0)
        {
            Error("Image.flip_vertical expects no arguments");
            return 0;
        }
        ImageFlipVertical(&img->image);
        return 0;
    }

    static int native_image_rotate(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.rotate called on invalid image");
            return 0;
        }
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("Image.rotate expects 1 number argument (degrees)");
            return 0;
        }

        int degrees = (int)args[0].asNumber();
        ImageRotate(&img->image, degrees);
        img->bpp = bpp_from_format(img->image.format);
        return 0;
    }

    static int native_image_rotate_cw(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.rotate_cw called on invalid image");
            return 0;
        }
        if (argCount != 0)
        {
            Error("Image.rotate_cw expects no arguments");
            return 0;
        }
        ImageRotateCW(&img->image);
        img->bpp = bpp_from_format(img->image.format);
        return 0;
    }

    static int native_image_rotate_ccw(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            Error("Image.rotate_ccw called on invalid image");
            return 0;
        }
        if (argCount != 0)
        {
            Error("Image.rotate_ccw expects no arguments");
            return 0;
        }
        ImageRotateCCW(&img->image);
        img->bpp = bpp_from_format(img->image.format);
        return 0;
    }

    static int native_image_load(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (!img)
        {
            vm->pushBool(false);
            return 1;
        }
        if (argCount != 1 || !args[0].isString())
        {
            Error("Image.load expects 1 string argument (path)");
            vm->pushBool(false);
            return 1;
        }

        bool ok = load_image_from_file(img, args[0].asStringChars());
        vm->pushBool(ok);
        return 1;
    }

    static int native_image_save(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (argCount != 1 || !args[0].isString())
        {
            Error("Image.save expects 1 string argument (path)");
            vm->pushBool(false);
            return 1;
        }
        if (!has_image_data(img))
        {
            vm->pushBool(false);
            return 1;
        }
        bool ok = ExportImage(img->image, args[0].asStringChars());
        vm->pushBool(ok);
        return 1;
    }

    static int native_image_to_graph(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (!has_image_data(img))
        {
            vm->pushInt(-1);
            return 1;
        }
        if (!(argCount == 0 || (argCount == 1 && args[0].isString())))
        {
            Error("Image.to_graph expects 0 or 1 argument ([name])");
            vm->pushInt(-1);
            return 1;
        }

        char autoName[64];
        const char *name = nullptr;
        if (argCount == 1)
        {
            name = args[0].asStringChars();
        }
        else
        {
            std::snprintf(autoName, sizeof(autoName), "image_%p", (void *)img);
            name = autoName;
        }

        int graphId = add_graph_from_image(img->image, name);
        vm->pushInt(graphId);
        return 1;
    }

    static int native_image_update_graph(Interpreter *vm, void *data, int argCount, Value *args)
    {
        ScriptImage *img = (ScriptImage *)data;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("Image.update_graph expects 1 number argument (graphId)");
            vm->pushBool(false);
            return 1;
        }
        if (!has_image_data(img))
        {
            vm->pushBool(false);
            return 1;
        }

        int graphId = (int)args[0].asNumber();
        vm->pushBool(replace_graph_texture(graphId, img->image, false));
        return 1;
    }

    static int native_create_image(Interpreter *vm, int argCount, Value *args)
    {
        if (!(argCount == 2 || argCount == 3))
        {
            Error("create_image expects 2 or 3 arguments (w, h, [bpp])");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || (argCount == 3 && !args[2].isNumber()))
        {
            Error("create_image expects numeric arguments (w, h, [bpp])");
            vm->pushNil();
            return 1;
        }

        int w = (int)args[0].asNumber();
        int h = (int)args[1].asNumber();
        int bpp = (argCount == 3) ? (int)args[2].asNumber() : 4;

        ScriptImage *img = new ScriptImage();
        if (!make_blank_image(img, w, h, bpp))
        {
            delete img;
            Error("create_image failed (w=%d, h=%d, bpp=%d). bpp must be 1,2,3,4", w, h, bpp);
            vm->pushNil();
            return 1;
        }

        return push_image_instance(vm, img);
    }

    static int native_create_image_from_file(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isString())
        {
            Error("create_image_from_file expects 1 string argument (path)");
            vm->pushNil();
            return 1;
        }

        ScriptImage *img = new ScriptImage();
        if (!load_image_from_file(img, args[0].asStringChars()))
        {
            delete img;
            vm->pushNil();
            return 1;
        }

        return push_image_instance(vm, img);
    }

    static int native_load_image(Interpreter *vm, int argCount, Value *args)
    {
        if (!(argCount == 1 || argCount == 2))
        {
            Error("load_image expects 1 or 2 arguments (image, [name])");
            vm->pushInt(-1);
            return 1;
        }
        ScriptImage *img = as_script_image(args[0]);
        if (!img || !has_image_data(img))
        {
            Error("load_image expects an Image instance as first argument");
            vm->pushInt(-1);
            return 1;
        }
        if (argCount == 2 && !args[1].isString())
        {
            Error("load_image second argument must be string name");
            vm->pushInt(-1);
            return 1;
        }

        if (argCount == 1)
        {
            bool ok = replace_graph_texture(0, img->image, true);
            vm->pushInt(ok ? 0 : -1);
            return 1;
        }

        const char *name = args[1].asStringChars();

        int graphId = add_graph_from_image(img->image, name);
        vm->pushInt(graphId);
        return 1;
    }

    static int native_get_image_info(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isString())
        {
            Error("get_image_info expects 1 string argument (path)");
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            return 3;
        }

        Image img = LoadImage(args[0].asStringChars());
        if (!img.data)
        {
            vm->pushInt(0);
            vm->pushInt(0);
            vm->pushInt(0);
            return 3;
        }

        vm->pushInt(img.width);
        vm->pushInt(img.height);
        vm->pushInt(bpp_from_format(img.format));
        UnloadImage(img);
        return 3;
    }

    void registerAll(Interpreter &vm)
    {
        gImageClassDef = vm.registerNativeClass(
            "Image",
            native_image_ctor,
            native_image_dtor,
            2,
            false);

        vm.addNativeMethod(gImageClassDef, "get_width", native_image_get_width);
        vm.addNativeMethod(gImageClassDef, "get_height", native_image_get_height);
        vm.addNativeMethod(gImageClassDef, "get_bpp", native_image_get_bpp);
        vm.addNativeMethod(gImageClassDef, "set_pixel", native_image_set_pixel);
        vm.addNativeMethod(gImageClassDef, "draw_pixel", native_image_set_pixel);
        vm.addNativeMethod(gImageClassDef, "get_pixel", native_image_get_pixel);
        vm.addNativeMethod(gImageClassDef, "fill", native_image_fill);
        vm.addNativeMethod(gImageClassDef, "clear", native_image_fill);
        vm.addNativeMethod(gImageClassDef, "resize", native_image_resize);
        vm.addNativeMethod(gImageClassDef, "resize_nn", native_image_resize_nn);
        vm.addNativeMethod(gImageClassDef, "flip_horizontal", native_image_flip_horizontal);
        vm.addNativeMethod(gImageClassDef, "flip_vertical", native_image_flip_vertical);
        vm.addNativeMethod(gImageClassDef, "flip_x", native_image_flip_horizontal);
        vm.addNativeMethod(gImageClassDef, "flip_y", native_image_flip_vertical);
        vm.addNativeMethod(gImageClassDef, "rotate", native_image_rotate);
        vm.addNativeMethod(gImageClassDef, "rotate_cw", native_image_rotate_cw);
        vm.addNativeMethod(gImageClassDef, "rotate_ccw", native_image_rotate_ccw);
        vm.addNativeMethod(gImageClassDef, "load", native_image_load);
        vm.addNativeMethod(gImageClassDef, "save", native_image_save);
        vm.addNativeMethod(gImageClassDef, "to_graph", native_image_to_graph);
        vm.addNativeMethod(gImageClassDef, "update_graph", native_image_update_graph);

        vm.registerNative("create_image", native_create_image, -1);
        vm.registerNative("create_image_from_file", native_create_image_from_file, 1);
        vm.registerNative("image_from_file", native_create_image_from_file, 1);
        vm.registerNative("load_image", native_load_image, -1);
        vm.registerNative("get_image_info", native_get_image_info, 1);
    }

}
