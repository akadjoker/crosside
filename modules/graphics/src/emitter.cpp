#include "engine.hpp"
#include <cmath>
#include <cstdlib>
#include <raymath.h>

ParticleSystem gParticleSystem;
extern GraphLib gGraphLib;
extern Scene gScene;

// Utility functions
inline float randf()
{
    return (float)rand() / (float)RAND_MAX;
}

inline float randf(float min, float max)
{
    return min + randf() * (max - min);
}

inline Color lerpColor(Color a, Color b, float t)
{
    return Color{
        (unsigned char)((1.0f - t) * a.r + t * b.r),
        (unsigned char)((1.0f - t) * a.g + t * b.g),
        (unsigned char)((1.0f - t) * a.b + t * b.b),
        (unsigned char)((1.0f - t) * a.a + t * b.a)};
}

// ============================================================================
// Emitter Implementation
// ============================================================================

Emitter::Emitter(EmitterType t, int gr, int maxParticles)
    : type(t), graph(gr), layer(0)
{
    // Posição e direção
    pos = {0, 0};
    dir = {1, 0};
    spread = 0.5f;

    // Emissão
    rate = 50.0f;
    speedMin = 50.0f;
    speedMax = 150.0f;
    particleLife = 1.0f;
    lifetime = 0.5f;

    // Aparência
    colorStart = WHITE;
    colorEnd = WHITE;
    sizeStart = 4.0f;
    sizeEnd = 0.0f;

    // Física
    gravity = {0, 0};
    drag = 0.0f;

    // Rotação
    rotationMin = 0.0f;
    rotationMax = 0.0f;
    angularVelMin = 0.0f;
    angularVelMax = 0.0f;

    // Rendering
    blendMode = BLEND_ALPHA;

    // Estado
    active = true;
    finished = false;
    elapsed = 0.0f;
    accumulator = 0.0f;
    aliveCount = 0;
    firstDead = 0;

    // Aloca partículas
    particles.resize(maxParticles);
    for (auto &p : particles)
    {
        p.alive = false;
    }
}

void Emitter::emitAt(int index)
{
    Particle &p = particles[index];

    p.alive = true;
    p.pos = pos;
    p.maxLife = particleLife;
    p.life = particleLife;
    p.startColor = colorStart;
    p.endColor = colorEnd;
    p.startSize = sizeStart;
    p.endSize = sizeEnd;
    p.size = sizeStart;
    p.color = colorStart;
    p.acc = {0, 0};

    // Velocidade com spread
    float baseAngle = atan2f(dir.y, dir.x);
    float angle = baseAngle + (randf() - 0.5f) * spread;
    float speed = randf(speedMin, speedMax);
    p.vel = {cosf(angle) * speed, sinf(angle) * speed};

    // Rotação
    p.rotation = randf(rotationMin, rotationMax);
    p.angularVel = randf(angularVelMin, angularVelMax);

    aliveCount++;
}

void Emitter::emit()
{
    // Otimização: se pool cheio, não procura
    if (aliveCount >= (int)particles.size())
    {
        return;
    }

    // Procura a partir de firstDead
    for (int i = firstDead; i < (int)particles.size(); i++)
    {
        if (!particles[i].alive)
        {
            emitAt(i);
            firstDead = i + 1;
            return;
        }
    }

    // Se não achou, procura do início
    for (int i = 0; i < firstDead; i++)
    {
        if (!particles[i].alive)
        {
            emitAt(i);
            firstDead = i + 1;
            return;
        }
    }
}

// void Emitter::update(float dt)
// {
//     elapsed += dt;

//     // Emissão
//     if (active)
//     {
//         if (type == EMITTER_ONESHOT && elapsed >= lifetime)
//         {
//             active = false;

//         }

//         if (type == EMITTER_CONTINUOUS || elapsed < lifetime)
//         {
//             accumulator += rate * dt;

