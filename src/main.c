/*
	Kartmaker. By fickleheart 2019.
	Takes a working folder (examples provided) and converts it into a character WAD for SRB2Kart.
	Uses lump.c and lump.h from Lumpmod. &copy; 2003 Thunder Palace Entertainment.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	main.c: Provides program functionality.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "cJSON.h"
#include "lodepng.h"
#include "lump.h"

// If your sprites are bigger than 256*256, consider a different approach than this program?
#define MAX_IMAGE_SIZE 256*256

#define S_SKIN_TEMPLATE "name = %s\nrealname = %s\nfacerank = %sRANK\nfacewant = %sWANT\nfacemmap = %sMMAP\nkartspeed = %d\nkartweight = %d\nstartcolor = %d\nprefcolor = %s\nDSKGLOAT = DS%sGL\nDSKWIN = DS%sWI\nDSKLOSE = DS%sLS\nDSKSLOW = DS%sSL\nDSKHURT1 = DS%sH1\nDSKHURT2 = DS%sH2\nDSKATTK1 = DS%sA1\nDSKATTK2 = DS%sA2\nDSKBOST1 = DS%sB1\nDSKBOST2 = DS%sB2\nDSKHITEM = DS%sHT\n"

#define SKINNAMESIZE 16

// This struct contains pre-lump-conversion data about a sprite
struct RGB_Sprite {
	char lumpname[9];

	int width, height, xoffs, yoffs;

	int numLayers;
	struct {int x, y;} layers[5]; // Max 5 layers. x and y are the top-left corner of each layer
	int flip;
	int heightFactor;
	int ditherStyle;

	struct RGB_Sprite* next;
};

struct skinprop {
	char name[SKINNAMESIZE];
	char realname[SKINNAMESIZE];
	uint8_t kartspeed;
	uint8_t kartweight;
	uint8_t startcolor;
	char prefcolor[32];
};

unsigned error;
unsigned char* sprite_sheet;
unsigned sprites_width, sprites_height;

unsigned char transColors[12]; // Allow four "transparent" colors, or alpha < 0.5 for transparent.
int numTransColors;

cJSON* metadata;

struct RGB_Sprite* rgb_sprites;
struct RGB_Sprite* lastsprite;
struct RGB_Sprite* gfxstart;

struct skinprop kskin;

char defprefix[4] = "SOME";

unsigned char* pix;
#define READPIXEL(x, y) (pix = sprite_sheet + ((x) + (y)*sprites_width) * 4)
#define PIX_R pix[0]
#define PIX_G pix[1]
#define PIX_B pix[2]
#define PIX_A pix[3]
/*
// Add the boilerplate Lua script file to the WAD
void addLuaScript(struct wadfile* wad, struct Filemap* files, int exportLua)
{
	// Huge buffer just to avoid having to reallocate
	unsigned char script[1<<20];
	unsigned char* scriptptr = script;

	printf("\nGenerating boilerplate Lua...\n\n");

#define WRITESTR(str) {strcpy(scriptptr, str); scriptptr += strlen(str);}
	WRITESTR("-- Auto-generated filename-lumpname mapping tables. Use as rootfoldername[\"path/to/file/without/extension\"] to get the lump name.");

	while (files)
	{
		struct Fileref* fileref = files->headfile;

		WRITESTR("\nrawset(_G, \"");
		WRITESTR(files->rootname);
		WRITESTR("\", {\n");

		while (fileref)
		{
			WRITESTR("\t[\"");
			WRITESTR(fileref->refname);
			WRITESTR("\"] = \"");
			WRITESTR(fileref->lumpname);
			WRITESTR("\",\n");

			fileref = fileref->next;
		}
		WRITESTR("})\n");

		files = files->next;
	}

#undef WRITESTR

	printf("%s", script);

	// Export as text
	if (exportLua != 0)
	{
		FILE* file;
		printf("Writing Lua file...");

		file = fopen("map.lua", "w");
		fwrite(script, 1, strlen(script), file);
		fclose(file);

		printf(" successful.\n");
	}

	// Add lump to WAD
	if (exportLua != 2)
	{
		printf("Writing lump to WAD...");
		add_lump(wad, NULL, "LUA_MAPS", strlen(script), script);
		printf(" successful.\n");
	}
}



// Load images into WAD file
void loadImages(struct wadfile* wad, struct Filemap* files)
{
	printf("Writing images to WAD...\n");
	while (files)
	{
		struct Fileref* fileref;

		for (fileref = files->headfile; fileref; fileref = fileref->next)
		{
			// Handle each file
			size_t size;
			unsigned char* data;

			printf("Converting image %s...\n", fileref->filename);
			data = imageInDoomFormat(fileref->filename, fileref->xoffs, fileref->yoffs, &size);

			printf("Adding file of %d bytes...\n", size);
			add_lump(wad, wad->head, fileref->lumpname, size, data);
			free(data);
			printf("Done.\n");
		}

		files = files->next;
	}
}*/

