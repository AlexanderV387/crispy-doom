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
//     the map zoom it.
//

#ifdef __3DS__

#include <stdlib.h>
#include <string.h>

#include "am_map.h"
#include "crispy.h"
#include "doomstat.h"
#include "i_video.h"
#include "n3ds.h"
#include "n3ds_doom.h"
#include "st_stuff.h"
#include "v_video.h"

extern int screenblocks;

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

// Zoom buttons, in the lower left corner of the map: "+" above "-".
#define BUTTON_SIZE 24
#define BUTTON_X 4
#define MIN_ZOOM 25
#define MAX_ZOOM 800

static void FillRect(pixel_t *screen, int x, int y, int w, int h, int color)
{
    int i;

    for (i = y; i < y + h; ++i)
    {
        memset(screen + i * N3DS_BOTTOM_WIDTH + x, color, w * sizeof(*screen));
    }
}

static void DrawZoomButton(pixel_t *screen, int y, boolean plus)
{
    const int center = BUTTON_SIZE / 2;

    FillRect(screen, BUTTON_X, y, BUTTON_SIZE, BUTTON_SIZE, 100);   // gray
    FillRect(screen, BUTTON_X + 2, y + 2, BUTTON_SIZE - 4, BUTTON_SIZE - 4, 0);
    FillRect(screen, BUTTON_X + 5, y + center - 1, BUTTON_SIZE - 10, 2, 4);

    if (plus)
    {
        FillRect(screen, BUTTON_X + center - 1, y + 5, 2, BUTTON_SIZE - 10, 4);
    }
}

// A tap zooms by a quarter; holding the button zooms on, 30 steps a second.
static void UpdateZoom(int plus_y, int minus_y)
{
    static int held = 0;
    int x, y, direction = 0;

    if (N3DS_GetTouch(&x, &y) && x < BUTTON_X + BUTTON_SIZE + 8)
    {
        if (y >= plus_y - 4 && y < plus_y + BUTTON_SIZE + 2)
        {
            direction = 1;
        }
        else if (y >= minus_y - 2 && y < minus_y + BUTTON_SIZE + 4)
        {
            direction = -1;
        }
    }

    if (direction == 0)
    {
        held = 0;
        return;
    }

    if (held == 0 || held >= 10)
    {
        const int step = held == 0 ? 125 : 104;

        if (direction > 0)
        {
            n3ds_map_zoom = n3ds_map_zoom * step / 100;
        }
        else
        {
            n3ds_map_zoom = n3ds_map_zoom * 100 / step;
        }
    }

    n3ds_map_zoom = BETWEEN(MIN_ZOOM, MAX_ZOOM, n3ds_map_zoom);
    ++held;
}

// The map at row y, with its zoom buttons.
static void DrawMap(pixel_t *screen, int y, int height)
{
    const int minus_y = y + height - BUTTON_SIZE - 4;
    const int plus_y = minus_y - BUTTON_SIZE - 4;

    UpdateZoom(plus_y, minus_y);
    AM_DrawBottom(screen, y, N3DS_BOTTOM_WIDTH, height,
                  BETWEEN(MIN_ZOOM, MAX_ZOOM, n3ds_map_zoom));
    DrawZoomButton(screen, plus_y, true);
    DrawZoomButton(screen, minus_y, false);
}

boolean N3DS_DoomDrawBottom(pixel_t *screen)
{
    const boolean bar = screenblocks == FULL_VIEW;
    const boolean map = n3ds_bottom_map != 0;
    const int map_height = N3DS_BOTTOM_HEIGHT - (bar ? ST_HEIGHT : 0);

    if (gamestate != GS_LEVEL || !gametic || players[displayplayer].mo == NULL
     || (!bar && !map))
    {
        return false;
    }

    if (!map)
    {
        // Top, middle or bottom of the screen.
        memset(screen, 0, N3DS_BOTTOM_WIDTH * N3DS_BOTTOM_HEIGHT * sizeof(*screen));
        DrawStatusBar(screen, n3ds_hud_position * (N3DS_BOTTOM_HEIGHT - ST_HEIGHT) / 2);
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

        DrawMap(screen, N3DS_BOTTOM_HEIGHT - map_height, map_height);
    }

    return true;
}

#endif // __3DS__
