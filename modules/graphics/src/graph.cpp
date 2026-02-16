#include "engine.hpp"
#include "div.h"
#include <cstdio>
#include <algorithm>

static int ReadPalette(FILE *fp, Color *palette)
{
    uint8_t colors[768];

    if (fread(colors, 1, 768, fp) != 768)
    {
        return 0;
    }

    // Convert 6-bit color values to 8-bit
    for (int i = 0; i < 256; i++)
    {
        palette[i].r = colors[i * 3 + 0] << 2;
        palette[i].g = colors[i * 3 + 1] << 2;
        palette[i].b = colors[i * 3 + 2] << 2;
        palette[i].a = 255;
    }

    return 1;
}

static int ReadPaletteWithGamma(FILE *fp, Color *palette)
{
    if (!ReadPalette(fp, palette))
    {
        return 0;
    }

    // Skip gamma correction data
    fseek(fp, 576, SEEK_CUR);

    return 1;
}

void GraphLib::create()
{
    Image image = GenImageChecked(32, 32, 4, 4, WHITE, BLACK);
    defaultTexture = LoadTextureFromImage(image);
    UnloadImage(image);

    Graph g = {};
    g.id = 0;
    g.texture = 0;
    g.width = defaultTexture.width;
    g.height = defaultTexture.height;
    g.clip = {0, 0, (float)g.width, (float)g.height};
    strncpy(g.name, "default", MAXNAME - 1);
    g.points.push_back({(float)defaultTexture.width / 2, (float)defaultTexture.height / 2});

    graphs.push_back(g);
    textures.push_back(defaultTexture);
}

