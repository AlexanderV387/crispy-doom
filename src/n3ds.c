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
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <3ds.h>

#include "m_config.h"
#include "m_misc.h"
#include "n3ds.h"

// libctru's main thread stack is 32 KiB by default: too small for Doom.
u32 __stacksize__ = 1024 * 1024;

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

// The log: each line with the time since startup, written at once (so it
// survives a hang).
static FILE *log_file;

// stdout before the log took its place: libctru's text console writes
// there (the game picker, fatal errors).
static FILE *console;

#define ConsolePrintf(...) (fprintf(console, __VA_ARGS__), fflush(console))
#define ConsoleClear() ConsolePrintf("\x1b[2J")
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

static aptHookCookie apt_hook;
static void AptHook(APT_HookType hook, void *param);
static void RestoreBottomBacklight(void);

void N3DS_Init(void)
{
    console = stdout;

    osSetSpeedupEnable(true);

    mkdir("sdmc:/3ds", 0777);
    mkdir(N3DS_DATA_DIR, 0777);
    chdir(N3DS_DATA_DIR);

    aptHook(&apt_hook, AptHook, NULL);
    atexit(RestoreBottomBacklight);

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
                console = stdout;
                stdout = stream;
                stderr = stream;
            }
        }
    }
}

boolean N3DS_IsNew3DS(void)
{
    static int is_new = -1;

    if (is_new < 0)
    {
        bool result = false;

        APT_CheckNew3DS(&result);
        is_new = result;
        printf("N3DS: %s 3DS\n", is_new ? "New" : "Old");
    }

    return is_new;
}

boolean N3DS_IsClosing(void)
{
    return aptShouldClose();
}

// ==========================================================================
// Options

int n3ds_bottom_map = 1;
int n3ds_hud_position = 0;
int n3ds_smooth_scaling = 1;
int n3ds_show_fps = 0;
int n3ds_map_zoom = 150;
int n3ds_config_version = 0;

void N3DS_BindVariables(void)
{
    M_BindIntVariable("n3ds_bottom_map",     &n3ds_bottom_map);
    M_BindIntVariable("n3ds_hud_position",   &n3ds_hud_position);
    M_BindIntVariable("n3ds_smooth_scaling", &n3ds_smooth_scaling);
    M_BindIntVariable("n3ds_show_fps",       &n3ds_show_fps);
    M_BindIntVariable("n3ds_map_zoom",       &n3ds_map_zoom);
    M_BindIntVariable("n3ds_dual_stick",     &n3ds_dual_stick);
    M_BindIntVariable("n3ds_run_mode",       &n3ds_run_mode);
    M_BindIntVariable("n3ds_touch_turn",     &n3ds_touch_turn);
    M_BindIntVariable("n3ds_touch_speed",    &n3ds_touch_speed);

    {
        static const char *const names[N3DS_NUM_ACTIONS] =
        {
            "n3ds_bind_fire", "n3ds_bind_use", "n3ds_bind_run",
            "n3ds_bind_strafe", "n3ds_bind_prevweapon",
            "n3ds_bind_nextweapon", "n3ds_bind_automap", "n3ds_bind_jump",
            "n3ds_bind_strafeleft", "n3ds_bind_straferight",
        };
        int i;

        N3DS_ResetBindings();

        for (i = 0; i < N3DS_NUM_ACTIONS; ++i)
        {
            M_BindIntVariable(names[i], &n3ds_bindings[i]);
        }
    }
    M_BindIntVariable("n3ds_config_version", &n3ds_config_version);
}

// ==========================================================================
// HOME Menu, sleep mode and the bottom screen's backlight.
//
// The gsp::Lcd session is opened only around each change: keeping it open
// froze the console at the HOME Menu (in Wolfenstein), which needs it.

void (*N3DS_SaveOnSuspend)(void);

static boolean config_saved;
static boolean bottom_on = true;

boolean N3DS_ConfigAlreadySaved(void)
{
    return config_saved && N3DS_IsClosing();
}

static void SetBottomBacklight(boolean on)
{
    if (R_FAILED(gspLcdInit()))
    {
        return;
    }

    if (on)
    {
        GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM);
    }
    else
    {
        GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
    }

    gspLcdExit();
}