//             while (accumulator >= 1.0f)
//             {
//                 emit();
//                 accumulator -= 1.0f;
//                 if (type == EMITTER_ONESHOT)
//                 {
//                     printf("ONESHOT: Emitted particle, aliveCount=%d\n", aliveCount);
//                 }
//             }
//         }
//     }

//     // Atualiza partículas
//     firstDead = 0;

//     for (int i = 0; i < (int)particles.size(); i++)
//     {
//         Particle &p = particles[i];

//         if (!p.alive)
//         {
//             if (firstDead == i)
//                 firstDead = i + 1;
//             continue;
//         }

//         p.life -= dt;
//         if (p.life <= 0.0f)
//         {
//             p.alive = false;
//             aliveCount--;
//             if (firstDead == i)
//                 firstDead = i + 1;
//             continue;
//         }

//         float t = 1.0f - (p.life / p.maxLife);
//         p.color = lerpColor(p.startColor, p.endColor, t);
//         p.size = p.startSize + t * (p.endSize - p.startSize);

//         p.vel.x += gravity.x * dt;
//         p.vel.y += gravity.y * dt;
//         p.vel.x += p.acc.x * dt;
//         p.vel.y += p.acc.y * dt;

//         if (drag > 0.0f)
//         {
//             float dragFactor = 1.0f - drag * dt;
//             if (dragFactor < 0.0f)
//                 dragFactor = 0.0f;
//             p.vel.x *= dragFactor;
//             p.vel.y *= dragFactor;
//         }

//         p.pos.x += p.vel.x * dt;
//         p.pos.y += p.vel.y * dt;
//         p.rotation += p.angularVel * dt;
//     }

//     // Marca finished quando ONESHOT terminou e não há partículas
//     if (type == EMITTER_ONESHOT && !active && aliveCount == 0)
//     {
//         if (!finished)
//         {

//         }
//         finished = true;
//     }
// }
void Emitter::update(float dt)
{
    elapsed += dt;

    // ONESHOT: emite tudo no primeiro update
    if (type == EMITTER_ONESHOT && active && elapsed <= dt)
    {
        int burstCount = (int)(rate * lifetime);
        if (burstCount > (int)particles.size())
        {
            burstCount = (int)particles.size();
        }
       // printf("ONESHOT: Bursting %d particles\n", burstCount);
        burst(burstCount);
        active = false;
    }

    // CONTINUOUS: emissão normal
    if (type == EMITTER_CONTINUOUS && active)
    {
        accumulator += rate * dt;
        while (accumulator >= 1.0f)
        {
            emit();
            accumulator -= 1.0f;
        }
    }

    firstDead = 0;

    for (int i = 0; i < (int)particles.size(); i++)
    {
        Particle &p = particles[i];

        if (!p.alive)
        {
            if (firstDead == i)
                firstDead = i + 1;
            continue;
        }

        p.life -= dt;
        if (p.life <= 0.0f)
        {
            p.alive = false;
            aliveCount--;
            if (firstDead == i)
                firstDead = i + 1;
            continue;
        }

        float t = 1.0f - (p.life / p.maxLife);
        p.color = lerpColor(p.startColor, p.endColor, t);
        p.size = p.startSize + t * (p.endSize - p.startSize);

        p.vel.x += gravity.x * dt;
        p.vel.y += gravity.y * dt;

        if (drag > 0.0f)
        {
            float dragFactor = 1.0f - drag * dt;
            if (dragFactor < 0.0f)
                dragFactor = 0.0f;
            p.vel.x *= dragFactor;
            p.vel.y *= dragFactor;
        }

        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.rotation += p.angularVel * dt;
    }

    // Marca finished
    if (!active && aliveCount == 0)
    {
        finished = true;
    }
}

