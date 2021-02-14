cd src
gcc lump.c lodepng.c main.c cJSON.c -o kartmaker -lm
cd ..
move /y src\kartmaker.exe .\kartmaker.exe
pause