// Never leave the HOME Menu or sleep mode with the bottom screen off.
// The configuration is saved when the HOME Menu opens: the game may be
// closed from there, and then the SD card is much slower.
static void AptHook(APT_HookType hook, void *param)
{
    switch (hook)
    {
        case APTHOOK_ONSUSPEND:
            if (N3DS_SaveOnSuspend != NULL)
            {
                N3DS_SaveOnSuspend();
                config_saved = true;
            }
            // fall through
        case APTHOOK_ONSLEEP:
        case APTHOOK_ONEXIT:
            if (!bottom_on)
            {
                SetBottomBacklight(true);
            }
            break;

        case APTHOOK_ONRESTORE:
            config_saved = false;
            // fall through
        case APTHOOK_ONWAKEUP:
            if (!bottom_on)
            {
                SetBottomBacklight(false);
            }
            break;

        default:
            break;
    }
}

static void RestoreBottomBacklight(void)
{
    if (!bottom_on)
    {
        SetBottomBacklight(true);
    }
}

// A tap turns the bottom screen off or on, except while it shows the
// status bar or the map (or turns the view); when they appear it turns on
// by itself.
static void UpdateBottomBacklight(boolean showing)
{
    static boolean was_touching = false;
    const boolean touching = (hidKeysHeld() & KEY_TOUCH) != 0;

    // Turning with the touch screen: a tap in the game is not a toggle.
    const boolean turning = n3ds_touch_turn && N3DS_InGame != NULL
                         && N3DS_InGame();

    if (touching && !was_touching && (!bottom_on || !(showing || turning)))
    {
        bottom_on = !bottom_on;
        SetBottomBacklight(bottom_on);
    }
    else if (showing && !bottom_on)
    {
        bottom_on = true;
        SetBottomBacklight(true);
    }

    was_touching = touching;
}

boolean N3DS_GetTouch(int *x, int *y)
{
    touchPosition touch;

    if ((hidKeysHeld() & KEY_TOUCH) == 0)
    {
        return false;
    }

    hidTouchRead(&touch);
    *x = touch.px;
    *y = touch.py;
    return true;
}

static int reserved_x, reserved_y, reserved_w, reserved_h;

void N3DS_ReserveTouch(int x, int y, int w, int h)
{
    reserved_x = x;
    reserved_y = y;
    reserved_w = w;
    reserved_h = h;
}

boolean N3DS_TouchReserved(int x, int y)
{
    return x >= reserved_x && x < reserved_x + reserved_w
        && y >= reserved_y && y < reserved_y + reserved_h;
}

// ==========================================================================
// Bottom screen (320x240, rotated like the top one): what the game draws
// (status bar, map), 30 times per second, and the frame counter.

boolean (*N3DS_DrawBottom)(pixel_t *screen);

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

// Frames per second, the game's time per frame without waiting for the
// screen (how much of the 16.7 ms of a 60 FPS frame it uses), and the time
// composing the screens on the other core. Averaged over each second.
static int counter_fps, counter_work_ms, counter_compose_ms;

static void CountFrame(int work_us, int compose_us)
{
    static int frames = 0;
    static u64 start = 0, work_total = 0, compose_total = 0;
    const u64 now = osGetTime();

    ++frames;
    work_total += work_us;
    compose_total += compose_us;

    if (now - start >= 1000)
    {
        counter_fps = (int) (frames * 1000 / (now - start));
        counter_work_ms = (int) ((work_total / frames + 500) / 1000);
        counter_compose_ms = (int) ((compose_total / frames + 500) / 1000);
        frames = 0;
        work_total = 0;
        compose_total = 0;
        start = now;
    }
}

// In a corner of the map, away from the status bar: frames per second
// (white), milliseconds of the game per frame (green) and of composing the
// screens (yellow).
static void DrawFrameCounter(u32 *framebuffer)
{
    const int y = n3ds_hud_position == 0 ? N3DS_BOTTOM_HEIGHT - 16 : 2;

    FillBottom(framebuffer, N3DS_BOTTOM_WIDTH - 94, y, 92, 14, 0x000000FF);
    DrawNumber(framebuffer, counter_fps, N3DS_BOTTOM_WIDTH - 91, y + 2, 2,
               0xFFFFFFFF);
    DrawNumber(framebuffer, counter_work_ms, N3DS_BOTTOM_WIDTH - 61, y + 2, 2,
               0x80FF80FF);
    DrawNumber(framebuffer, counter_compose_ms, N3DS_BOTTOM_WIDTH - 31, y + 2, 2,
               0xFFE060FF);
}