void Emitter::draw()
{
    Graph *g = gGraphLib.getGraph(graph);

    Texture2D tex = gGraphLib.textures[g->texture];

    float scroll_x = gScene.layers[layer].scroll_x;
    float scroll_y = gScene.layers[layer].scroll_y;

    // Bounds da tela para culling
    float screenW = (float)gScene.width;
    float screenH = (float)gScene.height;

    // Blend mode
    int raylibBlendMode = BLEND_ALPHA;
    switch (blendMode)
    {
    case BLEND_ADDITIVE:
        raylibBlendMode = BLEND_ADDITIVE;
        break;
    case BLEND_MULTIPLIED:
        raylibBlendMode = BLEND_MULTIPLIED;
        break;
    default:
        raylibBlendMode = BLEND_ALPHA;
        break;
    }

    BeginBlendMode(raylibBlendMode);

    for (auto &p : particles)
    {
        if (!p.alive)
        {
            continue;
        }

        // Vector2 screenPos =
        // {
        //     p.pos.x - scroll_x,
        //     p.pos.y - scroll_y};

        // // Culling simples
        // float margin = p.size * 2.0f;
        // if (screenPos.x < -margin || screenPos.x > screenW + margin ||
        //     screenPos.y < -margin || screenPos.y > screenH + margin)
        // {
        //     continue;
        // }

         

           DrawTexturePro(tex, g->clip, (Rectangle){p.pos.x - scroll_x, p.pos.y - scroll_y, p.size, p.size},
                       (Vector2){p.size / 2.0f, p.size / 2.0f}, p.rotation, p.color);
                       
        // DrawTexturePro(tex, g->clip, (Rectangle){p.pos.x - scroll_x, p.pos.y - scroll_y, p.size, p.size},
        //                (Vector2){g->clip.width / 2.0f, g->clip.height / 2.0f}, p.rotation, p.color);
    }

    EndBlendMode();
}

void Emitter::burst(int count)
{
    for (int i = 0; i < count; i++)
    {
        emit();
    }
}

void Emitter::stop()
{
    active = false;
}

void Emitter::restart()
{
    active = true;
    finished = false;
    elapsed = 0.0f;
    accumulator = 0.0f;
}

// ============================================================================
// Particle System Implementation
// ============================================================================

ParticleSystem::ParticleSystem()
{
}

ParticleSystem::~ParticleSystem()
{
    clear();
}

Emitter *ParticleSystem::spawn(EmitterType type, int graph, int maxParticles)
{
    Emitter *e = new Emitter(type, graph, maxParticles);
    emitters.push_back(e);
    return e;
}
// ============================================================================
// FOLHAS CAINDO (leaves)
// ============================================================================
Emitter *ParticleSystem::createFallingLeaves(Vector2 pos, int graph, float width)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 30);
    e->pos = pos;
    e->spawnZone = {-width/2, 0, width, 10};
    e->dir = {0, 1};
    e->spread = 0.5f;
    e->rate = 5.0f;  // Bem devagar
    e->speedMin = 30.0f;
    e->speedMax = 60.0f;
    e->particleLife = 5.0f;

    e->colorStart = (Color){150, 200, 80, 255};   // Verde
    e->colorEnd = (Color){200, 150, 50, 255};     // Marrom
    e->sizeStart = 3.0f;
    e->sizeEnd = 3.0f;

    e->gravity = {0, 20.0f};  // Cai devagar
    e->drag = 0.95f;

    e->angularVelMin = -3.0f;
    e->angularVelMax = 3.0f;

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// PEGADAS/POEIRA DOS PÉS (footstep dust)
// ============================================================================
Emitter *ParticleSystem::createFootstepDust(Vector2 pos, int graph)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 8);
    e->pos = pos;
    e->dir = {0, -0.5f};
    e->spread = PI;  // 180 graus
    e->rate = 1000.0f;
    e->speedMin = 20.0f;
    e->speedMax = 50.0f;
    e->particleLife = 0.5f;
    e->lifetime = 0.01f;

    e->colorStart = ColorAlpha(BEIGE, 0.6f);
    e->colorEnd = ColorAlpha(BROWN, 0.0f);
    e->sizeStart = 2.0f;
    e->sizeEnd = 5.0f;

    e->gravity = {0, -10.0f};  // Sobe levemente
    e->drag = 0.9f;

    e->blendMode = BLEND_ALPHA;

    return e;
}
// ============================================================================
// CONJURAÇÃO MÁGICA (magic casting)
// ============================================================================
Emitter *ParticleSystem::createMagicCast(Vector2 pos, int graph, Color magicColor)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 50);
    e->pos = pos;
    e->dir = {0, 0};
    e->spread = 2.0f * PI;
    e->rate = 80.0f;
    e->speedMin = 30.0f;
    e->speedMax = 80.0f;
    e->particleLife = 0.5f;

    e->colorStart = ColorAlpha(magicColor, 0.9f);
    e->colorEnd = ColorAlpha(WHITE, 0.0f);
    e->sizeStart = 5.0f;
    e->sizeEnd = 0.5f;

    e->gravity = {0, -80.0f};  // Sobe em espiral
    e->drag = 0.2f;

    e->angularVelMin = -8.0f;
    e->angularVelMax = 8.0f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

