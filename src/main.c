/*
	Followermaker. Created by Superstarxalien based on Kartmaker.
	Takes a working folder (examples provided) and converts it into a follower WAD for Dr. Robotnik's Ring Racers.
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "cJSON.h"
#include "lodepng.h"
#include "lump.h"

// If your sprites are bigger than 256*256, consider a different approach than this program?
#define MAX_IMAGE_SIZE 256*256

// it's only a snippet, the whole thing can't be a macro since it's variable now
#define FOLLOWER_SOC_SNIPPET_TEMPLATE "%s\nFOLLOWER\nName = %s\nIcon = ICOF%s\nCategory = %s\nHornSound = DSFH%s\nDefaultColor = %s\nMode = %s\nScale = %d*FRACUNIT\nBubbleScale = %d*FRACUNIT\nAtAngle = %d\nDistance = %d*FRACUNIT\nHeight = %d*FRACUNIT\nZOffs = %d*FRACUNIT\nHorzLag = %d*FRACUNIT\nVertLag = %d*FRACUNIT\nAngleLag = %d*FRACUNIT\nBobAmp = %d*FRACUNIT\nBobSpeed = %d*FRACUNIT\nHitConfirmTime = TICRATE*%d\n"

#define FOLLOWERNAMESIZE 16

// Since strupr doesn't actually exist in the standard C libraries it's defined here so its usage no longer breaks compatibility.
char* strupr(char* s)
{
    char* tmp = s;

    for (;*tmp;++tmp) {
        *tmp = toupper((unsigned char) *tmp);
    }

    return s;
}
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

// struct for follower data included in the SOC
struct followerstructthingwhatever {
	char name[FOLLOWERNAMESIZE];
	char category[FOLLOWERNAMESIZE]; // maybe follows the name size as well?
	char prefcolor[32];
	char mode[8]; // if floating or on ground
	char scale;
	char bubblescale;
	short atangle;
	char distance;
	uint8_t height;
	uint8_t zoffs;
	char horzlag;
	char vertlag;
	char anglelag;
	char bobamp;
	char bobspeed;
	char hitconfirmtime;

	char idleanimationspeed;
	char followinganimationspeed;
	char hurtanimationspeed;
	char loseanimationspeed;
	char winanimationspeed;
	char hitconfirmanimationspeed;
	char ringanimationspeed;

	char numstates;
	uint8_t highestanimframeletter;
	uint8_t followerstateanimframestart[7];
	struct state {
		uint8_t idle:1;
		uint8_t following:1;
		uint8_t hurt:1;
		uint8_t lose:1;
		uint8_t win:1;
		uint8_t hitconfirm:1;
		uint8_t ring:1;
		uint8_t padding:1;
	} states;
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

struct followerstructthingwhatever kfollower;

char defprefix[4] = "SOME";

unsigned char* pix;
#define READPIXEL(x, y) (pix = sprite_sheet + ((x) + (y)*sprites_width) * 4)
#define PIX_R pix[0]
#define PIX_G pix[1]
#define PIX_B pix[2]
#define PIX_A pix[3]

uint8_t getFixedAnimationIndex(uint8_t id)
{
	// accounting for animation frame ranges outside of uppercase A-Z
	if (id > 0x5A)
	{
		if (id <= 0x64)
		{
			// numbers range
			id -= 0x2B;
		}
		else if (id <= 0x7E)
		{
			// lowercase letters range
			id -= 4;
		}
		else
		{
			// ! and @
			if (id == 0x7F)
				id = 0x21;
			if (id == 0x80)
				id = 0x40;
		}
	}

	return id;
}

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

// sets placeholder follower values, to be replaced by the user's own input
void SetDefaultFollowerValues(void)
{
	strncpy(kfollower.name, "someone", 8);
	kfollower.name[8] = '\0';

	strncpy(kfollower.category, "beta", 5);
	kfollower.category[5] = '\0';

	strncpy(kfollower.prefcolor, "Green", 6);
	kfollower.prefcolor[6] = '\0';

	strncpy(kfollower.mode, "FLOAT", 6);
	kfollower.mode[6] = '\0';

	kfollower.scale = 1;
	kfollower.bubblescale = 0;
	kfollower.atangle = 230;
	kfollower.distance = 40;
	kfollower.height = 32;
	kfollower.zoffs = 32;
	kfollower.horzlag = 3;
	kfollower.vertlag = 6;
	kfollower.anglelag = 8;
	kfollower.bobamp = 4;
	kfollower.bobspeed = 70;
	kfollower.hitconfirmtime = 1;

	kfollower.idleanimationspeed = 35;
	kfollower.followinganimationspeed = 35;
	kfollower.hurtanimationspeed = 35;
	kfollower.loseanimationspeed = 35;
	kfollower.winanimationspeed = 35;
	kfollower.hitconfirmanimationspeed = 35;
	kfollower.ringanimationspeed = 35;

	kfollower.numstates = 0;
	kfollower.highestanimframeletter = 0x41;
	kfollower.followerstateanimframestart[0] = 0x41; // uppercase A in ASCII
	kfollower.states.idle = 1;
	kfollower.states.following = 0;
	kfollower.states.hurt = 0;
	kfollower.states.lose = 0;
	kfollower.states.win = 0;
	kfollower.states.hitconfirm = 0;
	kfollower.states.ring = 0;
}

// processes sprites on the template, which are separated by regions (usually visualized on the template image as squares)
void processSprites(void) {
	// properties.txt fields, read as json
	cJSON *item, *nesteditem, *prop;

	// sprite-related
	int spr_width, spr_height, step_width, step_height, stepw, steph;
	struct RGB_Sprite* cursprite;

	// to do with automating animation frame order indices
	uint8_t curanimframeletter = 0x41;
	uint8_t lastanimframeletterinstate = 0;
	uint8_t curstate = 0;
	uint8_t laststate = 0;

	kfollower.highestanimframeletter = 0x41;

	// prefix for follower files
	char prefix[5] = "____";

	// set prefix
	item = cJSON_GetObjectItem(metadata, "prefix");
	if (item)
		strncpy(prefix, strupr(item->valuestring), 4);
	else
		strncpy(prefix, defprefix, 4);

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
		// keep track of follower state which sprites are being read
		// "idle" is the first follower state, the rest of which follow it (pun unintended)
		// the "graphics" field is for the follower's icon on the menu
		// mainly used for ordering animation frames as seen below
		if ((!(strcmp(item->string, "idle") == 0)) && (!(strcmp(item->string, "graphics") == 0)))
		{
			curstate++;
			if (strcmp(item->string, "following") == 0)
				kfollower.states.following = 1;
			if (strcmp(item->string, "hurt") == 0)
				kfollower.states.hurt = 1;
			if (strcmp(item->string, "lose") == 0)
				kfollower.states.lose = 1;
			if (strcmp(item->string, "win") == 0)
				kfollower.states.win = 1;
			if (strcmp(item->string, "hitconfirm") == 0)
				kfollower.states.hitconfirm = 1;
			if (strcmp(item->string, "ring") == 0)
				kfollower.states.ring = 1;
		}
		lastanimframeletterinstate = 0;

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

			// handle automatic animation frame ordering
			// animation frames of sprites use a letter-based ordering, from A to Z
			// the idea is to be able to detect the highest letter used for an animation frame within a follower state and go above it upon reading sprites for the next state
			if (!(strcmp(item->string, "graphics") == 0))
			{
				curanimframeletter = nesteditem->string[0];

				// i more or less did this in almost complete mental fog
				// i can't tell you how i arrived here, sorry
				if (curstate > laststate)
				{
					if (curanimframeletter > lastanimframeletterinstate)
						curanimframeletter = kfollower.highestanimframeletter + 1;
					else
						curanimframeletter = kfollower.highestanimframeletter;
				}

				// this will store how many frames of animation are in the sprite for all of the follower states
				if (curanimframeletter > kfollower.highestanimframeletter)
					kfollower.highestanimframeletter = curanimframeletter;

				// store letter in which follower state animation begins
				if (lastanimframeletterinstate == 0)
					kfollower.followerstateanimframestart[curstate] = kfollower.highestanimframeletter;

				lastanimframeletterinstate = nesteditem->string[0];

				// once all alphabet letters are exhausted in the JSON file, the program simply uses whatever's next on the ascii table
				// obviously the game won't like that, so those entries get converted to the proper format after the fact
				nesteditem->string[0] = getFixedAnimationIndex(curanimframeletter);
				
				// for flipped sprites (e.g. A2A8)
				if (strlen(nesteditem->string) > 3)
					nesteditem->string[2] = nesteditem->string[0];
			}

			// this sucks
			if (!(strcmp(item->string, "graphics") == 0))
				sprintf(cursprite->lumpname, "%s%s", prefix, nesteditem->string);
			else
				sprintf(cursprite->lumpname, "ICOF%s", prefix);

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
		laststate = curstate;
	}

	kfollower.numstates = laststate;

	printf("Reading sprites... Done.\n");
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

// generate SOC
void addFollower(struct wadfile* wad)
{
	char buf[1<<16];
	char prebuf[1<<16];
	char ff_animate[16];
	char var1 = 0;
	int size;
	char prefix[5] = "____";
	uint32_t slen;

	if (cJSON_GetObjectItem(metadata, "prefix"))
		strncpy(prefix, strupr(cJSON_GetObjectItem(metadata, "prefix")->valuestring), 4);
	else
		strncpy(prefix, defprefix, 4);

	if (cJSON_GetObjectItem(metadata, "name"))
	{
		slen = strlen(cJSON_GetObjectItem(metadata, "name")->valuestring);
		strncpy(kfollower.name, cJSON_GetObjectItem(metadata, "name")->valuestring, slen);
		kfollower.name[slen] = '\0';
	}

	if (cJSON_GetObjectItem(metadata, "category"))
	{
		slen = strlen(cJSON_GetObjectItem(metadata, "category")->valuestring);
		strncpy(kfollower.category, cJSON_GetObjectItem(metadata, "category")->valuestring, slen);
		kfollower.category[slen] = '\0';
	}

	if (cJSON_GetObjectItem(metadata, "prefcolor"))
	{
		slen = strlen(cJSON_GetObjectItem(metadata, "prefcolor")->valuestring);
		strncpy(kfollower.prefcolor, cJSON_GetObjectItem(metadata, "prefcolor")->valuestring, slen);
		kfollower.prefcolor[slen] = '\0';
	}

	if (cJSON_GetObjectItem(metadata, "mode"))
	{
		slen = strlen((cJSON_GetObjectItem(metadata, "mode")->valueint == 0) ? "FLOAT" : "GROUND");
		strncpy(kfollower.mode, (cJSON_GetObjectItem(metadata, "mode")->valueint == 0) ? "FLOAT" : "GROUND", slen);
		kfollower.mode[slen] = '\0';
	}

	if (cJSON_GetObjectItem(metadata, "scale"))
		kfollower.scale = cJSON_GetObjectItem(metadata, "scale")->valueint;
	if (cJSON_GetObjectItem(metadata, "bubblescale"))
		kfollower.bubblescale = cJSON_GetObjectItem(metadata, "bubblescale")->valueint;
	if (cJSON_GetObjectItem(metadata, "atangle"))
		kfollower.atangle = cJSON_GetObjectItem(metadata, "atangle")->valueint;
	if (cJSON_GetObjectItem(metadata, "distance"))
		kfollower.distance = cJSON_GetObjectItem(metadata, "distance")->valueint;
	if (cJSON_GetObjectItem(metadata, "height"))
		kfollower.height = cJSON_GetObjectItem(metadata, "height")->valueint;
	if (cJSON_GetObjectItem(metadata, "zoffs"))
		kfollower.zoffs = cJSON_GetObjectItem(metadata, "zoffs")->valueint;
	if (cJSON_GetObjectItem(metadata, "horzlag"))
		kfollower.horzlag = cJSON_GetObjectItem(metadata, "horzlag")->valueint;
	if (cJSON_GetObjectItem(metadata, "vertlag"))
		kfollower.vertlag = cJSON_GetObjectItem(metadata, "vertlag")->valueint;
	if (cJSON_GetObjectItem(metadata, "anglelag"))
		kfollower.anglelag = cJSON_GetObjectItem(metadata, "anglelag")->valueint;
	if (cJSON_GetObjectItem(metadata, "bobamp"))
		kfollower.bobamp = cJSON_GetObjectItem(metadata, "bobamp")->valueint;
	if (cJSON_GetObjectItem(metadata, "bobspeed"))
		kfollower.bobspeed = cJSON_GetObjectItem(metadata, "bobspeed")->valueint;
	if (cJSON_GetObjectItem(metadata, "hitconfirmtime"))
		kfollower.hitconfirmtime = cJSON_GetObjectItem(metadata, "hitconfirmtime")->valueint;

	if (cJSON_GetObjectItem(metadata, "idle_animation_speed"))
		kfollower.idleanimationspeed = cJSON_GetObjectItem(metadata, "idle_animation_speed")->valueint;
	if (cJSON_GetObjectItem(metadata, "following_animation_speed"))
		kfollower.followinganimationspeed = cJSON_GetObjectItem(metadata, "following_animation_speed")->valueint;
	if (cJSON_GetObjectItem(metadata, "hurt_animation_speed"))
		kfollower.hurtanimationspeed = cJSON_GetObjectItem(metadata, "hurt_animation_speed")->valueint;
	if (cJSON_GetObjectItem(metadata, "lose_animation_speed"))
		kfollower.loseanimationspeed = cJSON_GetObjectItem(metadata, "lose_animation_speed")->valueint;
	if (cJSON_GetObjectItem(metadata, "win_animation_speed"))
		kfollower.winanimationspeed = cJSON_GetObjectItem(metadata, "win_animation_speed")->valueint;
	if (cJSON_GetObjectItem(metadata, "hitconfirm_animation_speed"))
		kfollower.hitconfirmanimationspeed = cJSON_GetObjectItem(metadata, "hitconfirm_animation_speed")->valueint;
	if (cJSON_GetObjectItem(metadata, "ring_animation_speed"))
		kfollower.winanimationspeed = cJSON_GetObjectItem(metadata, "ring_animation_speed")->valueint;

	sprintf(prebuf, "FREESLOT\nSPR_%s\nsfx_FH%s\nS_%sIDLE\n", prefix, prefix, prefix);

	if (kfollower.states.following)
		sprintf(prebuf, "%sS_%sFOLLOW\n", prebuf, prefix);
	if (kfollower.states.hurt)
		sprintf(prebuf, "%sS_%sHURT\n", prebuf, prefix);
	if (kfollower.states.lose)
		sprintf(prebuf, "%sS_%sLOSE\n", prebuf, prefix);
	if (kfollower.states.win)
		sprintf(prebuf, "%sS_%sWIN\n", prebuf, prefix);
	if (kfollower.states.hitconfirm)
		sprintf(prebuf, "%sS_%sHITCONFIRM\n", prebuf, prefix);
	if (kfollower.states.ring)
		sprintf(prebuf, "%sS_%sRING\n", prebuf, prefix);

	if (kfollower.highestanimframeletter > 0x41 && !(kfollower.numstates > 0))
	{
		sprintf(ff_animate, "A|FF_ANIMATE");
		var1 = kfollower.highestanimframeletter - 0x41;
	}
	else
	{
		if ((kfollower.followerstateanimframestart[1] - kfollower.followerstateanimframestart[0]) > 1)
		{
			sprintf(ff_animate, "A|FF_ANIMATE");
			var1 = (kfollower.followerstateanimframestart[1] - kfollower.followerstateanimframestart[0]) - 1;
		}
		else
			sprintf(ff_animate, "A");
	}

	sprintf(
		prebuf, "%s\nSTATE S_%sIDLE\nSpriteName = SPR_%s\nSpriteFrame = %s\nDuration = -1\nVar1 = %d #no. of sprites (starts from 0)\nVar2 = %d #animation speed\nNext = S_%sIDLE\n",
		prebuf, prefix, prefix, ff_animate, var1, kfollower.idleanimationspeed, prefix
	);

	if (kfollower.numstates > 0)
	{
		if (kfollower.states.following)
		{
			if (kfollower.numstates > 1)
			{
				if ((kfollower.followerstateanimframestart[2] - kfollower.followerstateanimframestart[1]) > 1)
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = (kfollower.followerstateanimframestart[2] - kfollower.followerstateanimframestart[1]) - 1;
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			else
			{
				if (kfollower.highestanimframeletter > kfollower.followerstateanimframestart[1])
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = kfollower.highestanimframeletter - kfollower.followerstateanimframestart[1];
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			ff_animate[0] = getFixedAnimationIndex(kfollower.followerstateanimframestart[1]);
			sprintf(prebuf, "%s\nSTATE S_%sFOLLOW\nSpriteName = SPR_%s\nSpriteFrame = %s\nDuration = -1\nVar1 = %d #no. of sprites (starts from 0)\nVar2 = %d #animation speed\nNext = S_%sFOLLOW\n", prebuf, prefix, prefix, ff_animate, var1, kfollower.followinganimationspeed, prefix);
		}
		if (kfollower.states.hurt)
		{
			if (kfollower.numstates > 2)
			{
				if ((kfollower.followerstateanimframestart[3] - kfollower.followerstateanimframestart[2]) > 1)
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = (kfollower.followerstateanimframestart[3] - kfollower.followerstateanimframestart[2]) - 1;
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			else
			{
				if (kfollower.highestanimframeletter > kfollower.followerstateanimframestart[2])
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = kfollower.highestanimframeletter - kfollower.followerstateanimframestart[2];
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			ff_animate[0] = getFixedAnimationIndex(kfollower.followerstateanimframestart[2]);
			sprintf(prebuf, "%s\nSTATE S_%sHURT\nSpriteName = SPR_%s\nSpriteFrame = %s\nDuration = -1\nVar1 = %d #no. of sprites (starts from 0)\nVar2 = %d #animation speed\nNext = S_%sHURT\n", prebuf, prefix, prefix, ff_animate, var1, kfollower.hurtanimationspeed, prefix);
		}
		if (kfollower.states.lose)
		{
			if (kfollower.numstates > 3)
			{
				if ((kfollower.followerstateanimframestart[4] - kfollower.followerstateanimframestart[3]) > 1)
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = (kfollower.followerstateanimframestart[4] - kfollower.followerstateanimframestart[3]) - 1;
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			else
			{
				if (kfollower.highestanimframeletter > kfollower.followerstateanimframestart[3])
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = kfollower.highestanimframeletter - kfollower.followerstateanimframestart[3];
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			ff_animate[0] = getFixedAnimationIndex(kfollower.followerstateanimframestart[3]);
			sprintf(prebuf, "%s\nSTATE S_%sLOSE\nSpriteName = SPR_%s\nSpriteFrame = %s\nDuration = -1\nVar1 = %d #no. of sprites (starts from 0)\nVar2 = %d #animation speed\nNext = S_%sLOSE\n", prebuf, prefix, prefix, ff_animate, var1, kfollower.loseanimationspeed, prefix);
		}
		if (kfollower.states.win)
		{
			if (kfollower.numstates > 4)
			{
				if ((kfollower.followerstateanimframestart[5] - kfollower.followerstateanimframestart[4]) > 1)
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = (kfollower.followerstateanimframestart[5] - kfollower.followerstateanimframestart[4]) - 1;
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			else
			{
				if (kfollower.highestanimframeletter > kfollower.followerstateanimframestart[4])
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = kfollower.highestanimframeletter - kfollower.followerstateanimframestart[4];
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			ff_animate[0] = getFixedAnimationIndex(kfollower.followerstateanimframestart[4]);
			sprintf(prebuf, "%s\nSTATE S_%sWIN\nSpriteName = SPR_%s\nSpriteFrame = %s\nDuration = -1\nVar1 = %d #no. of sprites (starts from 0)\nVar2 = %d #animation speed\nNext = S_%sWIN\n", prebuf, prefix, prefix, ff_animate, var1, kfollower.winanimationspeed, prefix);
		}
		if (kfollower.states.hitconfirm)
		{
			if (kfollower.numstates > 5)
			{
				if ((kfollower.followerstateanimframestart[6] - kfollower.followerstateanimframestart[5]) > 1)
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = (kfollower.followerstateanimframestart[6] - kfollower.followerstateanimframestart[5]) - 1;
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			else
			{
				if (kfollower.highestanimframeletter > kfollower.followerstateanimframestart[5])
				{
					sprintf(ff_animate, "A|FF_ANIMATE");
					var1 = kfollower.highestanimframeletter - kfollower.followerstateanimframestart[5];
				}
				else
				{
					sprintf(ff_animate, "A");
					var1 = 0;
				}
			}
			ff_animate[0] = getFixedAnimationIndex(kfollower.followerstateanimframestart[5]);
			sprintf(prebuf, "%s\nSTATE S_%sHITCONFIRM\nSpriteName = SPR_%s\nSpriteFrame = %s\nDuration = -1\nVar1 = %d #no. of sprites (starts from 0)\nVar2 = %d #animation speed\nNext = S_%sHITCONFIRM\n", prebuf, prefix, prefix, ff_animate, var1, kfollower.hitconfirmanimationspeed, prefix);
		}
		if (kfollower.states.ring)
		{
			if (kfollower.highestanimframeletter > kfollower.followerstateanimframestart[6])
			{
				sprintf(ff_animate, "A|FF_ANIMATE");
				var1 = kfollower.highestanimframeletter - kfollower.followerstateanimframestart[6];
			}
			else
			{
				sprintf(ff_animate, "A");
				var1 = 0;
			}
			ff_animate[0] = getFixedAnimationIndex(kfollower.followerstateanimframestart[6]);
			sprintf(prebuf, "%s\nSTATE S_%sRING\nSpriteName = SPR_%s\nSpriteFrame = %s\nDuration = -1\nVar1 = %d #no. of sprites (starts from 0)\nVar2 = %d #animation speed\nNext = S_%sRING\n", prebuf, prefix, prefix, ff_animate, var1, kfollower.ringanimationspeed, prefix);
		}
	}

	sprintf(prebuf, FOLLOWER_SOC_SNIPPET_TEMPLATE, prebuf,
		kfollower.name,
		prefix,
		kfollower.category,
		prefix,
		kfollower.prefcolor,
		kfollower.mode,
		kfollower.scale,
		kfollower.bubblescale,
		kfollower.atangle,
		kfollower.distance,
		kfollower.height,
		kfollower.zoffs,
		kfollower.horzlag,
		kfollower.vertlag,
		kfollower.anglelag,
		kfollower.bobamp,
		kfollower.bobspeed,
		kfollower.hitconfirmtime
	);

	sprintf(prebuf, "%sIdleState = S_%sIDLE\n", prebuf, prefix);

	if (kfollower.numstates > 0)
	{
		if (kfollower.states.following)
			sprintf(prebuf, "%sFollowState = S_%sFOLLOW\n", prebuf, prefix);
		if (kfollower.states.hurt)
			sprintf(prebuf, "%sHurtState = S_%sHURT\n", prebuf, prefix);
		if (kfollower.states.lose)
			sprintf(prebuf, "%sLoseState = S_%sLOSE\n", prebuf, prefix);
		if (kfollower.states.win)
			sprintf(prebuf, "%sWinState = S_%sWIN\n", prebuf, prefix);
		if (kfollower.states.hitconfirm)
			sprintf(prebuf, "%sHitConfirmState = S_%sHITCONFIRM\n", prebuf, prefix);
		if (kfollower.states.ring)
			sprintf(prebuf, "%sRingState = S_%sRING\n", prebuf, prefix);
	}

	size = sprintf(buf, prebuf);
		
	add_lump(wad, NULL, "SOC_FLLW", size, buf);
}

// entrypoint
int main(int argc, char *argv[]) {
	char path[400]; // Total path of whatever file we're working with. Always contains the directory.
	char* filename; // Pointer to where to write filenames.
	struct wadfile* wad; // WAD to be created.
	FILE* wadf; // File pointer for writing the WAD.

	char iconlump[9];

	if (argc != 2) {
		printf("followermaker <folder>: Converts a structured folder into a Dr. Robotnik's Ring Racers follower WAD. (Try dragging the folder onto the executable!)");
		return 1;
	}

#define CLEAR_FILENAME() memset(filename, '\0', 400 - (filename - path))
#define SET_FILENAME(fn) ({CLEAR_FILENAME(); strcpy(filename, fn);})

	SetDefaultFollowerValues();

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

	if (cJSON_GetObjectItem(metadata, "prefix"))
		sprintf(iconlump, "ICOF%s", strupr(cJSON_GetObjectItem(metadata, "prefix")->valuestring));
	else
		sprintf(iconlump, "ICOF%s", defprefix);
	iconlump[8] = '\0';

	struct RGB_Sprite* sprite = rgb_sprites;
	while (sprite) {
		unsigned char* image;
		size_t size;
		printf(" Lump %s...\n", sprite->lumpname);
		image = imageInDoomFormat(sprite, &size);

		if (strcmp(sprite->lumpname, iconlump) == 0)
		{
			add_lump(wad, find_last_lump(wad), "S_END", 0, NULL);
		}

		add_lump(wad, find_last_lump(wad), sprite->lumpname, size, image);
		free(image);

		sprite = sprite->next;
	}
	add_lump(wad, NULL, "S_START", 0, NULL);
	printf("Adding sprites to WAD... Done.\n");

	// Add SOC_FLLW into WAD
	printf("Adding SOC_FLLW to WAD... ");
	addFollower(wad);
	printf("Done.\n");

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
	printf("Adding SFX to WAD...\n");
	add_lump(wad, NULL, "DS_END", 0, NULL);
	
	char lumpname[9] = "DSFH____";

	if (cJSON_GetObjectItem(metadata, "prefix"))
		strncpy(lumpname+4, cJSON_GetObjectItem(metadata, "prefix")->valuestring, 4);
	else
		strncpy(lumpname+4, defprefix, 4);

	unsigned char* buffer;
	off_t size, bytesRead;
	FILE* file;

	SET_FILENAME("/follower_sound.ogg");

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

	printf("Done.\n");
	
	add_lump(wad, NULL, "DS_START", 0, NULL);
	printf("Adding SFX to WAD... Done.\n");

	// Write WAD and exit
	SET_FILENAME(".wad");
	wadf = fopen(path, "wb");
	write_wadfile(wadf, wad);
	fclose(wadf);

    return EXIT_SUCCESS;
}