cJSON* loadJSON(char* filename) {
	// Now get JSON
	unsigned char* buffer;
	off_t size, bytesRead;
	FILE* file = fopen(filename, "rb");

	// File couldn't be opened!
	if (file == NULL)
	{
		return NULL;
	}

	// seek to end of file
	fseek(file, 0, SEEK_END);

	// Load file into buffer
	size = ftell(file);
	buffer = malloc(size);

	// seek back to start
	fseek(file, 0, SEEK_SET);

	//read contents!
	bytesRead = fread(buffer, 1, size, file);
	fclose(file);

	return cJSON_Parse(buffer);
}

void readTransparentColors(void) {
	cJSON* item;

	// Read transparent colors
	printf("Read transparent colors... ");
	numTransColors = 0;
	item = cJSON_GetObjectItem(metadata, "transparent_colors")->child;
	while (item != NULL) {
		transColors[numTransColors] = (unsigned char) item->valueint;
		numTransColors++;
		item = item->next;
		printf("%d ", numTransColors);
	}
	printf("Done.\n");
}

void SetDefaultSkinValues(void)
{
	strncpy(kskin.name, "someone", 8);
	kskin.name[8] = '\0';
	strncpy(kskin.realname, "Someone", 8);
	kskin.realname[8] = '\0';
	kskin.kartspeed = 5;
	kskin.kartweight = 5;
	kskin.startcolor = 96;
	strncpy(kskin.prefcolor, "Green", 6);
	kskin.prefcolor[6] = '\0';
}


