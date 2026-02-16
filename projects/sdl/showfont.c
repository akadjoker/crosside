/*
  showfont:  An example of using the SDL_ttf library with 2D graphics.
  Copyright (C) 2001-2019 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* A simple program to test the text rendering feature of the TTF library */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "unistd.h"
#define cwd getcwd
#define cd chdir


#include "SDL.h"
#include "SDL_ttf.h"

#define DEFAULT_PTSIZE  18
#define DEFAULT_TEXT    "The quick brown fox jumped over the lazy dog"
#define WIDTH   640
#define HEIGHT  480





int ChangeDirectory(const char *dir)
{
    int result = cd(dir);
    if (result != 0) SDL_Log("SYSTEM: Failed to change to directory: %s  %i \n", dir,result);
    return (result);
}
const char *GetWorkingDirectory(void)
{
    static char currentDir[512];
    memset(currentDir, 0, 512);

    char *ptr = cwd(currentDir, 512 - 1);

    return ptr;
}


typedef enum
{
    TextRenderSolid,
    TextRenderShaded,
    TextRenderBlended
} TextRenderMethod;

typedef struct {
    SDL_Texture *caption;
    SDL_Rect captionRect;
    SDL_Texture *message;
    SDL_Rect messageRect;
} Scene;

static void draw_scene(SDL_Renderer *renderer, Scene *scene)
{
    /* Clear the background to background color */
    SDL_SetRenderDrawColor(renderer, 0,0, 45, 0xFF);
    SDL_RenderClear(renderer);

    SDL_RenderCopy(renderer, scene->caption, NULL, &scene->captionRect);
    SDL_RenderCopy(renderer, scene->message, NULL, &scene->messageRect);
    SDL_RenderPresent(renderer);
}

static void cleanup(int exitcode)
{
    TTF_Quit();
    SDL_Quit();
    exit(exitcode);
}

