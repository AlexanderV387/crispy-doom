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
//     Nintendo 3DS: Doom's bottom screen. With the full view (no status
//     bar on the top screen), the status bar and the automap share it:
//     the bar above the map, or below it with its option. With a smaller
//     view the bar is on the top screen, as in the original, and the map
//     fills the bottom screen. Only its option hides the map. Buttons on
//     the map zoom it. With their option, weapon buttons at the bottom.
//

#ifdef __3DS__

#include <stdlib.h>
#include <string.h>

#include "am_map.h"
#include "crispy.h"
#include "doomstat.h"
#include "i_video.h"
#include "m_menu.h"
#include "n3ds.h"
#include "n3ds_doom.h"
#include "st_stuff.h"
#include "v_video.h"

// Full view without the Crispy HUD: the status bar goes to the bottom.
#define FULL_VIEW 11

// The status bar (320x32) drawn by its own functions, as at the bottom of
// the game's screen, into a screen of its own; copied at row y.
static void DrawStatusBar(pixel_t *screen, int y)
{
    static pixel_t *bar_screen = NULL;
    static int bar_screen_size = 0;
    pixel_t *const saved_buffer = I_VideoBuffer;
    const int step = 1 << crispy->hires;
    const int top = SCREENHEIGHT - (ST_HEIGHT << crispy->hires);
    const int left = WIDESCREENDELTA << crispy->hires;
    int row, x;

    if (bar_screen_size < SCREENWIDTH * SCREENHEIGHT)
    {
        free(bar_screen);
        bar_screen_size = SCREENWIDTH * SCREENHEIGHT;
        bar_screen = calloc(bar_screen_size, sizeof(*bar_screen));
    }

    I_VideoBuffer = bar_screen;
    V_RestoreBuffer();
    ST_Drawer(false, true);
    I_VideoBuffer = saved_buffer;
    V_RestoreBuffer();

    for (row = 0; row < ST_HEIGHT; ++row)
    {
        const pixel_t *source = bar_screen + (top + row * step) * SCREENWIDTH
                              + left;
        pixel_t *dest = screen + (y + row) * N3DS_BOTTOM_WIDTH;

        for (x = 0; x < ORIGWIDTH; ++x)
        {
            dest[x] = source[x * step];
        }
    }
}

// The map at row y, with its zoom buttons (gray, black and white in
// Doom's palette).
static void DrawMap(pixel_t *screen, int y, int height)
{
    const int zoom = N3DS_UpdateMapZoom(y, height);

    AM_DrawBottom(screen, y, N3DS_BOTTOM_WIDTH, height, zoom);
    N3DS_DrawMapButtons(screen, y, height, 100, 0, 4);
}

// The weapon buttons: Doom's seven numbers, the chainsaw with the fist and
// the super shotgun with the shotgun, as their keys.
static void DrawWeaponButtons(pixel_t *screen, int y)
{
    static const int slot_of[NUMWEAPONS] =
    {
        [wp_fist] = 0, [wp_pistol] = 1, [wp_shotgun] = 2, [wp_chaingun] = 3,
        [wp_missile] = 4, [wp_plasma] = 5, [wp_bfg] = 6, [wp_chainsaw] = 0,
        [wp_supershotgun] = 2,
    };
    const player_t *const player = &players[displayplayer];
    unsigned owned = 0;
    int w;

    for (w = 0; w < NUMWEAPONS; ++w)
    {
        if (player->weaponowned[w])
        {
            owned |= 1u << slot_of[w];
        }
    }

    N3DS_DrawWeaponButtons(screen, y, 7, owned,
                           player->readyweapon < NUMWEAPONS
                           ? slot_of[player->readyweapon] : -1);
    N3DS_UpdateWeaponButtons(y, 7);
}

boolean N3DS_DoomInGame(void)
{
    return gamestate == GS_LEVEL && !menuactive && !demoplayback;
}

boolean N3DS_DoomDrawBottom(pixel_t *screen)
{
    const boolean bar = screenblocks == FULL_VIEW;
    const boolean map = n3ds_bottom_map != 0;
    const boolean weapons = n3ds_weapon_bar != 0;
    // The weapon buttons take the bottom rows; the rest as before.
    const int height = N3DS_BOTTOM_HEIGHT - (weapons ? N3DS_WEAPON_BAR_HEIGHT : 0);
    const int map_height = height - (bar ? ST_HEIGHT : 0);

    // The buttons set it again when they are drawn.
    N3DS_ClearReservedTouch();

    if (gamestate != GS_LEVEL || !gametic || players[displayplayer].mo == NULL
     || (!bar && !map && !weapons))
    {
        return false;
    }

    if (!map)
    {
        // Top, middle or bottom of the screen.
        memset(screen, 0, N3DS_BOTTOM_WIDTH * N3DS_BOTTOM_HEIGHT * sizeof(*screen));

        if (bar)
        {
            DrawStatusBar(screen, n3ds_hud_position * (height - ST_HEIGHT) / 2);
        }
    }
    else if (bar && n3ds_hud_position == 2)
    {
        DrawMap(screen, 0, map_height);
        DrawStatusBar(screen, map_height);
    }
    else
    {
        if (bar)
        {
            DrawStatusBar(screen, 0);
        }

        DrawMap(screen, height - map_height, map_height);
    }

    if (weapons)
    {
        memset(screen + height * N3DS_BOTTOM_WIDTH, 0,
               N3DS_WEAPON_BAR_HEIGHT * N3DS_BOTTOM_WIDTH * sizeof(*screen));
        DrawWeaponButtons(screen, height);
    }

    return true;
}

#endif // __3DS__