void processSprites(void) {
	cJSON *item, *nesteditem, *prop;
	int spr_width, spr_height, step_width, step_height, stepw, steph;
	struct RGB_Sprite* cursprite;
	char prefix[5] = "____";

	// Read sprite size
	printf("Read sprite size... ");
	item = cJSON_GetObjectItem(metadata, "sprite_size")->child;
	spr_width = item->valueint;
	spr_height = item->next->valueint;
	printf("width=%d height=%d Done.\n", spr_width, spr_height);

	// Read step size
	printf("Read step size... ");
	item = cJSON_GetObjectItem(metadata, "layer_step_size")->child;
	step_width = item->valueint;
	step_height = item->next->valueint;
	printf("stepwidth=%d stepheight=%d Done.\n", step_width, step_height);

	// Begin reading sprites
	printf("Reading sprites...\n");
	gfxstart = lastsprite = rgb_sprites = NULL;

	item = cJSON_GetObjectItem(metadata, "sprites")->child;

	while (item != NULL) {
		printf("Reading SPR2_%s... \n", item->string);
		strncpy(prefix, item->string, 4);
		nesteditem = item->child;

		while (nesteditem != NULL) {
			printf(" frame %s... ", nesteditem->string);

			cursprite = calloc(1, sizeof(struct RGB_Sprite));
			if (lastsprite != NULL)
				lastsprite->next = cursprite;
			if (rgb_sprites == NULL)
				rgb_sprites = cursprite;
			lastsprite = cursprite;

			prop = cJSON_GetObjectItem(nesteditem, "overwrite_sprite_size");
			if (prop)
			{
				cursprite->width = prop->child->valueint;
				cursprite->height = prop->child->next->valueint;
			}
			else
			{
				cursprite->width = spr_width;
				cursprite->height = spr_height;
			}

			prop = cJSON_GetObjectItem(nesteditem, "overwrite_layer_step_size");
			if (prop)
			{
				stepw = prop->child->valueint;
				steph = prop->child->next->valueint;
			}
			else
			{
				stepw = step_width;
				steph = step_height;
			}

			sprintf(cursprite->lumpname, "%s%s", prefix, nesteditem->string);

			prop = cJSON_GetObjectItem(nesteditem, "heightfactor");
			cursprite->heightFactor = prop != NULL ? prop->valueint : 1;

			prop = cJSON_GetObjectItem(nesteditem, "ditherstyle");
			cursprite->ditherStyle = prop != NULL ? prop->valueint : 0;

			prop = cJSON_GetObjectItem(nesteditem, "flip");
			cursprite->flip = (prop != NULL && prop->type == cJSON_True) ? -1 : 1;

			prop = cJSON_GetObjectItem(nesteditem, "offset");
			if (prop)
			{
				cursprite->xoffs = prop->child->valueint;
				cursprite->yoffs = prop->child->next->valueint;

				if (cursprite->flip == -1 && (nesteditem->string[1] == '1' || nesteditem->string[1] == '5' || nesteditem->string[1] == '0'))
					cursprite->xoffs -= 1; // MATCH FRONT/BACK VIEWS
			} // calloc means 0 otherwise

			cursprite->numLayers = 0;
			prop = cJSON_GetObjectItem(nesteditem, "layers")->child;
			while (prop != NULL) {
				cursprite->layers[cursprite->numLayers].x = prop->child->valueint*stepw;
				cursprite->layers[cursprite->numLayers].y = prop->child->next->valueint*steph;

				if (cursprite->flip == -1)
					cursprite->layers[cursprite->numLayers].x += (cursprite->width-1); // more human-readable template info this way

				prop = prop->next;
				cursprite->numLayers++;

				if (cursprite->numLayers == 5) break;
			}

			printf("layers=%d Done.\n", cursprite->numLayers);
			nesteditem = nesteditem->next;
		}
		item = item->next;
	}

	printf("Reading sprites... Done.\n");
}

void processGfx(void)
{
	cJSON* item;
	cJSON* prop;
	char prefix[5] = "____";

	printf("Reading graphic prefix... ");
	item = cJSON_GetObjectItem(metadata, "prefix");
	if (item)
		strncpy(prefix, item->valuestring, 4);
	else
		strncpy(prefix, defprefix, 4);

	printf("Done.\n");

	if ((item = cJSON_GetObjectItem(metadata, "gfx")) == NULL)
	{
		printf("No gfx found, skipping...\n");
		return;
	}
	item = item->child;
	while (item != NULL) {
		printf("Reading graphic %s%s... ", prefix, item->string);
		lastsprite->next = calloc(1, sizeof(struct RGB_Sprite));
		lastsprite = lastsprite->next;

		if (gfxstart == NULL)
			gfxstart = lastsprite;

		sprintf(lastsprite->lumpname, "%s%s", prefix, item->string);
		lastsprite->numLayers = 1;
		lastsprite->layers[0].x = item->child->valueint;
		lastsprite->layers[0].y = item->child->next->valueint;
		lastsprite->width = item->child->next->next->valueint;
		lastsprite->height = item->child->next->next->next->valueint;
		lastsprite->xoffs = item->child->next->next->next->next->valueint;
		lastsprite->yoffs = item->child->next->next->next->next->next->valueint;
		lastsprite->heightFactor = 1;
		lastsprite->ditherStyle = 0;
		lastsprite->flip = 1;

		item = item->next;
		printf("Done.\n");
	}
}