// ==========================================================================
// Presenting. The game's thread hands each frame (a copy) to a thread on
// another core, which composes both screens, swaps them and waits for the
// vertical blank, while the game makes the next frame. The game waits only
// if that thread has not finished the previous one, so the vertical blank
// still sets the pace. Composing took the game's core ~3 ms of each frame.

typedef struct
{
    pixel_t *top;
    int top_size;
    int width, height;
    u32 colors[256];        // palette in the framebuffer format (RGBA8)
    boolean bottom;         // the bottom screen is updated (every other frame)
    boolean bottom_showing; // and bottom_screen has something
    pixel_t bottom_screen[N3DS_BOTTOM_WIDTH * N3DS_BOTTOM_HEIGHT];
} frame_t;

static frame_t frame;
static LightEvent frame_ready, frame_done;
static Thread present_thread = NULL;
static volatile boolean present_quit = false;
static u64 compose_ticks; // composing the last frame

// The top screen (400x240): the frame stretched from its height to 240
// rows (Doom's 1.2 pixel aspect ratio) and centered when narrower than 400.
static void ComposeTop(const frame_t *f)
{
    static int rows[TOP_HEIGHT];
    static u32 weights[TOP_HEIGHT];
    static int rows_height = 0, rows_width = 0, rows_smooth = -1;
    u32 *framebuffer;
    int left, x, y, k;

    // Source row of each screen row, and how much of the next one to blend.
    if (rows_height != f->height || rows_width != f->width
     || rows_smooth != n3ds_smooth_scaling)
    {
        for (y = 0; y < TOP_HEIGHT; ++y)
        {
            const int position = y * f->height * 256 / TOP_HEIGHT;
            const int row = position / 256;

            rows[y] = row * f->width;
            weights[y] = row + 1 < f->height && n3ds_smooth_scaling ?
                         position % 256 : 0;
        }

        rows_height = f->height;
        rows_width = f->width;
        rows_smooth = n3ds_smooth_scaling;
    }

    framebuffer = (u32 *) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    // High resolution rendering (twice as wide and high): each screen pixel
    // is the average of 2x2 pixels, the screen's height from its 400 rows.
    if (f->width > TOP_WIDTH)
    {
        const int screen_width = f->width / 2 > TOP_WIDTH ? TOP_WIDTH : f->width / 2;

        left = (TOP_WIDTH - screen_width) / 2;
        memset(framebuffer, 0, TOP_WIDTH * TOP_HEIGHT * sizeof(u32));

        for (y = 0; y < TOP_HEIGHT; ++y)
        {
            const int source_y = y * f->height / TOP_HEIGHT;
            const pixel_t *row0 = f->top + source_y * f->width;
            const pixel_t *row1 = source_y + 1 < f->height ? row0 + f->width : row0;
            u32 *pixel = framebuffer + left * TOP_HEIGHT + (TOP_HEIGHT - 1 - y);

            for (x = 0; x < screen_width; ++x, pixel += TOP_HEIGHT)
            {
                // A quarter of each color (the 2 lowest bits are dropped).
                *pixel = ((f->colors[row0[2 * x]] >> 2) & 0x3F3F3F3F)
                       + ((f->colors[row0[2 * x + 1]] >> 2) & 0x3F3F3F3F)
                       + ((f->colors[row1[2 * x]] >> 2) & 0x3F3F3F3F)
                       + ((f->colors[row1[2 * x + 1]] >> 2) & 0x3F3F3F3F);
            }
        }

        goto composed;
    }

    // The framebuffer is rotated: screen pixel (x, y) is at
    // [x * 240 + (239 - y)]. Copied 8 columns at a time, so the writes of
    // each source row stay in a few cache lines. Stretching 200 rows to 240,
    // a screen row between two source rows is a blend of both (weights[y],
    // out of 256): repeating one row in five left visible steps.
    left = (TOP_WIDTH - f->width) / 2;

    if (left > 0)
    {
        memset(framebuffer, 0, left * TOP_HEIGHT * sizeof(u32));
        memset(framebuffer + (left + f->width) * TOP_HEIGHT, 0,
               (TOP_WIDTH - left - f->width) * TOP_HEIGHT * sizeof(u32));
    }

    for (x = 0; x < f->width; x += 8)
    {
        const int columns = f->width - x < 8 ? f->width - x : 8;
        u32 *column = framebuffer + (left + x) * TOP_HEIGHT + (TOP_HEIGHT - 1);

        for (y = 0; y < TOP_HEIGHT; ++y)
        {
            const pixel_t *source = f->top + rows[y] + x;
            const u32 weight = weights[y];

            if (weight == 0)
            {
                for (k = 0; k < columns; ++k)
                {
                    column[k * TOP_HEIGHT - y] = f->colors[source[k]];
                }
            }
            else
            {
                const pixel_t *next = source + f->width;

                for (k = 0; k < columns; ++k)
                {
                    const u32 c0 = f->colors[source[k]];
                    const u32 c1 = f->colors[next[k]];
                    // Two channels at a time in each 32-bit product.
                    const u32 low = (((c0 & 0x00FF00FF) * (256 - weight)
                                    + (c1 & 0x00FF00FF) * weight) >> 8) & 0x00FF00FF;
                    const u32 high = (((c0 >> 8) & 0x00FF00FF) * (256 - weight)
                                    + ((c1 >> 8) & 0x00FF00FF) * weight) & 0xFF00FF00;

                    column[k * TOP_HEIGHT - y] = low | high;
                }
            }
        }
    }

composed:
    GSPGPU_FlushDataCache(framebuffer, TOP_WIDTH * TOP_HEIGHT * sizeof(u32));
    gfxScreenSwapBuffers(GFX_TOP, false);
}

