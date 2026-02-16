
#include <SDL.h>
int main()
{
    SDL_Window *window=NULL;
    SDL_Renderer *render=NULL;
    SDL_Texture *texture=NULL;
    SDL_Event event;
    SDL_Rect rect;
    int quit=0;
    //Initialize the environment
    SDL_Init(0);
         //Create a window
    window = SDL_CreateWindow("test",200,200,900,600,SDL_WINDOW_SHOWN);
    if(!window)
    {
 
    }
         //Create a renderer
    render = SDL_CreateRenderer(window,-1,0);
    if(!render)
    {
    }

    texture = SDL_CreateTexture(render,
                                SDL_PIXELFORMAT_ABGR8888,
                                SDL_TEXTUREACCESS_TARGET,
                                640,
                                480);
 
   do
   {
                 SDL_PollEvent(&event);//Training method
        switch(event.type)
        {
        case SDL_QUIT:
            quit=1;
            break;
        default:
            break;
        }
 
        rect.w = 40;
        rect.h = 30;
        rect.x= rand()%640;
        rect.y = rand()%480;
 
        SDL_SetRenderTarget(render,texture);
        SDL_SetRenderDrawColor(render,0,0,0,0);
                 //Set the background color to fill in the background color in the drawing
        SDL_RenderClear(render);
 
        SDL_RenderDrawRect(render,&rect);
        SDL_SetRenderDrawColor(render,255,0,0,0);
        SDL_RenderFillRect(render,&rect);
 
                 //Set the rendering target
        SDL_SetRenderTarget(render,NULL);
                 //Texture copy to graphics card rendering
        SDL_RenderCopy(render,texture,NULL,NULL);
                 //Start rendering the image and copy to the monitor
        SDL_RenderPresent(render);
   }while(!quit);
 
       //Delete texture
    SDL_DestroyTexture(texture);
         //Delete renderer
    SDL_DestroyRenderer(render);
         //Delete the window
    SDL_DestroyWindow(window);
         //Uninstall the environment
    SDL_Quit();
    return 0;
}
