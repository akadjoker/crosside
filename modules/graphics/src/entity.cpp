#include "engine.hpp"
#include "math.hpp"
#include <math.h>

extern GraphLib gGraphLib;
extern Scene gScene;


void Entity::setPosition(double newX, double newY)
{
    x = newX;
    y = newY;
    markTransformDirty();
    bounds_dirty = true;
}

void Entity::setAngle(double newAngle)
{
    angle = newAngle;
    markTransformDirty();
    bounds_dirty = true;
}

void Entity::setSize(double newSize)
{
    size = newSize;
    markTransformDirty();
    bounds_dirty = true;
}

void Entity::setCenter(float cx, float cy)
{
    center_x = cx;
    center_y = cy;
    markTransformDirty();
    bounds_dirty = true;
}

void Entity::markTransformDirty()
{
    worldMatrixDirty = true;

    for (auto *child : childsBack)
        child->markTransformDirty();
    for (auto *child : childFront)
        child->markTransformDirty();
}

double Entity::getX()
{
    return GetWorldTransformation().tx;
}

double Entity::getY()
{
    return GetWorldTransformation().ty;
}

double Entity::getAngle()
{
    Matrix2D world = GetWorldTransformation();
    return atan2(world.b, world.a) * RAD2DEG;
}

Vector2 Entity::getLocalPoint(double px, double py)
{
    float scale_final = (float)size / 100.0f;

    Matrix2D mat = GetRelativeTransformation(
        (float)x, (float)y,
        scale_final, scale_final,
        0.0f, 0.0f,
        center_x, center_y,
        angle);
    return mat.TransformCoords((float)px, (float)py);
}

Vector2 Entity::getWorldPoint(double x, double y)
{
    return GetWorldTransformation().TransformCoords(x,y);
}

Vector2 Entity::getRealPoint(int pointIdx)
{
    Graph *g = gGraphLib.getGraph(graph);
    if (!g || pointIdx < 0 || pointIdx >= (int)g->points.size())
        return {0, 0};

    Vector2 point = g->points[pointIdx];

    return GetAbsoluteTransformation().TransformCoords(point);
}


bool Entity::collide(Entity *other)
{

    if (other == nullptr)
        return false;
    if (!shape || !other->shape)
        return false;

    return shape->collide(other->shape, GetWorldTransformation(), other->GetWorldTransformation());
}

static uint32 EIDS = 0;

Entity::Entity()
{
    shape = nullptr;
    id = 0;
    graph = 0;
    layer = 0;
    x = 0;
    y = 0;
    last_x = 0;
    last_y = 0;
    flip_x = false;
    flip_y = false;
    angle = 0;
    size = 100;
    size_x = 1.0;
    size_y = 1.0;

    center_x = -1; // POINT_UNDEFINED
    center_y = -1; // POINT_UNDEFINED
    angle = 0;
    color = WHITE;
    flags = B_VISIBLE | B_COLLISION;

    bounds_dirty = true;
    collision_layer = 1;         // Layer 1 por default
    collision_mask = 0xFFFFFFFF; // Colide com todas por default
}

Entity::~Entity()
{
    for (size_t i = 0; i < childFront.size(); i++)
    {
        delete childFront[i];
    }
    childFront.clear();
    for (size_t i = 0; i < childsBack.size(); i++)
    {
        delete childsBack[i];
    }
    childsBack.clear();
    if (shape)
        delete shape;
}

void Entity::moveBy(double x, double y)
{
    // Guarda última posição
    last_x = this->x;
    last_y = this->y;

    int moveX = (int)round(x);
    int moveY = (int)round(y);

    // Sem shape/collision? Move direto
    if (!shape || !(flags & B_COLLISION))
    {
        this->x += moveX;
        this->y += moveY;
        bounds_dirty = true;
        return;
    }

    // Query área do movimento
    updateBounds();
    Rectangle moveBounds = bounds;
    moveBounds.x += fmin(0, moveX);
    moveBounds.y += fmin(0, moveY);
    moveBounds.width += fabs(moveX);
    moveBounds.height += fabs(moveY);

    std::vector<Entity *> nearby;
    gScene.staticTree->query(moveBounds, nearby);
    for (Entity *dyn : gScene.dynamicEntities)
        if (dyn != this)
            nearby.push_back(dyn);

    // Move X pixel-a-pixel
    if (moveX != 0)
    {
        int sign = (moveX > 0) ? 1 : -1;
        while (moveX != 0)
        {
            this->x += sign;
            updateBounds();

            for (Entity *other : nearby)
            {
                if (!other->shape || !(other->flags & B_COLLISION))
                    continue;
                if (!canCollideWith(other))
                    continue;
                if (collide(other))
                {
                    this->x -= sign;
                }
            }
            moveX -= sign;
        }
    }

    // Move Y pixel-a-pixel
    if (moveY != 0)
    {
        int sign = (moveY > 0) ? 1 : -1;
        while (moveY != 0)
        {
            this->y += sign;
            updateBounds();

            for (Entity *other : nearby)
            {
                if (!other->shape || !(other->flags & B_COLLISION))
                    continue;
                if (!canCollideWith(other))
                    continue;
                if (collide(other))
                {
                    this->y -= sign;
                }
            }
            moveY -= sign;
        }
    }

    bounds_dirty = true;
}

