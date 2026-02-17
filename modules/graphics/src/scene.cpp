#include "engine.hpp"
#include "render.hpp"
#include "tinyxml2.h"
#include "filebuffer.hpp"

extern GraphLib gGraphLib;
extern Scene gScene;

Entity *Scene::addEntity(int graphId, int layer, double x, double y)
{
    Entity *node = new Entity();
    Graph *g = gGraphLib.getGraph(graphId);
    node->graph = graphId;
    node->x = x;
    node->y = y;
    if (layer < 0 || layer >= MAX_LAYERS)
        layer = 0;
    node->layer = layer;
    node->id = layers[layer].nodes.size();
    layers[layer].nodes.push_back(node);
    return node;
}

void Scene::moveEntityToLayer(Entity *node, int layer)
{
    if (!node)
        return;

    if (layer < 0 || layer >= MAX_LAYERS)
        layer = 0;

    int srcLayer = node->layer;
    if (srcLayer < 0 || srcLayer >= MAX_LAYERS)
        return;

    if (srcLayer == layer)
        return;

    Layer &src = layers[srcLayer];
    uint32 idx = node->id;

    if (src.nodes.empty())
        return;

    // Defensive: id can be stale after script/runtime mistakes.
    if (idx >= src.nodes.size() || src.nodes[idx] != node)
    {
        bool found = false;
        for (uint32 i = 0; i < src.nodes.size(); ++i)
        {
            if (src.nodes[i] == node)
            {
                idx = i;
                found = true;
                break;
            }
        }
        if (!found)
            return;
    }

    // node a mover (pode ser o próprio "node")
    Entity *remove = src.nodes[idx];

    // swap-remove NO LAYER DE ORIGEM
    Entity *back = src.nodes.back();
    src.nodes[idx] = back;
    back->id = idx;
    src.nodes.pop_back();

    // adicionar ao destino
    remove->layer = (uint8)layer;
    remove->id = (uint32)layers[layer].nodes.size();
    layers[layer].nodes.push_back(remove);
}

void Scene::moveEntityToParent(Entity *node, Entity *newParent, bool front)
{
    if (!node || !newParent)
        return;

    if (node->layer < 0 || node->layer >= MAX_LAYERS)
        return;

    Layer &layer = layers[node->layer];
    uint32 idx = node->id;

    if (idx >= layer.nodes.size())
        return;

    Entity *last = layer.nodes.back();

    // swap-remove
    layer.nodes[idx] = last;
    last->id = idx;
    layer.nodes.pop_back();

    // Adicionar como filho do novo pai
    if (front)
        newParent->childFront.push_back(node);
    else
        newParent->childsBack.push_back(node);
    node->parent = newParent;
}

bool Scene::IsOutOfScreen(Entity *entity) const
{
    if (!entity)
        return true;

    // Verifica se a AABB da entidade está fora da tela
    Rectangle bounds = entity->getBounds();
    if (bounds.x + bounds.width < 0 || bounds.x > width ||
        bounds.y + bounds.height < 0 || bounds.y > height)
    {
        return true;
    }
    return false;
}

void Scene::removeEntity(Entity *node)
{
    if (!node)
        return;

    if (node->layer < 0 || node->layer >= MAX_LAYERS)
        return;

    Layer &layer = layers[node->layer];
    uint32 idx = node->id;

    if (idx >= layer.nodes.size())
        return;

    Entity *last = layer.nodes.back();

    // swap-remove
    layer.nodes[idx] = last;
    last->id = idx;
    layer.nodes.pop_back();
    node->userData = nullptr;

    // marca para destruir mais tarde
    nodesToRemove.push_back(node);
}

void Scene::destroy()
{
    if (staticTree)
    {
        delete staticTree;
        staticTree = nullptr;
    }
    for (int i = 0; i < MAX_LAYERS; i++)
        layers[i].destroy();
}

int Scene::addSolid(float x, float y, float w, float h, const std::string &name, int id)
{
    solids.push_back({{x, y, w, h}, name, id});
    return solids.size() - 1;
}

Solid *Scene::getSolid(int id)
{
    
    for (Solid &solid : solids)
    {
        if (solid.id == id)
            return &solid;
    }
    return nullptr;
}