// Convert an RGBA pixel to palette index and transparency
static unsigned char palette[768];
int palInit = 0;
void rgbaToPalette(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha, unsigned char* paletteIndex, unsigned char* opaque)
{
	int closenessOfCurrent = 0xFFFFFF;
	int palCheck;

	if (alpha < 0x80)
	{
		// Transparent pixel
		*opaque = 0;
		return;
	}

	// Check for defined-transparent pixels
	for (palCheck = 0; palCheck < numTransColors; palCheck += 3)
	{
		if (red == transColors[palCheck] && green == transColors[palCheck+1] && blue == transColors[palCheck+2])
		{
			// Transparent pixel
			*opaque = 0;
			return;
		}
	}

	// Opaque pixel!
	*opaque = 1;

	// Map colors to palette index
	for (palCheck = 0; palCheck < 256; palCheck++)
	{
		int closeness;

		unsigned char palRed, palGreen, palBlue;
		palRed = palette[palCheck*3];
		palGreen = palette[palCheck*3+1];
		palBlue = palette[palCheck*3+2];

		closeness = (red-palRed)*(red-palRed) + (green-palGreen)*(green-palGreen) + (blue-palBlue)*(blue-palBlue);

		// If we're the closest so far, use this palette index!
		if (closeness < closenessOfCurrent) {
			closenessOfCurrent = closeness;
			*paletteIndex = palCheck;
		}
	}
}