Rectangle Entity::getBounds()
{
    if (bounds_dirty)
        updateBounds();
    return bounds;
}



Matrix2D Entity::GetWorldTransformation() const
{
    // Se já temos cache válido, retorna
    if (!worldMatrixDirty)
        return cachedWorldMatrix;

    // Calcular nova matriz
    Matrix2D localMat = GetAbsoluteTransformation();

    if (parent)
    {
        Matrix2D parentMat = parent->GetWorldTransformation();
        cachedWorldMatrix = Matrix2DMult(localMat, parentMat);
    }
    else
    {
        cachedWorldMatrix = localMat;
    }

    worldMatrixDirty = false;
    return cachedWorldMatrix;
}

Matrix2D Entity::GetAbsoluteTransformation() const
{
    Layer &l = gScene.layers[layer];
    float finalX = (float)(x - l.scroll_x);
    float finalY = (float)(y - l.scroll_y);
    float scale_final = (float)size / 100.0f;

   // Info("Entity %d absolute transformation: pos=(%f, %f) scroll=(%f, %f) size=%f", id, finalX, finalY, l.scroll_x, l.scroll_y, scale_final);
 

    return GetRelativeTransformation(
        finalX, finalY,
        scale_final, scale_final,
        0.0f, 0.0f,
        center_x, center_y,
        angle);
}

 

 
Vector2 Entity::getPoint(int pointIdx) const
{
    Graph *g = gGraphLib.getGraph(graph);
    if (!g || pointIdx < 0 || pointIdx >= (int)g->points.size())
        return {0, 0};

    Vector2 &p = g->points[pointIdx];
    return {(float)p.x, (float)p.y};
}

void Entity::render()
{

    if(graph == -1)
    {
         // Renderizar filhos de trás
        for (Entity *child : childsBack)
        {
            if (child->isVisible() && !child->isDead() && child->ready)
                child->render();
        }
        // Renderizar filhos da frente
        for (Entity *child : childFront)
        {
            if (child->isVisible() && !child->isDead() && child->ready)
                child->render();
        }

        return;
    }
    Graph *g = gGraphLib.getGraph(graph);
    if (!g)
        return;

    Texture2D tex = gGraphLib.textures[g->texture];

    // Inicializar centro se necessário
    if (center_x == -1 && center_y == -1)
    {
         center_x = g->points[0].x;
         center_y = g->points[0].y;
    }

    const Matrix2D &matrix = GetWorldTransformation();

    // Renderizar filhos de trás
    for (Entity *child : childsBack)
    {
        if (child->isVisible() && !child->isDead() && child->ready)
            child->render();
    }

    // Renderizar este entity
    RenderTransformFlipClip(tex, g->clip, flip_x, flip_y, color, &matrix, 0);
  

//   Vec2 offset;
//   offset.x = center_x;
//   offset.y = center_y;
//   float w = g->clip.width;
//     float h = g->clip.height;
//   RenderTransformFlipClipOffset(tex, offset, w, h, g->clip, flip_x, flip_y, color, &matrix, 0);


    // Renderizar filhos da frente
    for (Entity *child : childFront)
    {
        if (child->isVisible() && !child->isDead() && child->ready)
            child->render();
    }
}

void Entity::setRectangleShape(int x, int y, int w, int h)
{
    if (shape)
    {
        delete shape;
    }
    shape = new RectangleShape(x, y, w, h);
    updateBounds();
}

void Entity::setCircleShape(float radius)
{
    if (shape)
    {
        delete shape;
    }
    shape = new CircleShape();
    ((CircleShape *)shape)->radius = radius;
    updateBounds();
}

void Entity::setShape(Vector2 *points, int n)
{
    if (shape)
    {
        delete shape;
    }
    shape = new PolygonShape(n);

    PolygonShape *polygon = (PolygonShape *)shape;
    for (int i = 0; i < n; i++)
    {
        polygon->points[i] = points[i];
    }
    polygon->calcNormals();
    updateBounds();
}