bool Scene::ImportTileMap(const std::string &fileName)
{

    FileBuffer file;
    if (!file.load(fileName.c_str()))
    {
        TraceLog(LOG_ERROR, "Failed to load tile map file: %s", fileName.c_str());
        return false;
    } else 
    {
        TraceLog(LOG_INFO, "Tile map file loaded: %s", fileName.c_str());
    }

    // if (!FileExists(fileName.c_str()))
    // {
    //     Info("Tile map file not found: %s", fileName.c_str());
    //     return false;
    // }



    tinyxml2::XMLDocument document;
    document.Inport(file.c_str(), file.size());

    std::string filePath = GetDirectoryPath(fileName.c_str());

    tinyxml2::XMLElement *mapElem = document.FirstChildElement("map");
    if (!mapElem)
        return false;

    int mTileWidth;
    int mTileHeight;
    int mWidth;
    int mHeight;
    int mfirstgid;
    int mTileCount;
    int mColumns;

    std::string image;

    mTileWidth = mapElem->IntAttribute("tilewidth");
    mTileHeight = mapElem->IntAttribute("tileheight");
    mWidth = mapElem->IntAttribute("width");
    mHeight = mapElem->IntAttribute("height");
    // mfirstgid e mTileCount podem vir do tileset, mas por enquanto do map
    mfirstgid = mapElem->IntAttribute("firstgid", 1); // default 1
    mTileCount = mapElem->IntAttribute("tilecount", 0);
    // mColumns será extraído do tileset

    int graphId = -1;

   // graphId = gGraphLib.load("tiles.png", "assets/tiles.png");

    tinyxml2::XMLElement *tileSetElem = mapElem->FirstChildElement("tileset");
    while (tileSetElem)
    {
        tinyxml2::XMLElement *imageElement = tileSetElem->FirstChildElement();

        // Extrair columns do tileset
        mColumns = tileSetElem->IntAttribute("columns", 0);

        if (imageElement->FindAttribute("source") != nullptr)
        {
            image = imageElement->Attribute("source");

            std::string fullImagePath = filePath + "/" + image;
            std::string androidImagePath =  "assets/" + image;

            graphId = gGraphLib.load(GetFileNameWithoutExt(fullImagePath.c_str()), fullImagePath.c_str());
            if (graphId == -1)
            {
                graphId = gGraphLib.load(GetFileNameWithoutExt(androidImagePath.c_str()), androidImagePath.c_str());
                if (graphId == -1)
                {
                    TraceLog(LOG_ERROR, "Tile set image not found: %s or %s", fullImagePath.c_str(), androidImagePath.c_str());
                }
               
            }
            tileSetElem = tileSetElem->NextSiblingElement("tileset");
        }
    }

    TraceLog(LOG_INFO, "Load tile map %s Tile (%d,%d) Map (%d,%d) Columns: %d", fileName.c_str(), mTileWidth, mTileHeight, mWidth, mHeight, mColumns);

    int layer = 0;

    tinyxml2::XMLElement *layerElem = mapElem->FirstChildElement("layer");
    if (!layerElem)
        return false;
    while (layerElem)
    {

        std::string layerName = layerElem->Attribute("name");
        int width = layerElem->IntAttribute("width");
        int height = layerElem->IntAttribute("height");
   
         float offsetx = layerElem->FloatAttribute("offsetx");
         float offsety = layerElem->FloatAttribute("offsety");

        // void SetTileMap(int layer, int map_width, int map_height, int tile_width, int tile_height, int columns, int graph);
        // void SetTileMapSpacing(int layer, double spacing);
        // void SetTileMapMargin(int layer, double margin);
        // void SetTileMapMode(int layer, int mode);

        SetTileMap(layer, width, height, mTileWidth, mTileHeight, mColumns, graphId, offsetx, offsety);

        int id = layerElem->IntAttribute("id");
        //Info("Layer %s %d ", layerName.c_str(), id);

        if (!layerElem->FirstChildElement("data")->FirstChildElement("tile"))
        {
            tinyxml2::XMLElement *data = layerElem->FirstChildElement("data");
            if (data->Attribute("encoding") != nullptr)
            {
                std::string encoding = data->Attribute("encoding");
                std::string dataStr = data->GetText();
                SetTileMapFromString(layer, dataStr, 0);
            }
        }
        layerElem = layerElem->NextSiblingElement("layer");
        layer++;
    }

 

    tinyxml2::XMLElement* objElem = mapElem->FirstChildElement("objectgroup");
    if (objElem!=nullptr)
    {

        while(objElem)
        {
            std::string name = objElem->Attribute("name");
            int id                = objElem->IntAttribute("id");
         //   Info("Object %s %d ",name.c_str(),id);

                tinyxml2::XMLElement* obj = objElem->FirstChildElement("object");
                while(obj)
                {

                int id = obj->IntAttribute("id");

                float x = obj->FloatAttribute("x");
                float y = obj->FloatAttribute("y");
                float width = obj->FloatAttribute("width");
                float height = obj->FloatAttribute("height");

                addSolid(x,y,width,height,name,id);

               // Info("Object %d %f %f %f %f",id,x,y,width,height);

                obj = obj->NextSiblingElement("object");

            }

             objElem = objElem->NextSiblingElement("objectgroup");
            }
    }

    return true;
}