// ============================================================================
// PORTAL/TELETRANSPORTE
// ============================================================================
Emitter *ParticleSystem::createPortal(Vector2 pos, int graph)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 60);
    e->pos = pos;
    e->dir = {0, 0};
    e->spread = 2.0f * PI;
    e->rate = 100.0f;
    e->speedMin = 10.0f;
    e->speedMax = 40.0f;
    e->particleLife = 1.2f;

    e->colorStart = (Color){100, 100, 255, 255};  // Azul elétrico
    e->colorEnd = (Color){200, 100, 255, 0};      // Roxo
    e->sizeStart = 6.0f;
    e->sizeEnd = 1.0f;

    e->gravity = {0, 0};
    e->drag = 0.8f;

    e->angularVelMin = -10.0f;
    e->angularVelMax = 10.0f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}
// ============================================================================
// MUZZLE FLASH (flash do disparo de arma)
// ============================================================================
Emitter *ParticleSystem::createMuzzleFlash(Vector2 pos, int graph, Vector2 shootDirection)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 8);
    e->pos = pos;
    e->dir = shootDirection;
    e->spread = 0.5f;  // Cone estreito na direção do tiro
    e->rate = 1000.0f;
    e->speedMin = 80.0f;
    e->speedMax = 150.0f;
    e->particleLife = 0.1f;  // Muito rápido!
    e->lifetime = 0.01f;

    e->colorStart = (Color){255, 255, 200, 255};  // Branco amarelado
    e->colorEnd = (Color){255, 150, 0, 0};        // Laranja -> transparente
    e->sizeStart = 22.0f;
    e->sizeEnd = 2.0f;

    e->gravity = {0, 0};
    e->drag = 0.95f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

// ============================================================================
// CASQUILHOS EJETADOS (shell ejection)
// ============================================================================
Emitter *ParticleSystem::createShellEjection(Vector2 pos, int graph, bool facingRight)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 1);  // Só 1 casquilho
    e->pos = pos;
    e->dir = facingRight ? Vector2{1, -0.5f} : Vector2{-1, -0.5f};
    e->spread = 0.3f;
    e->rate = 1000.0f;
    e->speedMin = 100.0f;
    e->speedMax = 150.0f;
    e->particleLife = 0.8f;
    e->lifetime = 0.01f;

    e->colorStart = (Color){200, 180, 100, 255};  // Cor de bronze
    e->colorEnd = (Color){150, 130, 80, 255};
    e->sizeStart = 2.0f;
    e->sizeEnd = 2.0f;

    e->gravity = {0, 600.0f};  // Cai como objeto sólido
    e->drag = 0.3f;

    e->angularVelMin = -15.0f;  // Roda muito!
    e->angularVelMax = 15.0f;

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// TRACER DE BALA (rastro luminoso da bala)
// ============================================================================
Emitter *ParticleSystem::createBulletTracer(Vector2 startPos, Vector2 endPos, int graph)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 5);
    Vector2 direction = Vector2Normalize(Vector2Subtract(endPos, startPos));
    
    e->pos = startPos;
    e->dir = direction;
    e->spread = 0.05f;  // Quase linha reta
    e->rate = 1000.0f;
    e->speedMin = 2000.0f;  // Muito rápido!
    e->speedMax = 2500.0f;
    e->particleLife = 0.15f;
    e->lifetime = 0.01f;

    e->colorStart = (Color){255, 255, 100, 255};
    e->colorEnd = (Color){255, 200, 0, 0};
    e->sizeStart = 3.0f;
    e->sizeEnd = 0.5f;

    e->gravity = {0, 0};
    e->drag = 0.0f;  // Sem arrasto

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

