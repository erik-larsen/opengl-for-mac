set -o verbose

emcc sdl_gles_minimal.c -s WASM=1 -s USE_SDL=2 -s FULL_ES2=1 -o sdl_gles_minimal.js

# emrun sdl_gles_minimal.html 
# or 
# python3 -m http.server and open http://localhost:8000/sdl_gles_minimal.html
