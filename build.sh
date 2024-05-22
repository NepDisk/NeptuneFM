#!/bin/sh
cd src
gcc lump.c lodepng.c main.c cJSON.c -o followermaker -lm
cd ..
mv src/followermaker ./followermaker