// Load an image in Doom patch format
static unsigned char convertedimage[1<<26];
unsigned char* imageInDoomFormat(struct RGB_Sprite* image, size_t* size)
{
	//@TODO handle squishing
	unsigned x, y;

	unsigned char* img;
	unsigned char* imgptr = convertedimage;
	unsigned char *colpointers, *startofspan;

#define WRITE8(buf, a) ({*buf = (a); buf++;})
#define WRITE16(buf, a) ({*buf = (a)&255; buf++; *buf = (a)>>8; buf++;})
#define WRITE32(buf, a) ({WRITE16(buf, (a)&65535); WRITE16(buf, (a)>>16);})
	// Write image size and offset
	WRITE16(imgptr, image->width);
	WRITE16(imgptr, image->height/image->heightFactor);
	WRITE16(imgptr, image->xoffs);
	WRITE16(imgptr, image->yoffs);

	// Leave placeholder to column pointers
	colpointers = imgptr;
	imgptr += image->width*4;

	// Write columns
	for (x = 0; x < image->width; x++)
	{
		int lastStartY = 0;
		int spanSize = 0;
		startofspan = NULL;

		//printf("%d ", x);
		// Write column pointer (@TODO may be wrong)
		WRITE32(colpointers, imgptr - convertedimage);

		// Write pixels
		for (y = 0; y < image->height/image->heightFactor; y ++)
		{
			unsigned char paletteIndex = 0;
			unsigned char opaque = 0; // If 1, we have a pixel
			int layer;

			switch (image->ditherStyle)
			{
				case 1:
					// Stationary vibration
					if ((x+y) & 1)
					{
						for (layer = 0; layer < image->numLayers && !opaque; layer++)
						{
							int offset = y*image->heightFactor + 1;

							if (offset >= 0 && offset < image->height)
							{
								READPIXEL(image->layers[layer].x + x*image->flip, image->layers[layer].y + offset);
								rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
							}
						}
					}

					for (layer = 0; layer < image->numLayers && !opaque; layer++)
					{
						READPIXEL(image->layers[layer].x + x*image->flip, image->layers[layer].y + y*image->heightFactor);
						rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
					}
					break;
				case 2:
					// Slow driving vibration
					if ((x+y) & 1)
					{
						// Dither pattern 1!
						for (layer = 0; layer < image->numLayers && !opaque; layer++)
						{
							int offset = y*image->heightFactor + 3;

							if (offset >= 0 && offset < image->height)
							{
								READPIXEL(image->layers[layer].x + x*image->flip, image->layers[layer].y + offset);
								rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
							}
						}
					}
					else
					{
						// Dither pattern 2!
						for (layer = 0; layer < image->numLayers && !opaque; layer++)
						{
							READPIXEL(image->layers[layer].x + x*image->flip, image->layers[layer].y + y*image->heightFactor);
							rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
						}
					}
					break;
				case 3:
					// Drifting vibration A
					if (y & 1)
					{
						int offset = (x + 3);

						if (offset >= 0 && offset < image->width)
						{
							for (layer = 0; layer < image->numLayers && !opaque; layer++)
							{
								READPIXEL(image->layers[layer].x + (offset * image->flip), image->layers[layer].y + y*image->heightFactor);
								rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
							}
						}
					}

					for (layer = 0; layer < image->numLayers && !opaque; layer++)
					{
						READPIXEL(image->layers[layer].x + x*image->flip, image->layers[layer].y + y*image->heightFactor);
						rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
					}
					break;
				case 4:
					// Drifting vibration B
					if (y & 1)
					{
						int offset = (x - 3);

						if (offset >= 0 && offset < image->width)
						{
							for (layer = 0; layer < image->numLayers && !opaque; layer++)
							{
								READPIXEL(image->layers[layer].x + (offset * image->flip), image->layers[layer].y + y*image->heightFactor);
								rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
							}
						}
					}

					for (layer = 0; layer < image->numLayers && !opaque; layer++)
					{
						READPIXEL(image->layers[layer].x + x*image->flip, image->layers[layer].y + y*image->heightFactor);
						rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
					}
					break;
				default:
					// No dither, just read pixels
					for (layer = 0; layer < image->numLayers && !opaque; layer++)
					{
						READPIXEL(image->layers[layer].x + x*image->flip, image->layers[layer].y + y*image->heightFactor);
						rgbaToPalette(PIX_R, PIX_G, PIX_B, PIX_A, &paletteIndex, &opaque); // Get palette index and opacity from pixel values
					}
					break;
			}

			// End span if we have a transparent pixel
			if (!opaque)
			{
				if (startofspan)
				{
					WRITE8(imgptr, 0);
				}
				startofspan = NULL;
				continue;
			}

			// Start new column if we need to
			if (!startofspan || spanSize == 255)
			{
				int writeY = y;

				// If we reached the span size limit, finish the previous span
				if (startofspan)
				{
					WRITE8(imgptr, 0);
				}

				if (y > 254)
				{
					// Make sure we're aligned to 254
					if (lastStartY < 254)
					{
						WRITE8(imgptr, 254);
						WRITE8(imgptr, 0);
						imgptr += 2;
						lastStartY = 254;
					}

					// Write stopgap empty spans if needed
					writeY = y - lastStartY;

					while (writeY > 254)
					{
						WRITE8(imgptr, 254);
						WRITE8(imgptr, 0);
						imgptr += 2;
						writeY -= 254;
					}
				}

				startofspan = imgptr;
				WRITE8(imgptr, writeY);///@TODO calculate starting y pos
				imgptr += 2;
				spanSize = 0;

				lastStartY = y;
			}

			// Write the pixel
			WRITE8(imgptr, paletteIndex);
			spanSize++;
			startofspan[1] = spanSize;
		}

		if (startofspan)
			WRITE8(imgptr, 0);

		WRITE8(imgptr, 0xFF);
	}

	*size = imgptr-convertedimage;
	img = malloc(*size);
	memcpy(img, convertedimage, *size);
	return img;
}

