
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>




#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480




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



 void
DrawRect(SDL_Renderer *r, const int x, const int y, const int w, const int h)
{
    const SDL_Rect area = { x, y, w, h };
    SDL_RenderFillRect(r, &area);
}


    SDL_Point points[10];
	int numpoint=0;

	SDL_Point touch[10];
	int numtouch=0;

	
int main(int argc, char *argv[])
{
     SDL_Window *window = NULL;
    SDL_Renderer *screen = NULL;
    const char *name = NULL;
    SDL_bool retval = SDL_FALSE;
    SDL_bool done = SDL_FALSE;
    SDL_Event event;
    int i;

    /* Create a window to display joystick axis position */
    window = SDL_CreateWindow("android", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                              SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "Couldn't create window: %s\n", SDL_GetError());
        return SDL_FALSE;
    }

    screen = SDL_CreateRenderer(window, -1, 0);
    if (screen == NULL) {
        fprintf(stderr, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return SDL_FALSE;
    }


    SDL_RenderPresent(screen);
    SDL_RaiseWindow(window);

    

    /* Loop, getting joystick events! */
    while (!done) 
	{
      
        SDL_SetRenderDrawColor(screen, 0x00, 0x00, 45, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(screen);
	

      int count=0;


	while(SDL_PollEvent(&event)) 
	{
		
		
	 switch (event.type) 
	  {
	  case SDL_QUIT:
	    done = 1;
	    break;
	  case SDL_KEYDOWN:
		  SDL_Log("key %i\n",event.key.keysym.scancode);
	    break;
	  case SDL_KEYUP:
		   if(event.key.keysym.scancode==270)
				 {
                   done = 1;
				 }
		  break;
	
    case SDL_FINGERDOWN:
		{
			 float  x = event.tfinger.x;
             float y = event.tfinger.y;
			 drawCircle(screen,x*SCREEN_WIDTH,y*SCREEN_HEIGHT,10,0);
		}
		break;
	case SDL_FINGERUP:
		{
			
		}
		break;
	case SDL_FINGERMOTION:
	{
				  
			 float  x = event.tfinger.x;
             float y = event.tfinger.y;
     	    drawCircle(screen,x*SCREEN_WIDTH,y*SCREEN_HEIGHT,20,0);
	}
	 break;
			  
	  case SDL_MOUSEMOTION:
	  {
		SDL_SetRenderDrawColor(screen, 0,255,0, SDL_ALPHA_OPAQUE);
     	DrawRect(screen,  event.motion.x,event.motion.y, 32, 32);
	
	  }
	  break;
	  case SDL_MOUSEBUTTONDOWN:
	  {
		  
		  SDL_SetRenderDrawColor(screen, 0,255,0, SDL_ALPHA_OPAQUE);
                	DrawRect(screen,  event.button.x,event.button.y, 32, 32);
	  }
	  break;
	  case SDL_MOUSEBUTTONUP:
	  {}
	  break;
	 
	  }
	  

	}
	SDL_SetRenderDrawColor(screen, 255,255,0, SDL_ALPHA_OPAQUE);
	DrawRect(screen,  20,20, 32, 32);
	
     
        SDL_RenderPresent(screen);

    }

    SDL_DestroyRenderer(screen);
    SDL_DestroyWindow(window);

	
   

#ifdef ANDROID
    exit(0);
#else
    return 0;
#endif
}