static void ComposeBottom(const frame_t *f)
{
    // Whether each of the two buffers has anything but black.
    static boolean drawn[2] = {true, true};
    static int buffer = 0;
    u32 *framebuffer;
    int x, y, k;

    framebuffer = (u32 *) gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    if (f->bottom_showing)
    {
        // 8 columns at a time, as on the top screen.
        for (x = 0; x < N3DS_BOTTOM_WIDTH; x += 8)
        {
            u32 *column = framebuffer + x * TOP_HEIGHT + (TOP_HEIGHT - 1);

            for (y = 0; y < N3DS_BOTTOM_HEIGHT; ++y)
            {
                const pixel_t *source = f->bottom_screen + y * N3DS_BOTTOM_WIDTH + x;

                for (k = 0; k < 8; ++k)
                {
                    column[k * TOP_HEIGHT - y] = f->colors[source[k]];
                }
            }
        }

        drawn[buffer] = true;
    }
    else if (drawn[buffer])
    {
        memset(framebuffer, 0, N3DS_BOTTOM_WIDTH * TOP_HEIGHT * sizeof(u32));
        drawn[buffer] = false;
    }
    else if (!n3ds_show_fps)
    {
        return; // already black
    }

    if (n3ds_show_fps)
    {
        DrawFrameCounter(framebuffer);
        drawn[buffer] = true;
    }

    GSPGPU_FlushDataCache(framebuffer,
                          N3DS_BOTTOM_WIDTH * TOP_HEIGHT * sizeof(u32));
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
    buffer ^= 1;
}

static void ShowFrame(void)
{
    const u64 start = svcGetSystemTick();

    ComposeTop(&frame);

    if (frame.bottom)
    {
        ComposeBottom(&frame);
    }

    compose_ticks = svcGetSystemTick() - start;
    gspWaitForVBlank();
}

static void PresentThread(void *arg)
{
    for (;;)
    {
        LightEvent_Wait(&frame_ready);

        if (present_quit)
        {
            break;
        }

        // Closed from the HOME Menu: the screens are no longer ours.
        if (!N3DS_IsClosing())
        {
            ShowFrame();
        }

        LightEvent_Signal(&frame_done);
    }
}