int GraphLib::loadDIV(const char *filename)
{
    has_palette = 0;
    FILE *fp;
    char header[8];
    int bpp;

    fp = fopen(filename, "rb");
    if (!fp)
    {
        TraceLog(LOG_ERROR, "LoadFPG: Cannot open file %s", filename);
        return -1;
    }

    // Read and validate header
    if (fread(header, 1, 8, fp) != 8)
    {
        fclose(fp);
        TraceLog(LOG_ERROR, "LoadFPG: Cannot read header");
        return -1;
    }

    // Determine bit depth from magic number
    if (memcmp(header, F32_MAGIC, 7) == 0)
        bpp = 32;
    else if (memcmp(header, F16_MAGIC, 7) == 0)
        bpp = 16;
    else if (memcmp(header, FPG_MAGIC, 7) == 0)
        bpp = 8;
    else if (memcmp(header, F01_MAGIC, 7) == 0)
        bpp = 1;
    else
    {
        fclose(fp);
        TraceLog(LOG_ERROR, "LoadFPG: Invalid magic number");
        return -1;
    }

    if (bpp == 8)
    {
        if (!ReadPaletteWithGamma(fp, palette))
        {
            fclose(fp);
            TraceLog(LOG_ERROR, "LoadFPG: Cannot read palette");
            return -1;
        }
        has_palette = 1;
        palette[0].a = 0; // First color is transparent
    }

    int num_graphics = 0;

    // Read each graphic chunk
    while (!feof(fp))
    {
        struct
        {
            uint32_t code;
            uint32_t regsize;
            char name[32];
            char fpname[12];
            uint32_t width;
            uint32_t height;
            uint32_t flags;
        } chunk;

        if (fread(&chunk, 1, sizeof(chunk), fp) != sizeof(chunk))
        {
            break;
        }

        // Arrange byte order
        ARRANGE_DWORD(&chunk.code);
        ARRANGE_DWORD(&chunk.regsize);
        ARRANGE_DWORD(&chunk.width);
        ARRANGE_DWORD(&chunk.height);
        ARRANGE_DWORD(&chunk.flags);

        Graph g = {};
        g.id = (int)graphs.size();
        g.texture = (int)textures.size();
        g.width = chunk.width;
        g.height = chunk.height;
        g.clip = {0, 0, (float)chunk.width, (float)chunk.height};
        strncpy(g.name, chunk.name, MAXNAME - 1);
        int  ncpoints =chunk.flags;
  
        // Read control points
        if (ncpoints > 0)
        {
            
                for (int c = 0; c < ncpoints; c++)
                {
                    int16_t px, py;
                    fread(&px, sizeof(int16_t), 1, fp);
                    fread(&py, sizeof(int16_t), 1, fp);
                    ARRANGE_WORD(&px);
                    ARRANGE_WORD(&py);

                    if (px == -1 && py == -1)
                    {
                        g.points.push_back({(float)g.width / 2, (float)g.height / 2});
                    }
                    else
                    {
                        g.points.push_back({(float)px, (float)py});
                    }
                }
            
        }

        // Calculate bytes per line
        int widthb = (chunk.width * bpp + 7) / 8;

        // Allocate pixel data
        int format;
        int pixelSize;

        switch (bpp)
        {
        case 32:
            format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            pixelSize = 4;
            break;
        case 16:
            format = PIXELFORMAT_UNCOMPRESSED_R5G6B5;
            pixelSize = 2;
            break;
        case 8:
            format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8; // We'll convert to RGBA
            pixelSize = 4;
            break;
        case 1:
            format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8; // We'll convert to RGBA
            pixelSize = 4;
            break;
        default:
            format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            pixelSize = 4;
            break;
        }

        // Create image
        Image image;
        image.width = chunk.width;
        image.height = chunk.height;
        image.format = format;
        image.mipmaps = 1;

        // For 8-bit and 1-bit, we need to convert to RGBA
        if (bpp == 8 || bpp == 1)
        {
            image.data = malloc(chunk.width * chunk.height * 4);
            uint8_t *dest = (uint8_t *)image.data;

            for (int y = 0; y < chunk.height; y++)
            {
                uint8_t *line = (uint8_t *)malloc(widthb);
                fread(line, 1, widthb, fp);

                for (int x = 0; x < chunk.width; x++)
                {
                    uint8_t colorIndex;

                    if (bpp == 1)
                    {
                        int bytePos = x / 8;
                        int bitPos = 7 - (x % 8);
                        colorIndex = (~line[bytePos] >> bitPos) & 1;
                    }
                    else
                    {
                        colorIndex = line[x];
                    }

                    int destPos = (y * chunk.width + x) * 4;

                    if (has_palette)
                    {
                        dest[destPos + 0] = palette[colorIndex].r;
                        dest[destPos + 1] = palette[colorIndex].g;
                        dest[destPos + 2] = palette[colorIndex].b;
                        dest[destPos + 3] = palette[colorIndex].a;
                    }
                    else
                    {
                        // Grayscale
                        dest[destPos + 0] = colorIndex;
                        dest[destPos + 1] = colorIndex;
                        dest[destPos + 2] = colorIndex;
                        dest[destPos + 3] = 255;
                    }
                }

                free(line);
            }
        }
        else
        {
            // 16-bit or 32-bit - direct read
            image.data = malloc(chunk.width * chunk.height * pixelSize);

            for (int y = 0; y < chunk.height; y++)
            {
                uint8_t *dest = (uint8_t *)image.data + y * chunk.width * pixelSize;
                fread(dest, pixelSize, chunk.width, fp);

                // Arrange byte order for 16-bit and 32-bit
                if (bpp == 16)
                {
                    uint16_t *pixels = (uint16_t *)dest;
                    for (int x = 0; x < chunk.width; x++)
                    {
                        ARRANGE_WORD(&pixels[x]);
                    }
                }
                else if (bpp == 32)
                {
                    uint32_t *pixels = (uint32_t *)dest;
                    for (int x = 0; x < chunk.width; x++)
                    {
                        ARRANGE_DWORD(&pixels[x]);
                    }
                }
            }
        }

        Texture2D tex = LoadTextureFromImage(image);
        UnloadImage(image);
        graphs.push_back(g);
        textures.push_back(tex);

        num_graphics++;

        TraceLog(LOG_INFO, "LoadFPG: Loaded graphic %d '%s' (%dx%d) - Total graphics: %d",chunk.code, g.name, chunk.width, chunk.height,num_graphics-1);
    }

    fclose(fp);
    return num_graphics;
}

