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

#ifdef __3DS__

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <3ds.h>

#include "n3ds.h"

// libctru's main thread stack is 32 KiB by default: too small for Doom.
u32 __stacksize__ = 1024 * 1024;

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

void N3DS_Init(void)
{
    osSetSpeedupEnable(true);

    mkdir("sdmc:/3ds", 0777);
    mkdir(N3DS_DATA_DIR, 0777);
    chdir(N3DS_DATA_DIR);

    // The startup messages (and errors) go to a log: there is no console.
    // Line buffered, so it survives a hang.
    if (freopen(N3DS_DATA_DIR "crispy-doom-log.txt", "w", stdout) != NULL)
    {
        setvbuf(stdout, NULL, _IOLBF, 0);
        freopen(N3DS_DATA_DIR "crispy-doom-log.txt", "a", stderr);
        setvbuf(stderr, NULL, _IOLBF, 0);
    }
}

boolean N3DS_IsClosing(void)
{
    return aptShouldClose();
}

// Palette in the framebuffer format (RGBA8: 0xRRGGBBAA).
static u32 colors[256];

void N3DS_Present(const pixel_t *buffer, int width, int height,
                  const SDL_Color *palette, boolean palette_changed)
{
    static int rows[TOP_HEIGHT];
    static int rows_height = 0;
    u32 *framebuffer;
    int left, x, y, k;

    // Closed from the HOME Menu: the screens are no longer ours.
    if (N3DS_IsClosing())
    {
        return;
    }

    if (palette_changed)
    {
        for (k = 0; k < 256; ++k)
        {
            colors[k] = ((u32) palette[k].r << 24) | ((u32) palette[k].g << 16)
                      | ((u32) palette[k].b << 8) | 0xFF;
        }
    }

    // Source row of each screen row.
    if (rows_height != height)
    {
        for (y = 0; y < TOP_HEIGHT; ++y)
        {
            rows[y] = (y * height / TOP_HEIGHT) * width;
        }

        rows_height = height;
    }

    if (width > TOP_WIDTH)
    {
        width = TOP_WIDTH;
    }

    // The framebuffer is rotated: screen pixel (x, y) is at
    // [x * 240 + (239 - y)]. Copied 8 columns at a time, so the writes of
    // each source row stay in a few cache lines.
    framebuffer = (u32 *) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    left = (TOP_WIDTH - width) / 2;

    if (left > 0)
    {
        memset(framebuffer, 0, left * TOP_HEIGHT * sizeof(u32));
        memset(framebuffer + (left + width) * TOP_HEIGHT, 0,
               (TOP_WIDTH - left - width) * TOP_HEIGHT * sizeof(u32));
    }

    for (x = 0; x < width; x += 8)
    {
        const int columns = width - x < 8 ? width - x : 8;
        u32 *column = framebuffer + (left + x) * TOP_HEIGHT + (TOP_HEIGHT - 1);

        for (y = 0; y < TOP_HEIGHT; ++y)
        {
            const pixel_t *source = buffer + rows[y] + x;

            for (k = 0; k < columns; ++k)
            {
                column[k * TOP_HEIGHT - y] = colors[source[k]];
            }
        }
    }

    GSPGPU_FlushDataCache(framebuffer, TOP_WIDTH * TOP_HEIGHT * sizeof(u32));
    gfxScreenSwapBuffers(GFX_TOP, false);
    gspWaitForVBlank();

    // In the log: the first frames reach the screen.
    {
        static int frames = 0;

        if (++frames == 1 || frames == 300)
        {
            printf("N3DS_Present: frame %d (%dx%d)\n", frames, width, height);
        }
    }
}

void N3DS_ShowError(const char *message)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("\n Crispy Doom error:\n\n%s\n\n START: exit\n",
           message != NULL ? message : "");

    while (aptMainLoop())
    {
        hidScanInput();

        if ((hidKeysDown() & KEY_START) != 0)
        {
            break;
        }

        gspWaitForVBlank();
    }

    gfxExit();
}

#endif // __3DS__
