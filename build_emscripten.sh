set -o verbose

emcc sdl_gles_minimal.c -s USE_SDL=2 -s FULL_ES2=1 -o sdl_gles_minimal.html
emrun sdl_gles_minimal.html
