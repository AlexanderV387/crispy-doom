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
//     Nintendo 3DS: Strife's bottom screen, as Doom's (doom/n3ds_doom.c).
//     With the full view, the status bar (40 rows here: the inventory bar
//     and the strip above it with health and ammo) and the automap share
//     it; with a smaller view the map fills it.
//

#ifdef __3DS__

#include <stdlib.h>
#include <string.h>

#include "am_map.h"
#include "crispy.h"
#include "doomstat.h"
#include "d_event.h"
#include "i_video.h"
#include "m_controls.h"
#include "m_menu.h"
#include "n3ds.h"
#include "n3ds_strife.h"
#include "st_stuff.h"
#include "v_trans.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

// Full view: the status bar goes to the bottom screen.
#define FULL_VIEW 11

// The strip above the inventory bar (health, ammo) and the bar.
#define BAR_HEIGHT (ST_HEIGHT + 8)

boolean N3DS_StrifeInGame(void)
{
    return gamestate == GS_LEVEL && !menuactive && !demoplayback;
}

// The status bar drawn by its own functions, as at the bottom of the game's
// screen, into a screen of its own; copied at row y.
static void DrawStatusBar(pixel_t *screen, int y)
{
    static pixel_t *bar_screen = NULL;
    static int bar_screen_size = 0;
    pixel_t *const saved_buffer = I_VideoBuffer;
    const int step = 1 << crispy->hires;
    const int top = SCREENHEIGHT - (BAR_HEIGHT << crispy->hires);
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
    ST_DrawExternal();
    I_VideoBuffer = saved_buffer;
    V_RestoreBuffer();

    for (row = 0; row < BAR_HEIGHT; ++row)
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

static int touch_x, touch_y;

static void PostInventoryKey(evtype_t type)
{
    event_t ev;

    ev.type = type;
    ev.data1 = key_invuse;
    ev.data2 = 0;
    ev.data3 = 0;
    D_PostEvent(&ev);
}

// The inventory bar's six items (the bar's rows 8-39), as ST_doRefresh
// draws them: a tap on an item chooses it, a tap on the chosen one uses it.
#define ITEMS_X 42
#define ITEM_W 35
#define NUM_ITEM_SLOTS 6

static void UpdateInventoryTouch(int bar_y)
{
    static boolean was_touching = false;
    static int release_in = 0;
    player_t *const player = &players[consoleplayer];
    const boolean touching = N3DS_GetTouch(&touch_x, &touch_y);
    int first, item;

    N3DS_ReserveTouch(0, bar_y, N3DS_BOTTOM_WIDTH, BAR_HEIGHT);

    // The use key is held a few bottom screen frames, for the game to see it.
    if (release_in > 0 && --release_in == 0)
    {
        PostInventoryKey(ev_keyup);
    }

    if (touching && !was_touching && !menuactive
     && touch_y >= bar_y + 8 && touch_y < bar_y + BAR_HEIGHT
     && touch_x >= ITEMS_X && touch_x < ITEMS_X + ITEM_W * NUM_ITEM_SLOTS)
    {
        first = player->inventorycursor >= NUM_ITEM_SLOTS ?
                player->inventorycursor - (NUM_ITEM_SLOTS - 1) : 0;
        item = first + (touch_x - ITEMS_X) / ITEM_W;

        if (item < player->numinventory)
        {
            if (item == player->inventorycursor)
            {
                PostInventoryKey(ev_keydown);
                release_in = 3;
            }
            else
            {
                player->inventorycursor = item;
            }
        }
    }

    was_touching = touching;
}

// The map at row y, with its zoom buttons.
static void DrawMap(pixel_t *screen, int y, int height)
{
    static int gray = -1, black, white;
    const int zoom = N3DS_UpdateMapZoom(y, height);

    if (gray < 0)
    {
        byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);

        gray = V_GetPaletteIndex(playpal, 128, 128, 128);
        black = V_GetPaletteIndex(playpal, 0, 0, 0);
        white = V_GetPaletteIndex(playpal, 255, 255, 255);
        W_ReleaseLumpName("PLAYPAL");
    }

    AM_DrawBottom(screen, y, N3DS_BOTTOM_WIDTH, height, zoom);
    N3DS_DrawMapButtons(screen, y, height, gray, black, white);
}

boolean N3DS_StrifeDrawBottom(pixel_t *screen)
{
    const boolean bar = screenblocks == FULL_VIEW;
    const boolean map = n3ds_bottom_map != 0;
    const int map_height = N3DS_BOTTOM_HEIGHT - (bar ? BAR_HEIGHT : 0);

    // The buttons set it again when they are drawn.
    N3DS_ClearReservedTouch();

    if (gamestate != GS_LEVEL || !gametic || players[displayplayer].mo == NULL
     || (!bar && !map))
    {
        return false;
    }

    if (!map)
    {
        // Top, middle or bottom of the screen.
        memset(screen, 0, N3DS_BOTTOM_WIDTH * N3DS_BOTTOM_HEIGHT * sizeof(*screen));
        DrawStatusBar(screen, n3ds_hud_position * (N3DS_BOTTOM_HEIGHT - BAR_HEIGHT) / 2);
        UpdateInventoryTouch(n3ds_hud_position * (N3DS_BOTTOM_HEIGHT - BAR_HEIGHT) / 2);
    }
    else if (bar && n3ds_hud_position == 2)
    {
        DrawMap(screen, 0, map_height);
        DrawStatusBar(screen, map_height);
        UpdateInventoryTouch(map_height);
    }
    else
    {
        if (bar)
        {
            DrawStatusBar(screen, 0);
            UpdateInventoryTouch(0);
        }

        DrawMap(screen, N3DS_BOTTOM_HEIGHT - map_height, map_height);
    }

    return true;
}

#endif // __3DS__
