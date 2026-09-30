/*

    sdl_gles3_minimal.c - A very minimal GLES 3.0 + SDL2 sample that builds to Mac executable and Emscripten
    (WebGL 2) web page. Where sdl_gles_minimal.c draws one triangle with GLES 2, this one exercises what
    GLES 3 adds: a vertex array object, instanced drawing, and an unsigned integer texture read with
    texelFetch in #version 300 es shaders. It draws a grid of instanced squares, each coloured by one
    texel of an R8UI texture.

    Build sample app - Mac native
    -----------------------------
    clang sdl_gles3_minimal.c -o sdl_gles3_minimal $(sdl2-config --cflags --libs) -I$OGL_FOR_MAC/include -L$OGL_FOR_MAC/lib -l GLESv2 -l EGL
    ./sdl_gles3_minimal

    Build sample app - Emscripten
    -----------------------------
    emcc sdl_gles3_minimal.c -s USE_SDL=2 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -o sdl_gles3_minimal.js
    emrun sdl_gles3_minimal.html

*/
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL.h>
#include <GLES3/gl3.h>

#define GRID 16

typedef struct {
    SDL_Window* p_window;
    bool is_running;
    GLuint shader_program;
    GLuint vao;
    int vp_width, vp_height;
} AppData;

void check_shader_build(const char* shader_name, GLenum status, GLuint shader)
{
    GLint success;
    glGetShaderiv(shader, status, &success);
    if (success)
        printf("INFO: %s id %d build OK\n", shader_name, shader);
    else {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        printf("ERROR: %s id %d build FAILED!\n%s\n", shader_name, shader, log);
    }
}

GLuint init_shader()
{
    // One unit square per instance; gl_InstanceID picks its cell in the grid and its texel
    const GLchar* vertex_source =
        "#version 300 es\n"
        "layout(location = 0) in vec2 aCorner;\n"
        "uniform float aspect;\n"
        "flat out int vCell;\n"
        "void main()\n"
        "{\n"
        "    vec2 cell = vec2(gl_InstanceID % 16, gl_InstanceID / 16);\n"
        "    vec2 p = (cell + 0.1 + aCorner * 0.8) / 16.0 * 1.6 - 0.8;\n"
        "    gl_Position = vec4(p.x, p.y * aspect, 0.0, 1.0);\n"
        "    vCell = gl_InstanceID;\n"
        "}\n";

    // The cell's colour index comes from an unsigned integer texture, read exactly with texelFetch
    const GLchar* fragment_source =
        "#version 300 es\n"
        "precision mediump float;\n"
        "precision highp usampler2D;\n"
        "uniform usampler2D uIndex;\n"
        "flat in int vCell;\n"
        "out vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    uint k = texelFetch(uIndex, ivec2(vCell % 16, vCell / 16), 0).r;\n"
        "    vec3 c = 0.5 + 0.5 * cos(6.2832 * (float(k) / 256.0 + vec3(0.0, 0.33, 0.67)));\n"
        "    fragColor = vec4(c, 1.0);\n"
        "}\n";

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

    // Initialize uniforms
    glUniform1f(glGetUniformLocation(shader_program, "aspect"), 1.0f);
    glUniform1i(glGetUniformLocation(shader_program, "uIndex"), 0);

    return shader_program;
}

GLuint init_geometry()
{
    // A vertex array object holding the unit square, drawn once per instance
    static const GLfloat corners[] = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    return vao;
}

void init_texture()
{
    // A GRID x GRID unsigned integer texture: one byte per cell, the colour index
    GLubyte index[GRID * GRID];
    for (int i = 0; i < GRID * GRID; i++)
        index[i] = (GLubyte)((i % GRID) * 7 + (i / GRID) * 11);
    GLuint tex;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);   // integer textures cannot filter
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, GRID, GRID, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, index);
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

    // Draw the grid: one instance per cell
    glBindVertexArray(app_data->vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GRID * GRID);

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

    // Init OpenGLES driver and a GLES 3.0 context
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Explicitly set channel depths, otherwise we might get some < 8
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    // Create SDL GL window
    int vp_width = 512, vp_height = 512;
    SDL_Window* p_window = SDL_CreateWindow("SDL GLES3 minimal example",
                                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           vp_width, vp_height,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    // Create SDL GL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(p_window);
    if (!gl_context) {
        printf("ERROR: GLES 3.0 context creation failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    #ifndef __EMSCRIPTEN__
        SDL_GL_SetSwapInterval(1); // 1 = sync framerate to refresh rate (no screen tearing); needs a current context
    #endif                         // (the browser's requestAnimationFrame already does, below)
    printf("INFO: GL version: %s\n", glGetString(GL_VERSION));
    printf("INFO: GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("INFO: GL renderer: %s\n", glGetString(GL_RENDERER));
    GLint max_tex, max_vs_vec, max_fs_vec;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &max_vs_vec);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &max_fs_vec);
    printf("INFO: max texture size %d, uniform vectors: vertex %d, fragment %d\n", max_tex, max_vs_vec, max_fs_vec);

    // Load shader program, geometry and texture
    GLuint shader_program = init_shader();
    GLuint vao = init_geometry();
    init_texture();

    // Initialize viewport
    // NOTE: Use SDL_GL_GetDrawableSize to handle high dpi scaling, which may e.g. double our requested window size
    SDL_GL_GetDrawableSize(p_window, &vp_width, &vp_height);
    resize_viewport(vp_width, vp_height, shader_program);

    // Set clear color
    glClearColor(0.1F, 0.1F, 0.15F, 1.F);

    // Run app loop, handle events and redraw
    AppData app_data = (AppData) {p_window, true, shader_program, vao, vp_width, vp_height};
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
