cd src
gcc lump.c lodepng.c main.c cJSON.c -o followermaker -lm
cd ..
move /y src\followermaker.exe .\followermaker.exe
pause