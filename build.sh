#!/bin/sh
cd src
gcc lump.c lodepng.c main.c cJSON.c -o NeptuneFM -lm
cd ..
mv src/NeptuneFM ./NeptuneFM
