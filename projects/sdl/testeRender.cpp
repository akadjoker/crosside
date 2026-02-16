#include <SDL.h>
//#include <SDL_image.h>
#include "unistd.h"
#define cwd getcwd
#define cd chdir
#include <iostream>



bool ChangeDirectory(const char *dir)
{
    bool result = cd(dir);
    if (result != 0) SDL_Log("SYSTEM: Failed to change to directory: %s  %d \n", dir,result);
    return (result == 0);
}
const char *GetWorkingDirectory(void)
{
    static char currentDir[512];
    memset(currentDir, 0, 512);

    char *ptr = cwd(currentDir, 512 - 1);

    return ptr;
}


void setpix(SDL_Renderer *screen, float _x, float _y, unsigned int col)
{

  Uint32 colour;
  Uint8 r,g,b;
  int x = (int)_x;
  int y = (int)_y;
 SDL_RenderDrawPoint(screen,x,y);
}

void drawCircle(SDL_Renderer *screen,float x,float y,float r,unsigned int c)
{
  float tx,ty;
  float xr;
  for(ty = (float)-SDL_fabs(r);ty <= (float)SDL_fabs((int)r);ty++) {
    xr = (float)sqrt(r*r - ty*ty);
    if(r > 0) { //r > 0 ==> filled circle
      for(tx=-xr+.5f;tx<=xr-.5;tx++) {
    setpix(screen,x+tx,y+ty,c);
      }
    }
    else {
      setpix(screen,x-xr+.5f,y+ty,c);
      setpix(screen,x+xr-.5f,y+ty,c);
    }
  }
}

int main(int argc, char* argv[]) 
{

    SDL_Window *window;                    // Declare a pointer

    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL2



    //Set texture filtering to linear
		if( !SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ) )
		{
			SDL_Log( "Warning: Linear texture filtering not enabled!" );
		}

                    SDL_Rect gScreenRect = { 0, 0, 320, 240 };




        //Get device display mode

#if defined(__linux__)

#else
        SDL_DisplayMode displayMode;
        if( SDL_GetCurrentDisplayMode( 0, &displayMode ) == 0 )
        {
            gScreenRect.w = displayMode.w;
            gScreenRect.h = displayMode.h;
        }

#endif

    // Create an application window with the following settings:
    window = SDL_CreateWindow(
        "An SDL2 window",                  // window title
        SDL_WINDOWPOS_UNDEFINED,           // initial x position
        SDL_WINDOWPOS_UNDEFINED,           // initial y position
        gScreenRect.w, gScreenRect.h,                               // height, in pixels
        SDL_WINDOW_SHOWN
    );



    // Check that the window was successfully created
    if (window == NULL) {
        // In the case that the window could not be made...
       // printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }


    // Setup renderer
    SDL_Renderer* renderer = NULL;
    renderer =  SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);


    
SDL_Texture* gTouchDownTexture;
SDL_Texture* gTouchMotionTexture;
SDL_Texture* gTouchUpTexture;
SDL_Texture* gPortraitTexture;
SDL_Texture* gLandscapeTexture;
//Scene textures
SDL_Texture* gPinchCloseTexture;
SDL_Texture* gPinchOpenTexture;
SDL_Texture* gRotateTexture;

SDL_Surface *surface;

SDL_Log(" curr path  %s \n",argv[0]);
SDL_Log(" curr working directory  %s \n",GetWorkingDirectory());

#if defined(__linux__)

//stringc mediaPath = stringc(GetWorkingDirectory())+"/media/";
#else
#endif

ChangeDirectory("/media/djoker/code/projects/codeeditor/samples/sdl2");

surface=SDL_LoadBMP("media/hello.bmp");

gPinchOpenTexture = IMG_LoadTexture(renderer, "media/rotate.jpg");
gPinchCloseTexture = IMG_LoadTexture(renderer, "media/pinch_close.png");


