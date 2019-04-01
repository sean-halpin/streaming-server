mkdir -p ./bin
gcc ./src/server.c -o ./bin/server `pkg-config --cflags --libs gstreamer-1.0`