// On the system core (1), which the game may use up to 80% of the time;
// without it, everything stays on the game's core. Not on the Old 3DS: its
// system core already has the audio thread (no third core).
static void StartPresenting(void)
{
    s32 priority = 0x30;
    Result result = 0;
    int core, limit;

    LightEvent_Init(&frame_ready, RESET_ONESHOT);
    LightEvent_Init(&frame_done, RESET_STICKY);
    LightEvent_Signal(&frame_done);

    if (!N3DS_IsNew3DS())
    {
        printf("N3DS_Present: composing the screens on the game's core\n");
        return;
    }

    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    // The .3dsx gets 80% (the Homebrew Launcher's limits); a .cia only up
    // to what its exheader allows: the most it accepts.
    for (limit = 80; limit > 0; limit -= 10)
    {
        result = APT_SetAppCpuTimeLimit(limit);

        if (R_SUCCEEDED(result))
        {
            break;
        }
    }
    present_thread = threadCreate(PresentThread, NULL, 32 * 1024, priority,
                                  1, false);
    core = 1;

    // Otherwise the third core, with the audio thread.
    if (present_thread == NULL)
    {
        present_thread = threadCreate(PresentThread, NULL, 32 * 1024, priority,
                                      2, false);
        core = 2;
    }

    // A .cia needs core 1 in its affinity mask (tools/3ds/app.rsf).
    if (present_thread != NULL)
    {
        printf("N3DS_Present: composing the screens on core %d "
               "(core 1 time limit: %d%%, %08lx)\n", core, limit,
               (unsigned long) result);
    }
    else
    {
        printf("N3DS_Present: composing the screens on the game's core "
               "(core 1 time limit: %d%%, %08lx)\n", limit,
               (unsigned long) result);
    }
}

void N3DS_StopPresenting(void)
{
    if (present_thread != NULL)
    {
        LightEvent_Wait(&frame_done);
        present_quit = true;
        LightEvent_Signal(&frame_ready);
        threadJoin(present_thread, U64_MAX);
        threadFree(present_thread);
        present_thread = NULL;
    }
}

void N3DS_Present(const pixel_t *buffer, int width, int height,
                  const SDL_Color *palette, boolean palette_changed)
{
    static boolean started = false;
    static int bottom_frames = 0;
    static boolean showing = false;
    static u64 work_start = 0;
    u64 work_end;
    int k;

    // Closed from the HOME Menu: the screens are no longer ours.
    if (N3DS_IsClosing())
    {
        return;
    }

    if (!started)
    {
        StartPresenting();
        started = true;
    }

    // Wait until the previous frame is on its way to the screen.
    work_end = svcGetSystemTick();

    if (present_thread != NULL)
    {
        LightEvent_Wait(&frame_done);
        LightEvent_Clear(&frame_done);
        CountFrame((int) ((work_end - work_start) / CPU_TICKS_PER_USEC),
                   (int) (compose_ticks / CPU_TICKS_PER_USEC));
        work_start = svcGetSystemTick();
    }

    // The other thread is idle: the frame is free to fill.
    if (frame.top_size < width * height)
    {
        free(frame.top);
        frame.top_size = width * height;
        frame.top = malloc(frame.top_size * sizeof(*frame.top));
    }

    memcpy(frame.top, buffer, width * height * sizeof(*frame.top));
    frame.width = width;
    frame.height = height;

    if (palette_changed)
    {
        for (k = 0; k < 256; ++k)
        {
            frame.colors[k] = ((u32) palette[k].r << 24)
                            | ((u32) palette[k].g << 16)
                            | ((u32) palette[k].b << 8) | 0xFF;
        }
    }

    // The bottom screen, 30 times per second: drawn here, with the game.
    frame.bottom = (++bottom_frames & 1) == 0;

    if (frame.bottom)
    {
        showing = N3DS_DrawBottom != NULL && N3DS_DrawBottom(frame.bottom_screen);
        frame.bottom_showing = showing;
    }

    UpdateBottomBacklight(showing);

    if (present_thread != NULL)
    {
        LightEvent_Signal(&frame_ready);
    }
    else
    {
        ShowFrame();
        CountFrame((int) ((work_end - work_start + compose_ticks) / CPU_TICKS_PER_USEC),
                   (int) (compose_ticks / CPU_TICKS_PER_USEC));
        work_start = svcGetSystemTick();
    }

    // In the log: the first frame reaches the screen.
    {
        static boolean first = true;

        if (first)
        {
            printf("N3DS_Present: first frame (%dx%d)\n", width, height);
            first = false;
        }
    }
}

// ==========================================================================
// Game and mod picker, before the game starts, on the top screen's text
// console: the IWADs in the data folder and the mods (PWADs and DeHackEd
// patches) in its "mods" folder. Nothing to choose: straight into the game.