// ============================================================================
// RICOCHETE (quando bala bate na parede)
// ============================================================================
Emitter *ParticleSystem::createRicochet(Vector2 pos, int graph, Vector2 normal)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 15);
    e->pos = pos;
    e->dir = normal;  // Reflete na direção da normal
    e->spread = 1.2f;
    e->rate = 1000.0f;
    e->speedMin = 100.0f;
    e->speedMax = 250.0f;
    e->particleLife = 0.4f;
    e->lifetime = 0.01f;

    e->colorStart = (Color){255, 255, 150, 255};  // Faísca amarela
    e->colorEnd = (Color){255, 100, 0, 0};
    e->sizeStart = 2.0f;
    e->sizeEnd = 0.5f;

    e->gravity = {0, 400.0f};
    e->drag = 0.5f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}
// ============================================================================
// ESCUDO DE ENERGIA (hit effect)
// ============================================================================
Emitter *ParticleSystem::createShieldHit(Vector2 pos, int graph, Vector2 hitDirection)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 25);
    e->pos = pos;
    e->dir = Vector2Scale(hitDirection, -1);  // Oposto ao impacto
    e->spread = PI / 2;  // 90 graus
    e->rate = 1000.0f;
    e->speedMin = 50.0f;
    e->speedMax = 150.0f;
    e->particleLife = 0.5f;
    e->lifetime = 0.01f;

    e->colorStart = (Color){100, 200, 255, 255};  // Azul claro
    e->colorEnd = (Color){255, 255, 255, 0};
    e->sizeStart = 8.0f;
    e->sizeEnd = 2.0f;

    e->gravity = {0, 0};
    e->drag = 0.9f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}
