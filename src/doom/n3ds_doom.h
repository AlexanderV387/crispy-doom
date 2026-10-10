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
//     Nintendo 3DS: Doom's bottom screen.
//

#ifndef N3DS_DOOM_H
#define N3DS_DOOM_H

#ifdef __3DS__

#include "doomtype.h"

// Status bar and automap on the bottom screen (see N3DS_DrawBottom).
boolean N3DS_DoomDrawBottom(pixel_t *screen);

// Playing: in a level, with no menu (see N3DS_InGame).
boolean N3DS_DoomInGame(void);

#endif // __3DS__

#endif // N3DS_DOOM_H