static const struct { const char *file, *name; } iwad_names[] =
{
    { "doom.wad",      "The Ultimate Doom" },
    { "doom1.wad",     "Doom (shareware)" },
    { "doom2.wad",     "Doom II" },
    { "doom2f.wad",    "Doom II (French)" },
    { "plutonia.wad",  "Final Doom: The Plutonia Experiment" },
    { "tnt.wad",       "Final Doom: TNT: Evilution" },
    { "freedoom1.wad", "Freedoom: Phase 1" },
    { "freedoom2.wad", "Freedoom: Phase 2" },
    { "freedm.wad",    "FreeDM" },
    { "chex.wad",      "Chex Quest" },
    { "hacx.wad",      "HACX" },
    { "rekkrsa.wad",   "REKKR" },
    { "rekkrsl.wad",   "REKKR: Sunken Land" },
};

#define MODS_FOLDER "mods"
#define MAX_MODS 128
#define LAST_GAME_FILE N3DS_DATA_DIR "last-game.txt"
#define VISIBLE_ITEMS 9

static boolean FileExists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

static boolean IsModFile(const char *name)
{
    const char *dot = strrchr(name, '.');

    return dot != NULL && (!strcasecmp(dot, ".wad") || !strcasecmp(dot, ".deh")
                        || !strcasecmp(dot, ".bex"));
}

static int CompareNames(const void *a, const void *b)
{
    return strcasecmp(*(const char *const *) a, *(const char *const *) b);
}

static int FindMods(char **mods)
{
    DIR *dir = opendir(N3DS_DATA_DIR MODS_FOLDER);
    struct dirent *entry;
    int count = 0;

    if (dir == NULL)
    {
        return 0;
    }

    while (count < MAX_MODS && (entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] != '.' && IsModFile(entry->d_name))
        {
            mods[count++] = strdup(entry->d_name);
        }
    }

    closedir(dir);
    qsort(mods, count, sizeof(*mods), CompareNames);
    return count;
}

// After the menu was left with a button that may still be held: wait for
// every button to be released before reading the next one.
static u32 KeysDown(boolean *released)
{
    hidScanInput();

    if (!*released)
    {
        *released = hidKeysHeld() == 0;
        return 0;
    }

    return hidKeysDown();
}

// A list on the top screen. Returns the chosen item, -1 for B (back) and
// -2 for START (quit).
static int ChooseItem(const char *title, const char *const *items, int count,
                      int selected, const char *help)
{
    boolean released = false;
    int drawn = -1, first = 0, i;

    while (aptMainLoop())
    {
        const u32 down = KeysDown(&released);

        if (down & (KEY_DOWN | KEY_CPAD_DOWN))
        {
            selected = (selected + 1) % count;
        }
        if (down & (KEY_UP | KEY_CPAD_UP))
        {
            selected = (selected + count - 1) % count;
        }
        if (down & KEY_A)
        {
            return selected;
        }
        if (down & KEY_B)
        {
            return -1;
        }
        if (down & KEY_START)
        {
            return -2;
        }

        if (selected != drawn)
        {
            if (selected < first)
            {
                first = selected;
            }
            else if (selected >= first + VISIBLE_ITEMS)
            {
                first = selected - VISIBLE_ITEMS + 1;
            }

            ConsoleClear();
            ConsolePrintf("\n  \x1b[31mCRISPY DOOM 3DS\x1b[0m\n\n  %s\n\n", title);
            ConsolePrintf("  %s\n", first > 0 ? "  ..." : "");

            for (i = first; i < count && i < first + VISIBLE_ITEMS; ++i)
            {
                ConsolePrintf(i == selected ? "  \x1b[33m> %.44s\x1b[0m\n\n"
                                     : "    %.44s\n\n", items[i]);
            }

            ConsolePrintf("  %s\n", i < count ? "  ..." : "");
            ConsolePrintf("\n  %s\n", help);
            drawn = selected;
        }

        gspWaitForVBlank();
    }

    return -2;
}

static void ShowNoGame(void)
{
    boolean released = false;

    ConsoleClear();
    ConsolePrintf("\n  \x1b[31mCRISPY DOOM 3DS\x1b[0m\n\n"
           "  No game data found. Copy an IWAD to\n"
           "  " N3DS_DATA_DIR "\n\n"
           "  doom.wad, doom1.wad (shareware), doom2.wad,\n"
           "  plutonia.wad, tnt.wad, freedoom1.wad,\n"
           "  freedoom2.wad...\n\n"
           "  Mods (.wad, .deh) go in\n"
           "  " N3DS_DATA_DIR MODS_FOLDER "/\n\n"
           "  START: exit\n");

    while (aptMainLoop() && (KeysDown(&released) & KEY_START) == 0)
    {
        gspWaitForVBlank();
    }
}