// ============================================================================
// NUVEM DE POEIRA (dust cloud - ao cair algo pesado)
// ============================================================================
Emitter *ParticleSystem::createDustCloud(Vector2 pos, int graph, float radius)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 40);
    e->pos = pos;
    e->dir = {0, 0};
    e->spread = 2.0f * PI;
    e->rate = 1000.0f;
    e->speedMin = 50.0f;
    e->speedMax = 150.0f;
    e->particleLife = 1.5f;
    e->lifetime = 0.01f;

    e->colorStart = ColorAlpha(BEIGE, 0.7f);
    e->colorEnd = ColorAlpha(BROWN, 0.0f);
    e->sizeStart = 3.0f;
    e->sizeEnd = 15.0f;  // Expande muito

    e->gravity = {0, 0};
    e->drag = 0.95f;  // Para aos poucos

    e->blendMode = BLEND_ALPHA;

    return e;
}
// ============================================================================
// IMPACTO NO CHÃO (quando player aterrissa)
// ============================================================================
Emitter *ParticleSystem::createLandingDust(Vector2 pos, int graph, bool facingRight)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 15);
    e->pos = pos;
    e->dir = facingRight ? Vector2{1, -0.3f} : Vector2{-1, -0.3f}; // Direção lateral
    e->spread = 0.8f;
    e->rate = 1000.0f;
    e->speedMin = 30.0f;
    e->speedMax = 80.0f;
    e->particleLife = 0.4f;
    e->lifetime = 0.01f;

    e->colorStart = ColorAlpha(BEIGE, 0.8f);
    e->colorEnd = ColorAlpha(BROWN, 0.0f);
    e->sizeStart = 4.0f;
    e->sizeEnd = 8.0f; // Expande

    e->gravity = {0, 50.0f}; // Cai devagar
    e->drag = 0.8f; // Muita resistência

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// PARTÍCULAS DE PAREDE (quando colide com parede lateral)
// ============================================================================
Emitter *ParticleSystem::createWallImpact(Vector2 pos, int graph, bool hitFromLeft,float size_start,float size_end)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 10);
    e->pos = pos;
    e->dir = hitFromLeft ? Vector2{-1, 0} : Vector2{1, 0}; // Oposto à colisão
    e->spread = 1.0f;
    e->rate = 1000.0f;
    e->speedMin = 50.0f;
    e->speedMax = 120.0f;
    e->particleLife = 0.3f;
    e->lifetime = 0.01f;

    e->colorStart = (Color){200, 200, 200, 255};
    e->colorEnd = ColorAlpha(GRAY, 0.0f);
    e->sizeStart = size_start;
    e->sizeEnd = size_end;

    e->gravity = {0, 300.0f};
    e->drag = 0.6f;

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// RASTRO DE CORRIDA (trail contínuo enquanto anda)
// ============================================================================
Emitter *ParticleSystem::createRunTrail(Vector2 pos, int graph,float size_start,float size_end)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 30);
    e->pos = pos;
    e->dir = {0, 0}; // Espalhado
    e->spread = 2.0f * PI; // 360 graus
    e->rate = 30.0f;
    e->speedMin = 5.0f;
    e->speedMax = 20.0f;
    e->particleLife = 0.3f;

    e->colorStart = ColorAlpha(LIGHTGRAY, 0.5f);
    e->colorEnd = ColorAlpha(GRAY, 0.0f);
    e->sizeStart = size_start;
    e->sizeEnd = size_end;

    e->gravity = {0, 0};
    e->drag = 0.9f; // Para rápido

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// COLETA DE ITEM (brilho mágico)
// ============================================================================
Emitter *ParticleSystem::createCollectEffect(Vector2 pos, int graph, Color itemColor)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 25);
    e->pos = pos;
    e->dir = {0, -1}; // Para cima
    e->spread = 2.0f * PI;
    e->rate = 1000.0f;
    e->speedMin = 50.0f;
    e->speedMax = 150.0f;
    e->particleLife = 0.8f;
    e->lifetime = 0.01f;

    e->colorStart = itemColor;
    e->colorEnd = ColorAlpha(WHITE, 0.0f);
    e->sizeStart = 6.0f;
    e->sizeEnd = 1.0f;

    e->gravity = {0, -100.0f}; // Sobem
    e->drag = 0.4f;

    e->angularVelMin = -3.0f;
    e->angularVelMax = 3.0f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

// ============================================================================
// PARTÍCULAS DE DANO (quando toma dano)
// ============================================================================
Emitter *ParticleSystem::createBloodSplatter(Vector2 pos, int graph, Vector2 hitDirection)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 20);
    e->pos = pos;
    e->dir = hitDirection; // Direção do impacto
    e->spread = 1.2f;
    e->rate = 1000.0f;
    e->speedMin = 80.0f;
    e->speedMax = 200.0f;
    e->particleLife = 0.6f;
    e->lifetime = 0.01f;

    e->colorStart = (Color){180, 0, 0, 255}; // Vermelho
    e->colorEnd = (Color){100, 0, 0, 0};
    e->sizeStart = 4.0f;
    e->sizeEnd = 2.0f;

    e->gravity = {0, 400.0f}; // Queda
    e->drag = 0.7f;

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// BRILHO DE POWER-UP (aura contínua)
// ============================================================================
Emitter *ParticleSystem::createPowerUpAura(Vector2 pos, int graph, Color auraColor)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 40);
    e->pos = pos;
    e->dir = {0, -1};
    e->spread = 2.0f * PI; // 360 graus
    e->rate = 40.0f;
    e->speedMin = 20.0f;
    e->speedMax = 60.0f;
    e->particleLife = 1.0f;

    e->colorStart = auraColor;
    e->colorEnd = ColorAlpha(auraColor, 0.0f);
    e->sizeStart = 3.0f;
    e->sizeEnd = 0.5f;

    e->gravity = {0, -30.0f}; // Sobem devagar
    e->drag = 0.3f;

    e->angularVelMin = -2.0f;
    e->angularVelMax = 2.0f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