void Scene::setCollisionCallback(CollisionCallback callback, void *userdata)
{

    onCollision = callback;
    collisionUserData = userdata;
}
Scene::Scene()
{

    width = GetScreenWidth();
    height = GetScreenHeight();

    staticTree = nullptr;
    for (int i = 0; i < MAX_LAYERS; i++)
    {
        layers[i].back = -1;
        layers[i].front = -1;
        layers[i].tilemap = nullptr;

        layers[i].mode = LAYER_MODE_TILEX | LAYER_MODE_TILEY;

        layers[i].size.width = width;
        layers[i].size.height = height;
        layers[i].size.x = 0;
        layers[i].size.y = 0;

        layers[i].scroll_x = 0;
        layers[i].scroll_y = 0;
        layers[i].scroll_factor_x = 1;
        layers[i].scroll_factor_y = 1;
    }
}

Scene::~Scene()
{
    destroy();
}

void Layer::destroy()
{
    for (int i = 0; i < nodes.size(); i++)
    {
        delete nodes[i];
    }
    nodes.clear();

    if (tilemap)
    {
        delete tilemap;
        tilemap = nullptr;
    }
}

void Layer::render_parelax(Graph *g)
{
    if (!g)
        return;

    Texture2D bg_texture = gGraphLib.textures[g->texture];

    float screen_w = gScene.width;
    float screen_h = gScene.height;
    float tex_w = (float)bg_texture.width;
    float tex_h = (float)bg_texture.height;

    // Aplicar parallax
    float effective_scroll_x = (float)scroll_x * scroll_factor_x;
    float effective_scroll_y = (float)scroll_y * scroll_factor_y;

    //   printf("w %d %d\n",gScene.width,gScene.height);

    float uv_x1 = 0.0f, uv_y1 = 0.0f;
    float uv_x2 = 1.0f, uv_y2 = 1.0f;

    if (mode & LAYER_MODE_TILEX)
    {
        // Normaliza scroll para UV space
        uv_x1 = fmod(effective_scroll_x / tex_w, 1.0f);
        if (uv_x1 < 0)
            uv_x1 += 1.0f; // Handle negative scroll
        uv_x2 = uv_x1 + (screen_w / tex_w);
    }

    if (mode & LAYER_MODE_TILEY)
    {
        uv_y1 = fmod(effective_scroll_y / tex_h, 1.0f);
        if (uv_y1 < 0)
            uv_y1 += 1.0f;
        uv_y2 = uv_y1 + (screen_h / tex_h);
    }

    if (mode & LAYER_MODE_STRETCHX)
    {
        // Stretch mantém proporção 0-1 mas offset por scroll
        uv_x1 = effective_scroll_x / size.width;
        uv_x2 = (effective_scroll_x + screen_w) / size.width;
    }

    if (mode & LAYER_MODE_STRETCHY)
    {
        uv_y1 = effective_scroll_y / size.height;
        uv_y2 = (effective_scroll_y + screen_h) / size.height;
    }

    // Flips
    if (mode & LAYER_MODE_FLIPX)
        std::swap(uv_x1, uv_x2);

    if (mode & LAYER_MODE_FLIPY)
        std::swap(uv_y1, uv_y2);

    Rectangle src = {
        uv_x1 * tex_w,
        uv_y1 * tex_h,
        (uv_x2 - uv_x1) * tex_w,
        (uv_y2 - uv_y1) * tex_h};

    Rectangle dst = {
        0,
        0,
        screen_w,
        screen_h};

    // Desenhar
    DrawTexturePro(
        bg_texture,
        src,
        dst,
        {0, 0},
        0.0f,
        WHITE);

    // RenderQuadUV(0, 0, screen_w, screen_h,
    //              uv_x1, uv_y1, uv_x2, uv_y2, bg_texture);

    // DrawTexture(bg_texture, 0, 0, WHITE);
}

void Layer::render()
{
    if (!visible)
        return;
    if (back != -1)
    {

        Graph *g = gGraphLib.getGraph(back);
        render_parelax(g);
        // DrawRectangleLines(size.x, size.y, size.width, size.height, WHITE);
    }

    if (tilemap)
    {
        tilemap->render();
    }

    for (int i = 0; i < nodes.size(); i++)
    {
        Entity *e = nodes[i];
        if ((e->flags & B_VISIBLE) && !(e->flags & B_DEAD) && e->ready)
        {
            e->render();
        }
    }

    if (front != -1)
    {
        Graph *g = gGraphLib.getGraph(front);
        render_parelax(g);
    }

    // DrawRectangleLines(size.x, size.y, size.width , size.height  , WHITE);
}