int main(int argc, char *argv[])
{
    char *argv0 = argv[0];
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Surface *text = NULL;
    Scene scene;
    int ptsize;
    int i, done;
    SDL_Color white = { 0xFF, 0xFF, 0xFF, 0 };
    SDL_Color black = { 0x00, 0x00, 0x00, 0 };
    SDL_Color *forecol;
    SDL_Color *backcol;
    SDL_Event event;
    TextRenderMethod rendermethod;
    int renderstyle;
    int outline;
    int hinting;
    int kerning;
    int dump;
    enum {
        RENDER_LATIN1,
        RENDER_UTF8,
        RENDER_UNICODE
    } rendertype;
    char *message, string[128];

    /* Look for special execution mode */
    dump = 0;
    /* Look for special rendering types */
    rendermethod = TextRenderShaded;
    renderstyle = TTF_STYLE_NORMAL;
    rendertype = RENDER_LATIN1;
    outline = 0;
    hinting = TTF_HINTING_NORMAL;
    kerning = 1;
    /* Default is black and white */
    forecol = &black;
    backcol = &white;


            rendermethod = TextRenderSolid;
            rendermethod = TextRenderShaded;
            rendertype = RENDER_UTF8;
            renderstyle |= TTF_STYLE_BOLD;
            hinting = TTF_HINTING_LIGHT;

            hinting = TTF_HINTING_MONO;
            hinting = TTF_HINTING_NONE;

            forecol->r = 255;
            forecol->g = 0;
            forecol->b = 255;
            forecol->a = 255;



SDL_Log(" curr path  %s \n",argv[0]);
SDL_Log(" curr working directory  %s \n",GetWorkingDirectory());


ChangeDirectory("/media/djoker/code/projects/codeeditor/samples/sdl2");
//stringc mediaPath = stringc(GetWorkingDirectory())+"/media/";

    /* Initialize the TTF library */
    if (TTF_Init() < 0) 
{
        fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
        SDL_Quit();
        return(2);
    }

    /* Open the font file with the requested point size */
        ptsize = DEFAULT_PTSIZE;
    font = TTF_OpenFont("media/DroidSans.ttf", ptsize);
    if (font == NULL) 
   {
        fprintf(stderr, "Couldn't load %d pt font from %s: %s\n",
                    ptsize, argv[0], SDL_GetError());
        cleanup(2);
    }
    TTF_SetFontStyle(font, renderstyle);
    TTF_SetFontOutline(font, outline);
    TTF_SetFontKerning(font, kerning);
    TTF_SetFontHinting(font, hinting);

    if(dump) {
        for(i = 48; i < 123; i++) {
            SDL_Surface* glyph = NULL;

            glyph = TTF_RenderGlyph_Shaded(font, i, *forecol, *backcol);

            if(glyph) {
                char outname[64];
                SDL_snprintf(outname, sizeof(outname), "glyph-%d.bmp", i);
                SDL_SaveBMP(glyph, outname);
            }

        }
        cleanup(0);
    }

    /* Create a window */
    if (SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer) < 0) {
        fprintf(stderr, "SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());
        cleanup(2);
    }

    /* Show which font file we're looking at */
    SDL_snprintf(string, sizeof(string), "Font file: %s", argv[0]);  /* possible overflow */
    switch (rendermethod) {
    case TextRenderSolid:
        text = TTF_RenderText_Solid(font, string, *forecol);
        break;
    case TextRenderShaded:
        text = TTF_RenderText_Shaded(font, string, *forecol, *backcol);
        break;
    case TextRenderBlended:
        text = TTF_RenderText_Blended(font, string, *forecol);
        break;
    }
    if (text != NULL) {
        scene.captionRect.x = 4;
        scene.captionRect.y = 4;
        scene.captionRect.w = text->w;
        scene.captionRect.h = text->h;
        scene.caption = SDL_CreateTextureFromSurface(renderer, text);
        SDL_FreeSurface(text);
    }

    /* Render and center the message */
    if (argc > 2) {
        message = argv[2];
    } else {
        message = DEFAULT_TEXT;
    }
    switch (rendertype) {
        case RENDER_LATIN1:
            switch (rendermethod) {
            case TextRenderSolid:
                text = TTF_RenderText_Solid(font, message, *forecol);
                break;
            case TextRenderShaded:
                text = TTF_RenderText_Shaded(font, message, *forecol, *backcol);
                break;
            case TextRenderBlended:
                text = TTF_RenderText_Blended(font, message, *forecol);
                break;
            }
            break;

        case RENDER_UTF8:
            switch (rendermethod) {
            case TextRenderSolid:
                text = TTF_RenderUTF8_Solid(font, message, *forecol);
                break;
            case TextRenderShaded:
                text = TTF_RenderUTF8_Shaded(font, message, *forecol, *backcol);
                break;
            case TextRenderBlended:
                text = TTF_RenderUTF8_Blended(font, message, *forecol);
                break;
            }
            break;

        case RENDER_UNICODE:
        {
            Uint16 *unicode_text = SDL_iconv_utf8_ucs2(message);
            switch (rendermethod) {
            case TextRenderSolid:
                text = TTF_RenderUNICODE_Solid(font, unicode_text, *forecol);
                break;
            case TextRenderShaded:
                text = TTF_RenderUNICODE_Shaded(font, unicode_text, *forecol, *backcol);
                break;
            case TextRenderBlended:
                text = TTF_RenderUNICODE_Blended(font, unicode_text, *forecol);
                break;
            }
            SDL_free(unicode_text);
        }
        break;
    }
    if (text == NULL) {
        fprintf(stderr, "Couldn't render text: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        cleanup(2);
    }
    scene.messageRect.x = (WIDTH - text->w)/2;
    scene.messageRect.y = (HEIGHT - text->h)/2;
    scene.messageRect.w = text->w;
    scene.messageRect.h = text->h;
    scene.message = SDL_CreateTextureFromSurface(renderer, text);
    printf("Font is generally %d big, and string is %d big\n",
                        TTF_FontHeight(font), text->h);

    draw_scene(renderer, &scene);

    /* Wait for a keystroke, and blit text on mouse press */
    done = 0;
    while (!done) {
        if (SDL_WaitEvent(&event) < 0) {
            fprintf(stderr, "SDL_PullEvent() error: %s\n",
                                SDL_GetError());
            done = 1;
            continue;
        }
        switch (event.type) {
            case SDL_MOUSEBUTTONDOWN:
                scene.messageRect.x = event.button.x - text->w/2;
                scene.messageRect.y = event.button.y - text->h/2;
                scene.messageRect.w = text->w;
                scene.messageRect.h = text->h;
                draw_scene(renderer, &scene);
                break;

            case SDL_KEYDOWN:
            case SDL_QUIT:
                done = 1;
                break;
            default:
                break;
        }
    }
    SDL_FreeSurface(text);
    TTF_CloseFont(font);
    SDL_DestroyTexture(scene.caption);
    SDL_DestroyTexture(scene.message);
    cleanup(0);

    /* Not reached, but fixes compiler warnings */
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