// ============================================================================
// CHUVA (ambiente)
// ============================================================================
Emitter *ParticleSystem::createRain(Vector2 pos, int graph, float width)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 200);
    e->pos = pos;
    e->spawnZone = {-width/2, 0, width, 10}; // Zona horizontal larga
    e->dir = {0, 1}; // Para baixo
    e->spread = 0.1f;
    e->rate = 150.0f;
    e->speedMin = 300.0f;
    e->speedMax = 400.0f;
    e->particleLife = 2.0f;

    e->colorStart = ColorAlpha(SKYBLUE, 0.6f);
    e->colorEnd = ColorAlpha(BLUE, 0.3f);
    e->sizeStart = 1.0f;
    e->sizeEnd = 1.0f;

    e->gravity = {0, 200.0f};
    e->drag = 0.0f;

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// RESPINGOS DE ÁGUA (ao cair na água)
// ============================================================================
Emitter *ParticleSystem::createWaterSplash(Vector2 pos, int graph)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 30);
    e->pos = pos;
    e->dir = {0, -1}; // Para cima
    e->spread = PI; // 180 graus
    e->rate = 1000.0f;
    e->speedMin = 100.0f;
    e->speedMax = 250.0f;
    e->particleLife = 0.8f;
    e->lifetime = 0.01f;

    e->colorStart = ColorAlpha(SKYBLUE, 0.9f);
    e->colorEnd = ColorAlpha(BLUE, 0.0f);
    e->sizeStart = 5.0f;
    e->sizeEnd = 2.0f;

    e->gravity = {0, 600.0f}; // Queda rápida
    e->drag = 0.5f;

    e->blendMode = BLEND_ALPHA;

    return e;
}

// ============================================================================
// PARTÍCULAS DE VELOCIDADE (quando correndo muito rápido)
// ============================================================================
Emitter *ParticleSystem::createSpeedLines(Vector2 pos, int graph, Vector2 velocity)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 20);
    e->pos = pos;
    e->dir = Vector2Normalize(Vector2Scale(velocity, -1)); // Oposto ao movimento
    e->spread = 0.3f;
    e->rate = 50.0f;
    e->speedMin = 50.0f;
    e->speedMax = 100.0f;
    e->particleLife = 0.2f;

    e->colorStart = ColorAlpha(WHITE, 0.7f);
    e->colorEnd = ColorAlpha(WHITE, 0.0f);
    e->sizeStart = 3.0f;
    e->sizeEnd = 1.0f;

    e->gravity = {0, 0};
    e->drag = 0.9f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

// ============================================================================
// BRILHO DE MOEDA/ESTRELA (rotating sparkle)
// ============================================================================
Emitter *ParticleSystem::createSparkle(Vector2 pos, int graph)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 15);
    e->pos = pos;
    e->dir = {0, 0};
    e->spread = 2.0f * PI;
    e->rate = 10.0f;
    e->speedMin = 10.0f;
    e->speedMax = 30.0f;
    e->particleLife = 0.6f;

    e->colorStart = (Color){255, 255, 100, 255}; // Dourado
    e->colorEnd = ColorAlpha(YELLOW, 0.0f);
    e->sizeStart = 4.0f;
    e->sizeEnd = 1.0f;

    e->gravity = {0, -20.0f}; // Sobe levemente
    e->drag = 0.5f;

    e->angularVelMin = -5.0f;
    e->angularVelMax = 5.0f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}
