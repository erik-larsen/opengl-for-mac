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

typedef struct {
    SDL_Window* p_window;
    bool is_running;
    GLuint shader_program;
    int vp_width, vp_height;
} AppData;

void check_shader_build(const char* shader_name, GLenum status, GLuint shader) 
{
    GLint success;
    glGetShaderiv(shader, status, &success);
    if (success)
        printf("INFO: %s id %d build OK\n", shader_name, shader);
    else
        printf("ERROR: %s id %d build FAILED!\n", shader_name, shader);
}

GLuint init_shader()
{
    const GLchar* vertex_source = " \
        attribute vec4 vPosition; \
        uniform float aspect; \
        varying vec3 color; \
        void main() \
        { \
            gl_Position = vec4(vPosition.xyz, 1.0); \
            gl_Position.y *= aspect; \
            color = gl_Position.xyz + vec3(0.5); \
        } \
    ";

    const GLchar* fragment_source = " \
        precision mediump float; \
        varying vec3 color; \
        void main() \
        { \
            gl_FragColor = vec4 ( color, 1.0 ); \
        } \
    ";

    // Create and compile vertex shader
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_source, NULL);
    glCompileShader(vertex_shader);
    check_shader_build("vertex_shader", GL_COMPILE_STATUS, vertex_shader);

    // Create and compile fragment shader
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_source, NULL);
    glCompileShader(fragment_shader);
    check_shader_build("fragment_shader", GL_COMPILE_STATUS, fragment_shader);

    // Link vertex and fragment shader into shader program and start using it
    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
    glUseProgram(shader_program);

    // Initialize aspect uniform
    glUniform1f(glGetUniformLocation(shader_program, "aspect"), 1.0f);

    return shader_program;
}

void resize_viewport(int width, int height, GLuint shader_program)
{
    printf("INFO: GL viewport resize = %dx%d\n", width, height);

    glViewport(0, 0, width, height);
    float aspect = (float)width / (float)height;
    glUniform1f(glGetUniformLocation(shader_program, "aspect"), aspect);
}

void handle_events(AppData* app_data)
{
    // Poll SDL events, check for quit/escape and window resize
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch(event.type) {

            case SDL_QUIT:
                app_data->is_running = false;
            break;
            
            case SDL_KEYDOWN: 
            {
                // Escape key pressed
                const Uint8* keyStates = SDL_GetKeyboardState(NULL);
                if (keyStates[SDL_SCANCODE_ESCAPE]) 
                    app_data->is_running = false;
                break;
            }

            case SDL_WINDOWEVENT:
            {
                // Resize viewport
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    int width, height;
                    SDL_GL_GetDrawableSize(app_data->p_window, &width, &height);
                    resize_viewport(width, height, app_data->shader_program);
                }
                break;
            }

            default:
                break;
        }
    }
}

void redraw(AppData* app_data)
{ 
    // Clear
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw geometry
    GLfloat vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Update window
    SDL_GL_SwapWindow(app_data->p_window);
}

void main_loop(void* main_loop_arg)
{    
    // Main loop: handle events and redraw
    AppData* app_data = (AppData*)main_loop_arg;
    handle_events(app_data);
    redraw(app_data);
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
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetSwapInterval(1); // 1 = sync framerate to refresh rate (no screen tearing)

    // Explicitly set channel depths, otherwise we might get some < 8
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    // Create SDL GL window
    int vp_width = 512, vp_height = 512;
    SDL_Window* p_window = SDL_CreateWindow("SDL GLES minimal example", 
                                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                           vp_width, vp_height, 
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    // Create SDL GL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(p_window);
    printf("INFO: GL version: %s\n", glGetString(GL_VERSION));

    // Load shader program
    GLuint shader_program = init_shader();

    // Initialize viewport
    // NOTE: Use SDL_GL_GetDrawableSize to handle high dpi scaling, which may e.g. double our requested window size
    SDL_GL_GetDrawableSize(p_window, &vp_width, &vp_height);
    resize_viewport(vp_width, vp_height, shader_program);

    // Set clear color
    glClearColor(0.1F, 0.1F, 0.15F, 1.F);

    // Run app loop, handle events and redraw
    AppData app_data = (AppData) {p_window, true, shader_program, vp_width, vp_height};
    void* main_loop_arg = &app_data; // User-defined data to pass to main_loop()

    #ifdef __EMSCRIPTEN__
        // Run Emscripten main loop     
        int fps = 0; // Set to 0 to use browser's requestAnimationFrame (Emscripten recommended)
        int simulate_infinite_loop = 1;
        emscripten_set_main_loop_arg(main_loop, main_loop_arg, fps, simulate_infinite_loop);

        // Emscripten handles SDL cleanup
    #else
        // Run native main loop
        while (app_data.is_running)
            main_loop(main_loop_arg);

        // Clean up SDL
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(p_window);
        SDL_Quit();
    #endif

    return EXIT_SUCCESS;
}