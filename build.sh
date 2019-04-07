mkdir -p ./bin
gcc -g ./src/server.c -o ./bin/server `pkg-config --cflags --libs gstreamer-1.0`