Emitter *ParticleSystem::createExplosion(Vector2 pos, int graph, Color color)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 50);
    e->pos = pos;
    e->dir = {1, 0};
    e->spread = 2.0f * PI; // 360 graus
    e->rate = 1000.0f;     // Emite tudo de uma vez
    e->speedMin = 100.0f;
    e->speedMax = 300.0f;
    e->particleLife = 0.8f;
    e->lifetime = 0.01f; // Burst rápido

    e->colorStart = color;
    e->colorEnd = ColorAlpha(color, 0.0f);
    e->sizeStart = 8.0f;
    e->sizeEnd = 0.0f;

    e->gravity = {0, 2.0f}; // Queda
    e->drag = 0.5f;

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

Emitter *ParticleSystem::createSmoke(Vector2 pos, int graph)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 100);
    e->pos = pos;
    e->dir = {0, -1}; // Para cima
    e->spread = 0.3f;
    e->rate = 20.0f;
    e->speedMin = 20.0f;
    e->speedMax = 50.0f;
    e->particleLife = 2.0f;

    e->colorStart = ColorAlpha(GRAY, 0.6f);
    e->colorEnd = ColorAlpha(DARKGRAY, 0.0f);
    e->sizeStart = 4.0f;
    e->sizeEnd = 12.0f; // Cresce

    e->gravity = {0, -20.0f}; // Sobe
    e->drag = 0.3f;

    e->angularVelMin = -1.0f;
    e->angularVelMax = 1.0f;

    e->blendMode = BLEND_ALPHA;

    return e;
}

Emitter *ParticleSystem::createFire(Vector2 pos, int graph)
{
    Emitter *e = spawn(EMITTER_CONTINUOUS, graph, 80);
    e->pos = pos;
    e->dir = {0, -1}; // Para cima
    e->spread = 0.4f;
    e->rate = 60.0f;
    e->speedMin = 50.0f;
    e->speedMax = 100.0f;
    e->particleLife = 0.6f;

    e->colorStart = (Color){255, 200, 50, 255}; // Amarelo
    e->colorEnd = (Color){255, 50, 0, 0};       // Vermelho -> transparente
    e->sizeStart = 6.0f;
    e->sizeEnd = 2.0f;

    e->gravity = {0, -50.0f}; // Sobe

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

Emitter *ParticleSystem::createSparks(Vector2 pos, int graph, Color color)
{
    Emitter *e = spawn(EMITTER_ONESHOT, graph, 30);
    e->pos = pos;
    e->dir = {0, -1}; // Para cima
    e->spread = PI;   // 180 graus
    e->rate = 1000.0f;
    e->speedMin = 150.0f;
    e->speedMax = 300.0f;
    e->particleLife = 0.5f;
    e->lifetime = 0.01f;

    e->colorStart = color;
    e->colorEnd = ColorAlpha(color, 0.0f);
    e->sizeStart = 3.0f;
    e->sizeEnd = 0.5f;

    e->gravity = {0, 500.0f}; // Queda forte

    e->blendMode = BLEND_ADDITIVE;

    return e;
}

void ParticleSystem::update(float dt)
{
    for (size_t i = 0; i < emitters.size(); i++)
    {
        emitters[i]->update(dt);
    }
}

void ParticleSystem::cleanup()
{
    for (size_t i = 0; i < emitters.size();)
    {
        if (emitters[i]->finished)
        {
            delete emitters[i];
            emitters[i] = emitters.back();
            emitters.pop_back();
        }
        else
        {
            i++;
        }
    }
}

void ParticleSystem::draw()
{
    for (auto &e : emitters)
    {
        e->draw();
    }
}

void ParticleSystem::clear()
{
    for (auto e : emitters)
    {
        delete e;
    }
    emitters.clear();
}

int ParticleSystem::getTotalParticles() const
{
    int total = 0;
    for (auto e : emitters)
    {
        total += e->getAliveCount();
    }
    return total;
}
