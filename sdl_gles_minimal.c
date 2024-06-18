/*

    sdl_gles_minimal.c - A very minimal GLES + SDL2 hello triangle sample that builds to Mac executable and Emscripten web page.

    Build sample app - Mac native
    -----------------------------
    clang sdl_gles_minimal.c -o sdl_gles_minimal $(sdl2-config --cflags --libs) -I$GLES_FOR_MAC/include -L$GLES_FOR_MAC/lib -l GLESv2 -l EGL
    ./sdl_gles_minimal

    Build sample app - Emscripten
    -----------------------------
    emcc sdl_gles_minimal.c -s USE_SDL=2 -s FULL_ES2=1 -o sdl_gles_minimal.html
    emrun sdl_gles_minimal.html

*/
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL.h>
#include <SDL_opengles2.h>

GLuint initShader()
{
    const GLchar* vertexSource = " \
        attribute vec4 vPosition; \
        varying vec3 color; \
        void main() \
        { \
            gl_Position = vec4(vPosition.xyz, 1.0); \
            color = gl_Position.xyz + vec3(0.5); \
        } \
    ";

    const GLchar* fragmentSource = " \
        precision mediump float; \
        varying vec3 color; \
        void main() \
        { \
            gl_FragColor = vec4 ( color, 1.0 ); \
        } \
    ";

    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    // Link vertex and fragment shader into shader program and use it
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);

    return shaderProgram;
}

bool handle_events()
{
    // Poll SDL events, check for quit/escape
    bool is_running = true;

    SDL_Event event;
    while (0 != SDL_PollEvent(&event)) {
        if (SDL_QUIT == event.type)
            is_running = false;
        
        else if (SDL_KEYDOWN == event.type) {
            const Uint8* keyStates = SDL_GetKeyboardState(NULL);
            if (keyStates[SDL_SCANCODE_ESCAPE]) 
                is_running = false;
        }
    }

    return is_running;
}

void redraw(SDL_Window *pWindow)
{
    // Clear
    glClear(GL_COLOR_BUFFER_BIT);

    // Render scene
    GLfloat vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Update window
    SDL_GL_SwapWindow(pWindow);
}

typedef struct {
    SDL_Window* pWindow;
    bool is_running;
} LoopData;

void main_loop(void* main_loop_arg)
{
    LoopData* loop_data = (LoopData*)main_loop_arg;
    loop_data->is_running = handle_events();
    redraw(loop_data->pWindow);
}

int main(int argc, char** argv) 
{
    // Init SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_version version;
    SDL_GetVersion(&version);
    printf("INFO: SDL version: %d.%d.%d\n", version.major, version.minor, version.patch);

    // Init OpenGLES driver and context
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    // Explicitly set channel depths, otherwise we might get some < 8
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetSwapInterval(1); // 1 = sync framerate to refresh rate

    // Create window
    int winWidth = 512, winHeight = 512;
    SDL_Window* pWindow = SDL_CreateWindow("SDL GLES minimal example", 
                                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                           winWidth, winHeight, 
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);

    // Init GL context
    SDL_GLContext glContext = SDL_GL_CreateContext(pWindow);
    printf("INFO: GL version: %s\n", glGetString(GL_VERSION));

    // Get actual GL window size in pixels, in case of high dpi scaling
    SDL_GL_GetDrawableSize(pWindow, &winWidth, &winHeight);
    printf("INFO: GL window size = %dx%d\n", winWidth, winHeight);
    glClearColor(0.1F, 0.1F, 0.15F, 1.F);
    glViewport(0, 0, winWidth, winHeight);   

    // Load shader program
    GLuint program = initShader();
    glUseProgram(program);

    // Run event/redraw loop
    LoopData loop_data = {pWindow, true};
    void* main_loop_arg = &loop_data; // User-defined data to pass to main_loop()

    #ifdef __EMSCRIPTEN__
        int fps = 0; // Set to 0 to use browser's requestAnimationFrame (Emscripten recommended)
        int simulate_infinite_loop = 0;
        emscripten_set_main_loop_arg(main_loop, main_loop_arg, fps, simulate_infinite_loop);
    #else
        while (loop_data.is_running)
            main_loop(main_loop_arg);

        // Clean up
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(pWindow);
        SDL_Quit();
    #endif

    return EXIT_SUCCESS;
}
