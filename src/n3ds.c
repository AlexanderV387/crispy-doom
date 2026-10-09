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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <3ds.h>

#include "n3ds.h"

// libctru's main thread stack is 32 KiB by default: too small for Doom.
u32 __stacksize__ = 1024 * 1024;

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

// The log: each line with the time since startup, written at once (so it
// survives a hang).
static FILE *log_file;
static u64 log_start;

static int LogWrite(void *cookie, const char *buffer, _READ_WRITE_BUFSIZE_TYPE size)
{
    static boolean at_line_start = true;
    int i;

    for (i = 0; i < (int) size; ++i)
    {
        if (at_line_start)
        {
            fprintf(log_file, "%6llu ", (unsigned long long) (osGetTime() - log_start));
            at_line_start = false;
        }

        fputc(buffer[i], log_file);

        if (buffer[i] == '\n')
        {
            at_line_start = true;
        }
    }

    fflush(log_file);
    return (int) size;
}

void N3DS_Init(void)
{
    osSetSpeedupEnable(true);

    mkdir("sdmc:/3ds", 0777);
    mkdir(N3DS_DATA_DIR, 0777);
    chdir(N3DS_DATA_DIR);

    // There is no /tmp for the few temporary files left (M_TempFile).
    setenv("TMPDIR", N3DS_DATA_DIR, 1);

    // The startup messages (and errors) go to a log: there is no console.
    // The old log is blanked instead of emptied (see N3DS_OpenRewrite) and
    // written over from the start. Every line starts with the milliseconds
    // since startup: where startup spends its time. stderr is the same
    // stream (threads inherit it), so their lines do not overwrite each other.
    {
        const char *path = N3DS_DATA_DIR "crispy-doom-log.txt";
        FILE *log = fopen(path, "r+b");

        if (log != NULL)
        {
            long size;

            fseek(log, 0, SEEK_END);
            size = ftell(log);
            rewind(log);

            while (size-- > 0)
            {
                fputc(size % 64 == 0 ? '\n' : ' ', log);
            }

            rewind(log);
        }
        else
        {
            log = fopen(path, "wb");
        }

        if (log != NULL)
        {
            FILE *stream;

            log_file = log;
            log_start = osGetTime();
            stream = funopen(NULL, NULL, LogWrite, NULL, NULL);

            if (stream != NULL)
            {
                setvbuf(stream, NULL, _IOLBF, 0);
                stdout = stream;
                stderr = stream;
            }
        }
    }
}

boolean N3DS_IsClosing(void)
{
    return aptShouldClose();
}

// ==========================================================================
// Frame counter on the bottom screen (320x240, rotated like the top one):
// frames per second, small, in the top right corner. For the tests; it will
// become an option (off by default), next to the map.

#define BOTTOM_WIDTH 320

static void FillBottom(u32 *framebuffer, int x, int y, int w, int h, u32 color)
{
    int i, j;

    for (i = x; i < x + w; ++i)
    {
        for (j = y; j < y + h; ++j)
        {
            framebuffer[i * TOP_HEIGHT + (TOP_HEIGHT - 1 - j)] = color;
        }
    }
}