void addSkin(struct wadfile* wad)
{
	char buf[1<<16];
	int size;
	char prefix[5] = "____";
	uint32_t slen;

	if (cJSON_GetObjectItem(metadata, "prefix"))
		strncpy(prefix, cJSON_GetObjectItem(metadata, "prefix")->valuestring, 4);
	else
		strncpy(prefix, defprefix, 4);

	if (cJSON_GetObjectItem(metadata, "name"))
	{
		slen = strlen(cJSON_GetObjectItem(metadata, "name")->valuestring);
		strncpy(kskin.name, cJSON_GetObjectItem(metadata, "name")->valuestring, slen);
		kskin.name[slen] = '\0';
	}

	if (cJSON_GetObjectItem(metadata, "realname"))
	{
		slen = strlen(cJSON_GetObjectItem(metadata, "realname")->valuestring);
		printf("%s\n", cJSON_GetObjectItem(metadata, "realname")->valuestring);
		strncpy(kskin.realname, cJSON_GetObjectItem(metadata, "realname")->valuestring, slen);
		kskin.realname[slen] = '\0';
		printf("%s\n", kskin.realname);
	}

	if (cJSON_GetObjectItem(metadata, "stats"))
		kskin.kartspeed = cJSON_GetObjectItem(metadata, "stats")->child->valueint;

	if (cJSON_GetObjectItem(metadata, "stats"))
		kskin.kartweight = cJSON_GetObjectItem(metadata, "stats")->child->next->valueint;

	if (cJSON_GetObjectItem(metadata, "startcolor"))
		kskin.startcolor = cJSON_GetObjectItem(metadata, "startcolor")->valueint;

	if (cJSON_GetObjectItem(metadata, "prefcolor"))
	{
		slen = strlen(cJSON_GetObjectItem(metadata, "prefcolor")->valuestring);
		strncpy(kskin.prefcolor, cJSON_GetObjectItem(metadata, "prefcolor")->valuestring, slen);
	}

	size = sprintf(buf, S_SKIN_TEMPLATE, kskin.name,
		kskin.realname,
		prefix, prefix, prefix,
		kskin.kartspeed,
		kskin.kartweight,
		kskin.startcolor,
		kskin.prefcolor,
		prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix, prefix
	);
	add_lump(wad, NULL, "S_SKIN", size, buf);
}