gRotateTexture=SDL_CreateTextureFromSurface(renderer,surface);
SDL_FreeSurface(surface);




    // Display image
    SDL_Rect dstrect;



    // Render to the screen
    SDL_RenderPresent(renderer);

        int i;
    Uint32 then, now, frames;
    Uint64 seed;


       frames = 0;
    then = SDL_GetTicks();


    // Event loop
    bool quit = false;
    SDL_Point touchLocation = { gScreenRect.w / 2, gScreenRect.h / 2 };
    SDL_Event event;
    while(!quit)
    {
       ++frames;
      while( SDL_PollEvent( &event ) != 0 )
      {
        switch (event.type)
        {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_WINDOWEVENT:
                 if( event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED )
                        {
                            gScreenRect.w = event.window.data1;
                            gScreenRect.h = event.window.data2;


                            SDL_RenderPresent( renderer );
                        }
            break;
            case SDL_KEYDOWN:
                if ((event.key.keysym.sym == SDLK_AC_BACK) || (event.key.keysym.sym == SDLK_ESCAPE))
                {
                    quit = true;
                }
                break;

            case SDL_MULTIGESTURE:

             if( fabs( event.mgesture.dTheta ) > 3.14 / 180.0 )
                        {
                            touchLocation.x = event.mgesture.x * gScreenRect.w;
                            touchLocation.y = event.mgesture.y * gScreenRect.h;
                                                         	SDL_Log( "Rotate!" );
//                            currentTextureR = &gRotateTexture;
                        }
						 //Pinch detected
                        else if( fabs( event.mgesture.dDist ) > 0.002 )
                        {
                            touchLocation.x = event.mgesture.x * gScreenRect.w;
                            touchLocation.y = event.mgesture.y * gScreenRect.h;

                            //Pinch open
                            if( event.mgesture.dDist > 0 )
                            {
                             //   currentTextureR = &gPinchOpenTexture;
                             	SDL_Log( "Pinch Open!" );
                            }
                            //Pinch close
                            else
                            {
                                                         	SDL_Log( "Pinch Close!" );
                            //    currentTextureR = &gPinchCloseTexture;
                            }
                        }
            break;
            case SDL_FINGERDOWN:
                                                touchLocation.x = event.tfinger.x * gScreenRect.w;
						touchLocation.y = event.tfinger.y * gScreenRect.h;
            break;
            case SDL_FINGERMOTION:
                                                touchLocation.x = event.tfinger.x * gScreenRect.w;
						touchLocation.y = event.tfinger.y * gScreenRect.h;

            break;
            case SDL_FINGERUP:

                                                touchLocation.x = event.tfinger.x * gScreenRect.w;
						touchLocation.y = event.tfinger.y * gScreenRect.h;
             break;
              default:
                break;
        }
        }
         // Set render color to red ( background will be rendered in this color )
    SDL_SetRenderDrawColor( renderer, 0, 0, 45, 255 );
      SDL_RenderClear( renderer );

      SDL_SetRenderDrawColor( renderer, 255, 255,255, 255 );

       SDL_Rect rect;

   rect.x = touchLocation.x;
   rect.y = touchLocation.y;
   rect.w = 5;
   rect.h = 5;
   SDL_RenderFillRect(renderer, &rect );

//      drawCircle(renderer,touchLocation.x,touchLocation.y,5,0xffff00ff);

if (gRotateTexture)
{
  SDL_Rect renderQuad = { 0, 0, 50,50 };
  SDL_RenderCopy( renderer, gRotateTexture,NULL,&renderQuad);
}
if (gPinchOpenTexture)
{
  SDL_Rect renderQuad = { 100, 10, 50,50 };
 SDL_RenderCopy( renderer, gPinchOpenTexture,NULL,&renderQuad);
}
if (gPinchCloseTexture)
{
  SDL_Rect renderQuad = { 150, 10, 50,50 };
  SDL_RenderCopy( renderer, gPinchCloseTexture,NULL,&renderQuad);
}

      SDL_RenderPresent(renderer);

        now = SDL_GetTicks();
    if (now > then) {
        double fps = ((double) frames * 1000) / (now - then);
    //    SDL_Log("%2.2f frames per second\n", fps);
    }
    }



    // Close and destroy the window
    SDL_DestroyWindow(window);

    // Clean up
    SDL_Quit();

    return 0;
}