// 3x5 digits, as in the n3ds-ports hello world.
static void DrawNumber(u32 *framebuffer, int number, int x, int y, int scale,
                       u32 color)
{
    static const byte digits[10][5] =
    {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
        {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1},
        {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
    };
    char text[12];
    int i, row, col;

    snprintf(text, sizeof(text), "%d", number);

    for (i = 0; text[i] != '\0'; ++i)
    {
        for (row = 0; row < 5; ++row)
        {
            for (col = 0; col < 3; ++col)
            {
                if (digits[text[i] - '0'][row] & (4 >> col))
                {
                    FillBottom(framebuffer, x + (i * 4 + col) * scale,
                               y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

// work_us: the frame's time without the vertical blank wait (the game and
// composing the frame): how much of the 16.7 ms of a 60 FPS frame is used.
static void DrawFrameCounter(int work_us)
{
    static int frames = 0, fps = 0, work_ms = 0, buffers_drawn = 0;
    static u64 start = 0, work_total = 0;
    const u64 now = osGetTime();
    u32 *framebuffer;

    ++frames;
    work_total += work_us;

    if (now - start >= 1000)
    {
        fps = (int) (frames * 1000 / (now - start));
        work_ms = (int) ((work_total / frames + 500) / 1000);
        frames = 0;
        work_total = 0;
        start = now;
        buffers_drawn = 0;
    }

    // Both buffers of the bottom screen, once per second.
    if (buffers_drawn >= 2)
    {
        return;
    }

    framebuffer = (u32 *) gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    // Frames per second (white) and milliseconds of work per frame (green).
    FillBottom(framebuffer, BOTTOM_WIDTH - 64, 2, 62, 14, 0x000000FF);
    DrawNumber(framebuffer, fps, BOTTOM_WIDTH - 61, 4, 2, 0xFFFFFFFF);
    DrawNumber(framebuffer, work_ms, BOTTOM_WIDTH - 31, 4, 2, 0x80FF80FF);
    GSPGPU_FlushDataCache(framebuffer, BOTTOM_WIDTH * TOP_HEIGHT * sizeof(u32));
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
    ++buffers_drawn;
}

// Palette in the framebuffer format (RGBA8: 0xRRGGBBAA).
static u32 colors[256];

void N3DS_Present(const pixel_t *buffer, int width, int height,
                  const SDL_Color *palette, boolean palette_changed)
{
    static int rows[TOP_HEIGHT];
    static int rows_height = 0;
    static u64 work_start = 0;
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

    framebuffer = (u32 *) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    // High resolution rendering (twice as wide and high): each screen pixel
    // is the average of 2x2 pixels, the screen's height from its 400 rows.
    if (width > TOP_WIDTH)
    {
        const int screen_width = width / 2 > TOP_WIDTH ? TOP_WIDTH : width / 2;

        left = (TOP_WIDTH - screen_width) / 2;
        memset(framebuffer, 0, TOP_WIDTH * TOP_HEIGHT * sizeof(u32));

        for (y = 0; y < TOP_HEIGHT; ++y)
        {
            const int source_y = y * height / TOP_HEIGHT;
            const pixel_t *row0 = buffer + source_y * width;
            const pixel_t *row1 = source_y + 1 < height ? row0 + width : row0;
            u32 *pixel = framebuffer + left * TOP_HEIGHT + (TOP_HEIGHT - 1 - y);

            for (x = 0; x < screen_width; ++x, pixel += TOP_HEIGHT)
            {
                // A quarter of each color (the 2 lowest bits are dropped).
                *pixel = ((colors[row0[2 * x]] >> 2) & 0x3F3F3F3F)
                       + ((colors[row0[2 * x + 1]] >> 2) & 0x3F3F3F3F)
                       + ((colors[row1[2 * x]] >> 2) & 0x3F3F3F3F)
                       + ((colors[row1[2 * x + 1]] >> 2) & 0x3F3F3F3F);
            }
        }

        goto shown;
    }

    // The framebuffer is rotated: screen pixel (x, y) is at
    // [x * 240 + (239 - y)]. Copied 8 columns at a time, so the writes of
    // each source row stay in a few cache lines.
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

shown:
    GSPGPU_FlushDataCache(framebuffer, TOP_WIDTH * TOP_HEIGHT * sizeof(u32));
    gfxScreenSwapBuffers(GFX_TOP, false);
    DrawFrameCounter((int) ((svcGetSystemTick() - work_start) / CPU_TICKS_PER_USEC));
    gspWaitForVBlank();
    work_start = svcGetSystemTick();

    // In the log: the first frames reach the screen.
    {
        static int frames = 0;

        if (++frames == 1 || frames == 300)
        {
            printf("N3DS_Present: frame %d (%dx%d)\n", frames, width, height);
        }
    }
}

FILE *N3DS_OpenRewrite(const char *path)
{
    FILE *file = fopen(path, "r+b");

    return file != NULL ? file : fopen(path, "wb");
}

void N3DS_CloseRewrite(FILE *file)
{
    fflush(file);

    if (ftruncate(fileno(file), ftell(file)) != 0)
    {
        // Complete anyway; only an old longer tail stays.
    }

    fclose(file);
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