// Quit in the game: this title opens again, with the picker. Not when the
// HOME Menu closes it.
static void BackToPicker(void)
{
    if (!N3DS_IsClosing())
    {
        aptSetChainloaderToSelf();
    }
}

boolean N3DS_PickGame(int *argc, char ***argv)
{
    static char *args[8];
    const char *games[arrlen(iwad_names)];
    const char *game_files[arrlen(iwad_names)];
    char *mods[MAX_MODS + 1];
    char last_game[64] = "", last_mod[256] = "";
    int num_games = 0, num_mods, game = 0, mod = 0, i;
    boolean shown = false;
    FILE *file;

    for (i = 0; i < arrlen(iwad_names); ++i)
    {
        char path[128];

        M_snprintf(path, sizeof(path), "%s%s", N3DS_DATA_DIR, iwad_names[i].file);

        if (FileExists(path))
        {
            games[num_games] = iwad_names[i].name;
            game_files[num_games++] = iwad_names[i].file;
        }
    }

    // The first item is no mod.
    mods[0] = "No mod";
    num_mods = 1 + FindMods(mods + 1);

    // The last choice comes first.
    file = fopen(LAST_GAME_FILE, "r");

    if (file != NULL)
    {
        if (fgets(last_game, sizeof(last_game), file) != NULL)
        {
            last_game[strcspn(last_game, "\r\n")] = '\0';
        }
        if (fgets(last_mod, sizeof(last_mod), file) != NULL)
        {
            last_mod[strcspn(last_mod, "\r\n")] = '\0';
        }
        fclose(file);
    }

    for (i = 0; i < num_games; ++i)
    {
        if (!strcasecmp(game_files[i], last_game))
        {
            game = i;
        }
    }
    for (i = 1; i < num_mods; ++i)
    {
        if (!strcasecmp(mods[i], last_mod))
        {
            mod = i;
        }
    }

    if (num_games == 0 || num_games > 1 || num_mods > 1)
    {
        gfxInitDefault();
        consoleInit(GFX_TOP, NULL);
        shown = true;

        if (num_games == 0)
        {
            ShowNoGame();
            gfxExit();
            return false;
        }

        for (;;)
        {
            if (num_games > 1)
            {
                game = ChooseItem("Choose a game:", games, num_games, game,
                                  "A: choose  START: exit");

                if (game < 0)
                {
                    gfxExit();
                    return false;
                }
            }

            if (num_mods == 1)
            {
                mod = 0;
                break;
            }

            mod = ChooseItem(games[game], (const char *const *) mods, num_mods,
                             mod, num_games > 1 ? "A: start  B: back  START: exit"
                                                : "A: start  START: exit");

            // B goes back to the games (or stays here, with only one).
            if (mod == -2)
            {
                gfxExit();
                return false;
            }
            if (mod >= 0)
            {
                break;
            }

            mod = 0;
        }

        gfxExit(); // SDL sets the screens up again
    }

    file = N3DS_OpenRewrite(LAST_GAME_FILE);

    if (file != NULL)
    {
        fprintf(file, "%s\n%s\n", game_files[game], mod > 0 ? mods[mod] : "");
        N3DS_CloseRewrite(file);
    }

    printf("N3DS_PickGame: %s%s%s\n", game_files[game], mod > 0 ? " + " : "",
           mod > 0 ? mods[mod] : "");

    // The program's name, the game and the mod.
    i = 0;
    args[i++] = (*argv)[0];
    args[i++] = "-iwad";
    args[i++] = (char *) game_files[game];

    if (mod > 0)
    {
        const char *dot = strrchr(mods[mod], '.');
        static char mod_path[300];

        M_snprintf(mod_path, sizeof(mod_path), "%s/%s", MODS_FOLDER, mods[mod]);
        args[i++] = !strcasecmp(dot, ".wad") ? "-file" : "-deh";
        args[i++] = mod_path;
    }

    args[i] = NULL;
    *argc = i;
    *argv = args;

    // Quit goes back to the picker when there was something to pick and
    // this is the installed .cia (relaunching a .3dsx would restart the
    // Homebrew Launcher instead).
    if (shown && !envIsHomebrew())
    {
        atexit(BackToPicker);
    }

    return true;
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

    ConsolePrintf("\n Crispy Doom error:\n\n%s\n\n START: exit\n",
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
