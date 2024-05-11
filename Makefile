CC = gcc
SRC = src/main.c src/lump.c src/lodepng.c src/cJSON.c
CCFLAGS = -O2 -lm
OUTPUT = followermaker.exe

all: $(OUTPUT)

$(OUTPUT):
	$(CC) $(SRC) $(CCFLAGS) -o $(OUTPUT)

clean:
	rm *.o