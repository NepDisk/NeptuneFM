cd src
gcc lump.c lodepng.c main.c cJSON.c -o NeptuneFM -lm
cd ..
move /y src\NeptuneFM.exe .\NeptuneFM.exe
pause
