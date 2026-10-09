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
//     Nintendo 3DS: startup, the screen and fatal errors.
//

#ifndef N3DS_H
#define N3DS_H

#ifdef __3DS__

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

// Shows a fatal error on the top screen until START is pressed: the 3DS has
// no message boxes, so without it the game would just close.
void N3DS_ShowError(const char *message);

#endif // __3DS__

#endif // N3DS_H
