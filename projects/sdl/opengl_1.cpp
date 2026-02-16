#include <stdio.h>
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengles2.h>
#include <string.h>
#include <math.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

int SCREEN_WIDTH = 800;
int SCREEN_HEIGHT = 450;



SDL_Window* gWindow = NULL;
SDL_GLContext gContext =NULL;
GLuint vao;
GLuint vbo;

bool running = true;


void glCheckError(const char* file, unsigned int line)
{
    // Get the last error
    GLenum errorCode = glGetError();

    if (errorCode != GL_NO_ERROR)
    {

      printf("*** Error: %s line %i \n",file,line);

        // Decode the error code
        switch (errorCode)
        {
            case GL_INVALID_ENUM:
            {

                printf("an unacceptable value has been specified for an enumerated argument\n");
                break;
            }

            case GL_INVALID_VALUE:
            {

               printf("a numeric argument is out of range\n");
                break;
            }

            case GL_INVALID_OPERATION:
            {

                printf("the specified operation is not allowed in the current state\n");
                break;
            }

            case 0x0503://GL_STACK_OVERFLOW:
            {

                printf("this command would cause a stack overflow\n");
                break;
            }

            case 0x0504://GL_STACK_UNDERFLOW:
            {

                printf("this command would cause a stack underflow\n");
                break;
            }

            case GL_OUT_OF_MEMORY:
            {

                printf("there is not enough memory left to execute the command\n");
                break;
            }


        }


    }
}



bool init()
{
    //Initialization flag
    bool success = true;

    //Initialize SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        SDL_Log( "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
        success = false;
    }
    else
    {

        //Get device display mode
           SDL_DisplayMode displayMode;
        if( SDL_GetCurrentDisplayMode( 0, &displayMode ) == 0 )
        {
         

    #ifdef ANDROID
    SCREEN_WIDTH = displayMode.w;
    SCREEN_HEIGHT = displayMode.h;
    #else
    SCREEN_WIDTH = (int)(displayMode.h - 200) / 1.5;
    SCREEN_HEIGHT = displayMode.h - 200;
    #endif
        }










   Uint32 flags =   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
  flags =   SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN |SDL_WINDOW_OPENGL;


    // gWindow = SDL_CreateWindow(     TITLE,     SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,     SCREEN_WIDTH,SCREEN_HEIGHT,  flags );

gWindow= SDL_CreateWindow("Opengl", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,   SCREEN_WIDTH,SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);



    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetSwapInterval(0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);



     gContext = SDL_GL_CreateContext( gWindow );

        if( gWindow == NULL )
        {
            SDL_Log( "Window could not be created! SDL Error: %s\n", SDL_GetError() );
            success = false;
        }
    }

        SDL_DisplayMode mode;
      SDL_GetDesktopDisplayMode(0, &mode);

   SDL_Log("Screen bpp: %d", SDL_BITSPERPIXEL(mode.format));
    SDL_Log("");
    SDL_Log("Vendor     : %s", glGetString(GL_VENDOR));
    SDL_Log("Renderer   : %s", glGetString(GL_RENDERER));
    SDL_Log("Version    : %s", glGetString(GL_VERSION));
    SDL_Log("Extensions : %s", glGetString(GL_EXTENSIONS));
    SDL_Log("");

    return success;
}

bool loadMedia()
{
    bool success = true;
    
    
  


// Shader sources
const GLchar* vertexSource =
    "attribute vec4 position;    \n"
    "void main()                  \n"
    "{                            \n"
    "   gl_Position = vec4(position.xyz, 1.0);  \n"
    "}                            \n";
const GLchar* fragmentSource =

    "void main()                                  \n"
    "{                                            \n"
    "  gl_FragColor = vec4 (1.0, 1.0, 1.0, 1.0 );\n"
    "}                                            \n";






    glGenVertexArraysOES(1, &vao);
    glCheckError("main",329);
    glBindVertexArrayOES(vao);
    glCheckError("main",330);

    // Create a Vertex Buffer Object and copy the vertex data to it

    glGenBuffers(1, &vbo);
    glCheckError("main",336);

    GLfloat vertices[] = {0.0f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f};

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glCheckError("main",342);

    // Create and compile the vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    glCheckError("main",348);

    // Create and compile the fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    glCheckError("main",354);

    // Link the vertex and fragment shader into a shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glCheckError("main",360);
    // glBindFragDataLocation(shaderProgram, 0, "outColor");
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
    glCheckError("main",364);
    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glCheckError("main",369);

 
    return success;
}

void close()
{

    SDL_GL_DeleteContext(gContext);
    SDL_DestroyWindow( gWindow );
    gWindow = NULL;
    SDL_Quit();
}

  const SDL_MessageBoxButtonData buttons[] = {
        { /* .flags, .buttonid, .text */        0, 0, "No" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },

    };
    const SDL_MessageBoxColorScheme colorScheme = {
        { /* .colors (.r, .g, .b) */
            /* [SDL_MESSAGEBOX_COLOR_BACKGROUND] */
            { 45,   45,   45 },
            /* [SDL_MESSAGEBOX_COLOR_TEXT] */
            {   255, 255,   255 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BORDER] */
            { 255, 255,   255 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND] */
            {   70,   70, 75 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED] */
            { 5,   5, 5 }
        }
    };
    const SDL_MessageBoxData messageboxdata = {
        SDL_MESSAGEBOX_INFORMATION, /* .flags */
        NULL, /* .window */
        "Exit Aplication", /* .title */
        "Press a button", /* .message */
        SDL_arraysize(buttons), /* .numbuttons */
        buttons, /* .buttons */
        &colorScheme /* .colorScheme */
    };

void handle_events()
{
	         SDL_Event e;
	        while( SDL_PollEvent( &e ) != 0 )
                {



                    switch(e.type)
                    {
                     case SDL_QUIT:
                       {
                     //   emscripten_cancel_main_loop();
                        running = false;
				       }
                        break;
                        case SDL_MOUSEBUTTONDOWN:
                        {
                       //  call_mouse_down(e.button.button,e.button.x,e.button.y);

                     //  SDL_Log("down");
             
                        }
                        break;
                        case SDL_MOUSEBUTTONUP:
                        {
                        // call_mouse_up(e.button.button,e.button.x,e.button.y);
                            // SDL_Log("up");
            
                        }
                        break;
                        case SDL_MOUSEMOTION:
                        {
                         //call_mouse_move(e.button.x,e.button.y);
                        //   SDL_Log("move");
              
                        }
                        break;

                   
                }
		}

}

void game_loop()
{

        handle_events();

        glClearColor(0, 0, 0.4f, 0);
        glClearDepthf(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

           glDrawArrays(GL_TRIANGLES, 0, 3);

          SDL_GL_SwapWindow( gWindow );
          
          
         
}





int main(int argc, char* args[])
{

   if( !init() )
    {
        SDL_Log( "Failed to initialize!\n" );
    }
    else
    {

        //Load media
        if( !loadMedia() )
        {
            SDL_Log( "Failed to load media!\n" );
        }
        else
        {


#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(game_loop, 0, 1);
#else
                while (running)
        	{
                 game_loop();
                }
#endif

       //  emscripten_set_main_loop(game_loop, 0, 1);
        }
    }

    close();
    return 0;
}