int GraphLib::load(const char *name, const char *texturePath)
{
    Image img = LoadImage(texturePath);
    if (img.data == nullptr)
    {
        return -1;
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    Graph g = {};
    g.id = (int)graphs.size();
    g.texture = (int)textures.size();
    g.width = tex.width;
    g.height = tex.height;
    g.clip = {0, 0, (float)tex.width, (float)tex.height};
    strncpy(g.name, name, MAXNAME - 1);
    g.name[MAXNAME - 1] = '\0';
    g.points.push_back({(float)tex.width / 2, (float)tex.height / 2});

    graphs.push_back(g);
    textures.push_back(tex);

    return g.id;
}

int GraphLib::loadAtlas(const char *name, const char *texturePath, int count_x, int count_y)
{
    Image img = LoadImage(texturePath);
    if (img.data == nullptr)
    {
        return -1;
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    int tile_w = tex.width / count_x;
    int tile_h = tex.height / count_y;
    int firstId = (int)graphs.size();

    for (int y = 0; y < count_y; y++)
    {
        for (int x = 0; x < count_x; x++)
        {
            Graph g = {};
            g.id = (int)graphs.size();
            g.texture = (int)textures.size();
            g.width = tile_w;
            g.height = tile_h;
            g.clip = {(float)(x * tile_w), (float)(y * tile_h), (float)tile_w, (float)tile_h};
            snprintf(g.name, MAXNAME, "%s_%d_%d", name, x, y);
            g.name[MAXNAME - 1] = '\0';
            g.points.push_back({(float)tile_w / 2, (float)tile_h / 2});

            graphs.push_back(g);
        }
    }

    textures.push_back(tex);
    return firstId;
}

int GraphLib::addSubGraph(int parentId, const char *name, int x, int y, int w, int h)
{
    if (parentId < 0 || parentId >= (int)graphs.size())
        return -1;

    Graph parent = graphs[parentId];

    Graph g = {};
    g.id = (int)graphs.size();
    g.texture = parent.texture; // Reutiliza a MESMA textura!
    g.width = w;
    g.height = h;
    g.clip = {(float)x, (float)y, (float)w, (float)h};
    strncpy(g.name, name, MAXNAME - 1);
    g.name[MAXNAME - 1] = '\0';
    g.points.push_back({(float)w / 2, (float)h / 2});

    graphs.push_back(g);
    return g.id;
}

Graph *GraphLib::getGraph(int id)
{
    if (id < 0 || id >= (int)graphs.size())
        return &graphs[0];
    return &graphs[id];
}

Texture2D *GraphLib::getTexture(int id)
{
    if (id < 0 || id >= (int)textures.size())
        return nullptr;
    return &textures[id];
}

void GraphLib::DrawGraph(int id, float x, float y, Color tint)
{
    Graph *g = getGraph(id);
    Texture2D *tex = getTexture(g->texture);
    if (tex)
    {
        DrawTextureRec(*tex, g->clip, {x, y}, tint);
    }
}

bool GraphLib::savePak(const char *pakFile)
{
    FILE *f = fopen(pakFile, "wb");
    if (!f)
        return false;

    // Header
    PakHeader header;
    memcpy(header.magic, PAK_MAGIC, sizeof(header.magic));
    header.version = PAK_VERSION;
    header.textureCount = (int)textures.size();
    header.graphCount = (int)graphs.size();

    if (fwrite(&header, sizeof(PakHeader), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    // ===== SAVE UNIQUE TEXTURES =====
    for (size_t texIdx = 0; texIdx < textures.size(); texIdx++)
    {
        Texture2D &tex = textures[texIdx];

        // Lê os pixels da VRAM
        Image img = LoadImageFromTexture(tex);

        // Texture header
        PakTextureHeader texHeader;
        snprintf(texHeader.name, MAXNAME, "tex_%zu", texIdx);
        texHeader.width = img.width;
        texHeader.height = img.height;
        texHeader.size = img.width * img.height * 4;

        if (fwrite(&texHeader, sizeof(PakTextureHeader), 1, f) != 1)
        {
            UnloadImage(img);
            fclose(f);
            return false;
        }

        // Pixels (RGBA)
        Image rgba = ImageCopy(img);
        ImageFormat(&rgba, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        if (fwrite(rgba.data, 1, texHeader.size, f) != (size_t)texHeader.size)
        {
            UnloadImage(img);
            UnloadImage(rgba);
            fclose(f);
            return false;
        }

        UnloadImage(img);
        UnloadImage(rgba);
    }

    // ===== SAVE GRAPHS (COM REFERÊNCIAS À TEXTURA) =====
    for (auto &g : graphs)
    {
        PakGraphHeader graphHeader;
        strncpy(graphHeader.name, g.name, MAXNAME - 1);
        graphHeader.name[MAXNAME - 1] = '\0';

        graphHeader.texture = g.texture;
        graphHeader.clip_x = g.clip.x;
        graphHeader.clip_y = g.clip.y;
        graphHeader.clip_w = g.clip.width;
        graphHeader.clip_h = g.clip.height;
        graphHeader.point_count = (int)g.points.size();

        if (fwrite(&graphHeader, sizeof(PakGraphHeader), 1, f) != 1)
        {
            fclose(f);
            return false;
        }

        // Save points
        for (auto &p : g.points)
        {
            if (fwrite(&p, sizeof(Vector2), 1, f) != 1)
            {
                fclose(f);
                return false;
            }
        }
    }

    fclose(f);
    return true;
}

bool GraphLib::loadPak(const char *pakFile)
{
    FILE *f = fopen(pakFile, "rb");
    if (!f)
        return false;

    // Header
    PakHeader header;
    if (fread(&header, sizeof(PakHeader), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    // Verifica
    if (memcmp(header.magic, PAK_MAGIC, sizeof(header.magic)) != 0 || header.version != PAK_VERSION)
    {
        fclose(f);
        return false;
    }

    // Limpa
    destroy();

    // ===== LOAD UNIQUE TEXTURES =====
    for (int i = 0; i < header.textureCount; i++)
    {
        PakTextureHeader texHeader;
        if (fread(&texHeader, sizeof(PakTextureHeader), 1, f) != 1)
        {
            fclose(f);
            return false;
        }

        unsigned char *pixels = (unsigned char *)malloc(texHeader.size);
        if (fread(pixels, 1, texHeader.size, f) != (size_t)texHeader.size)
        {
            delete[] pixels;
            fclose(f);
            return false;
        }
        printf("Loaded texture %s (%d x %d)\n", texHeader.name, texHeader.width, texHeader.height);

        Image img = {};
        img.data = pixels;
        img.width = texHeader.width;
        img.height = texHeader.height;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

        Texture2D tex = LoadTextureFromImage(img);
        textures.push_back(tex);

        UnloadImage(img);
    }

    // ===== LOAD GRAPHS (QUE REFERENCIAM AS TEXTURAS) =====
    for (int i = 0; i < header.graphCount; i++)
    {
        PakGraphHeader graphHeader;
        if (fread(&graphHeader, sizeof(PakGraphHeader), 1, f) != 1)
        {
            fclose(f);
            return false;
        }

        Graph g = {};
        g.id = (int)graphs.size();
        g.texture = graphHeader.texture; // Aponta à textura deduplicated!
        g.width = (int)graphHeader.clip_w;
        g.height = (int)graphHeader.clip_h;
        g.clip = {graphHeader.clip_x, graphHeader.clip_y, graphHeader.clip_w, graphHeader.clip_h};
        strncpy(g.name, graphHeader.name, MAXNAME - 1);
        g.name[MAXNAME - 1] = '\0';

        // Load points
        for (int p = 0; p < graphHeader.point_count; p++)
        {
            Vector2 pt;
            if (fread(&pt, sizeof(Vector2), 1, f) != 1)
            {
                fclose(f);
                return false;
            }
            g.points.push_back(pt);
        }

        graphs.push_back(g);
    }

    fclose(f);
    return true;
}

void GraphLib::destroy()
{
    for (size_t i = 0; i < textures.size(); i++)
    {
        UnloadTexture(textures[i]);
    }
    textures.clear();
    graphs.clear();
}
