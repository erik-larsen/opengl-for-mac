set -o verbose

# workaround for resetting of DYLD_FALLBACK_LIBRARY_PATH in child processes
GLES_FOR_MAC=`pwd`
export DYLD_FALLBACK_LIBRARY_PATH=$GLES_FOR_MAC/lib

# GLES 3 sample: ./build_mac.sh gles3
if [ "$1" = "gles3" ]; then
    clang sdl_gles3_minimal.c -o sdl_gles3_minimal $(sdl2-config --cflags --libs) -I$GLES_FOR_MAC/include -L$GLES_FOR_MAC/lib -l GLESv2 -l EGL && ./sdl_gles3_minimal
    exit
fi

clang sdl_gles_minimal.c -o sdl_gles_minimal $(sdl2-config --cflags --libs) -I$GLES_FOR_MAC/include -L$GLES_FOR_MAC/lib -l GLESv2 -l EGL && ./sdl_gles_minimal
