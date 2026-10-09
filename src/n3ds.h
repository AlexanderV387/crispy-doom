//
// Copyright(C) 2026 Alexander Vinueza
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Nintendo 3DS: startup, the screens, options and fatal errors.
//

#ifndef N3DS_H
#define N3DS_H

#ifdef __3DS__

#include <stdio.h>

#include "SDL.h"
#include "doomtype.h"

// Folder of the game data, the configuration and the saved games.
#define N3DS_DATA_DIR "sdmc:/3ds/crispy-doom/"

// What SDL's own 3DS main would do (804 MHz on the New 3DS), and the data
// folder becomes the current directory, where the IWADs are looked for.
void N3DS_Init(void);

// Shows the 8-bit frame on the top screen (400x240): stretched from its
// height to 240 rows (Doom's 1.2 pixel aspect ratio) and centered when
// narrower than 400, then waits for the vertical blank. SDL's software
// renderer on the 3DS scales and rotates with the CPU and ignores vsync.
void N3DS_Present(const pixel_t *buffer, int width, int height,
                  const SDL_Color *palette, boolean palette_changed);

// The HOME Menu is closing the game: only quit, as soon as possible.
boolean N3DS_IsClosing(void);

// The configuration was saved when the HOME Menu opened, and the game is
// being closed from there: no need to write it again.
boolean N3DS_ConfigAlreadySaved(void);

// Set by the game once its configuration is loaded: saves it (called when
// the HOME Menu opens, as the game may be closed from there).
extern void (*N3DS_SaveOnSuspend)(void);

// Options of the 3DS, saved in the configuration file.
extern int n3ds_bottom_map;     // the automap on the bottom screen
extern int n3ds_hud_position;   // status bar: 0 top, 1 middle, 2 bottom
extern int n3ds_smooth_scaling; // blend rows when stretching to 240 rows
extern int n3ds_show_fps;       // frame counter on the bottom screen
extern int n3ds_config_version; // for changes to older configurations

void N3DS_BindVariables(void);

// Bottom screen, 8-bit like the game's own screen.
#define N3DS_BOTTOM_WIDTH 320
#define N3DS_BOTTOM_HEIGHT 240

// Set by the game: draws the bottom screen and returns whether it shows
// anything. When it does not, it stays black and a tap turns it off.
extern boolean (*N3DS_DrawBottom)(pixel_t *screen);

// Opens a file to write it again from the start, over its old contents:
// on the SD card, writing a new or emptied file sometimes takes seconds (8
// on a large card) while space is found for it. N3DS_CloseRewrite cuts it
// to what was written.
FILE *N3DS_OpenRewrite(const char *path);
void N3DS_CloseRewrite(FILE *file);

// Shows a fatal error on the top screen until START is pressed: the 3DS has
// no message boxes, so without it the game would just close.
void N3DS_ShowError(const char *message);

#endif // __3DS__

#endif // N3DS_H