// entrypoint
int main(int argc, char *argv[]) {
	char path[400]; // Total path of whatever file we're working with. Always contains the directory.
	char* filename; // Pointer to where to write filenames.
	struct wadfile* wad; // WAD to be created.
	FILE* wadf; // File pointer for writing the WAD.

	if (argc != 2) {
		printf("kartmaker <folder>: Converts a structured folder into an SRB2Kart character WAD. (Try dragging the folder onto the EXE!)");
		return 1;
	}

#define CLEAR_FILENAME() memset(filename, '\0', 400 - (filename - path))
#define SET_FILENAME(fn) ({CLEAR_FILENAME(); strcpy(filename, fn);})

	SetDefaultSkinValues();

	//@TODO load PLAYPAL.lmp from folder containing exe, not running folder (use argv[0] or something)
	strncpy(path, argv[0], 360);
	filename = path;
	while (*filename) filename++;
	while (*(filename-1) != '/' && *(filename-1) != '\\' && filename > path) filename--;
	SET_FILENAME("PLAYPAL.lmp");
	printf("%s\n", path);

	wadf = fopen(path, "rb");

	if (wadf == NULL)
	{
		fprintf(stderr, "Could not open file %s: %s\n", path, strerror(errno));
		return EXIT_FAILURE;
	}

	palInit = 1;

	fread(palette, 3, 256, wadf);
	fclose(wadf);

	// Initialize directory name and file stuff
	strncpy(path, argv[1], 360);
	filename = path;
	while (*filename) filename++;
	if (*(filename-1) == '/' || *(filename-1) == '\\') filename--;
	CLEAR_FILENAME();

	printf("Beginning to create WAD from path %s\n", path);

	// New WAD file
	wad = calloc(1, sizeof(struct wadfile));
	strncpy(wad->id, "PWAD", 4);

	// Open sprite sheet
	printf("Opening sprites.png... ");
	SET_FILENAME("/sprites.png");
	error = lodepng_decode32_file(&sprite_sheet, &sprites_width, &sprites_height, path);
	if (error) {
		printf("Can't open spritesheet! Error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	printf("Done.\n");

	// Open properties JSON
	printf("Opening properties.txt... ");
	SET_FILENAME("/properties.txt");
	metadata = loadJSON(path);
	if (!metadata) {
		printf("Properties file can't be opened or is malformed\n");
		return 1;
	}
	printf("Done.\n");

	// Read transparent color definitions
	readTransparentColors();

	// Process sprite sheet into separate sprites
	printf("Processing sprites...\n");
	processSprites();
	printf("Processing sprites... Done.\n");

	// Add sprites into WAD
	printf("Adding sprites to WAD...\n");
	{
		struct RGB_Sprite* sprite = rgb_sprites;
		while (sprite) {
			unsigned char* image;
			size_t size;
			printf(" Lump %s...\n", sprite->lumpname);
			image = imageInDoomFormat(sprite, &size);
			add_lump(wad, find_last_lump(wad), sprite->lumpname, size, image);
			free(image);

			sprite = sprite->next;
		}
	}
	printf("Adding sprites to WAD... Done.\n");

	// Add S_SKIN into WAD
	printf("Adding S_SKIN to WAD... ");
	addSkin(wad);
	printf("Done.\n");

	// Process graphics
	printf("Processing graphics...\n");
	processGfx();
	printf("Processing graphics... Done.\n");

	if (gfxstart)
	{
		printf("Adding graphics to WAD...\n");
		add_lump(wad, NULL, "GX_END", 0, NULL);
		{
			struct RGB_Sprite* sprite = gfxstart;
			while (sprite) {
				unsigned char* image;
				size_t size;

				image = imageInDoomFormat(sprite, &size);
				add_lump(wad, NULL, sprite->lumpname, size, image);
				free(image);

				sprite = sprite->next;
			}
		}
		add_lump(wad, NULL, "GX_START", 0, NULL);
		printf("Adding graphics to WAD... Done.\n");
	}

	// Add SFX into WAD
	{
		cJSON* item;
		if ((item = cJSON_GetObjectItem(metadata, "sfx")) == NULL)
		{
			printf("No sfx found, skipping...\n");
		}
		else
		{
			printf("Adding SFX to WAD...\n");
			add_lump(wad, NULL, "DS_END", 0, NULL);
			{
				char lumpname[9] = "DS______";

				if (cJSON_GetObjectItem(metadata, "prefix"))
					strncpy(lumpname+2, cJSON_GetObjectItem(metadata, "prefix")->valuestring, 4);
				else
					strncpy(lumpname+2, defprefix, 4);

				item = item->child;
				while (item != NULL) {
					unsigned char* buffer;
					off_t size, bytesRead;
					FILE* file;
					printf(" File %s... ", item->valuestring);

					strncpy(lumpname+6, item->string, 2);
					SET_FILENAME(item->valuestring);

					file = fopen(path, "rb");

					// seek to end of file
					fseek(file, 0, SEEK_END);

					// Load file into buffer
					size = ftell(file);
					buffer = malloc(size);

					// seek back to start
					fseek(file, 0, SEEK_SET);

					//read contents!
					bytesRead = fread(buffer, 1, size, file);
					fclose(file);

					add_lump(wad, NULL, lumpname, bytesRead, buffer);

					item = item->next;
					printf("Done.\n");
				}
			}
			add_lump(wad, NULL, "DS_START", 0, NULL);
			printf("Adding SFX to WAD... Done.\n");
		}
	}

	// Write WAD and exit
	SET_FILENAME(".wad");
	wadf = fopen(path, "wb");
	write_wadfile(wadf, wad);
	fclose(wadf);

    return EXIT_SUCCESS;
}
