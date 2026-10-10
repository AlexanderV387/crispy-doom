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

// The game of this program (set by the build): its folder, its picker and
// its controls.
#if defined(N3DS_GAME_STRIFE)
#define N3DS_GAME "strife"
#define N3DS_TITLE "CRISPY STRIFE 3DS"
#else
#define N3DS_GAME_DOOM
#define N3DS_GAME "doom"
#define N3DS_TITLE "CRISPY DOOM 3DS"
#endif

// Folder of the game data, the configuration and the saved games.
#define N3DS_DATA_DIR "sdmc:/3ds/crispy-" N3DS_GAME "/"

// What SDL's own 3DS main would do (804 MHz on the New 3DS), and the data
// folder becomes the current directory, where the IWADs are looked for.
void N3DS_Init(void);

// Shows the 8-bit frame on the top screen (400x240): stretched from its
// height to 240 rows (Doom's 1.2 pixel aspect ratio) and centered when
// narrower than 400, composed on another core, at the vertical blank; and
// the bottom screen. SDL's software renderer on the 3DS scales and rotates
// with the CPU and ignores vsync.
void N3DS_Present(const pixel_t *buffer, int width, int height,
                  const SDL_Color *palette, boolean palette_changed);

// New 3DS (804 MHz, the third core) or Old 3DS (268 MHz, two cores).
boolean N3DS_IsNew3DS(void);

// Stops the thread that composes the screens (before SDL closes them).
void N3DS_StopPresenting(void);

// Chooses the game (IWAD) and the mod in the data folder, with a menu if
// there is anything to choose, and sets the arguments to play them.
// Returns false to quit.
boolean N3DS_PickGame(int *argc, char ***argv);

// The keyboard on the bottom screen: its keys reach the game as key
// presses. Opened when the game asks for text and with the map's button.
void N3DS_ShowKeyboard(boolean show);

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
extern int n3ds_map_zoom;       // bottom screen map: % of each level's zoom
extern int n3ds_config_version; // for changes to older configurations

void N3DS_BindVariables(void);

// Bottom screen, 8-bit like the game's own screen.
#define N3DS_BOTTOM_WIDTH 320
#define N3DS_BOTTOM_HEIGHT 240

// Where the bottom screen is being touched, if it is.
boolean N3DS_GetTouch(int *x, int *y);

// Touches starting in these rectangles (the bottom screen's own buttons,
// set by the game each time it draws them) do not turn the view.
void N3DS_ClearReservedTouch(void);
void N3DS_ReserveTouch(int x, int y, int w, int h);
boolean N3DS_TouchReserved(int x, int y);

// ==========================================================================
// Controls (i_joystick.c). Each action of the game is a joystick button of
// its own (its bit: its index), on while any of its 3DS buttons is held.

typedef struct
{
    const char *name;   // in the menu
    const char *config; // its configuration variable
    int *joyb;          // the game's joystick button variable, or
    int *key;           // a key it presses (actions with no joystick button)
    int default_keys;   // 3DS buttons (libctru's KEY_*)
} n3ds_action_t;

#if defined(N3DS_GAME_STRIFE)
#define N3DS_NUM_ACTIONS 14
#else
#define N3DS_NUM_ACTIONS 10
#endif
#define N3DS_ACTION_RUN 2

extern const n3ds_action_t n3ds_actions[N3DS_NUM_ACTIONS];
extern int n3ds_bindings[N3DS_NUM_ACTIONS]; // 3DS buttons of each action

// A, B and START as themselves, for the menus (joystick buttons).
#define N3DS_JOY_A 17
#define N3DS_JOY_B 18
#define N3DS_JOY_START 19

enum
{
    N3DS_RUN_HOLD,  // while the button is held
    N3DS_RUN_ONCE,  // from a press until the player stops moving
    N3DS_RUN_STICK, // the Circle Pad's tilt gives the speed up to running
    N3DS_NUM_RUN_MODES
};

extern int n3ds_dual_stick;  // the C-stick turns (otherwise it strafes)
extern int n3ds_run_mode;
extern int n3ds_touch_turn;  // sliding on the bottom screen turns
extern int n3ds_touch_speed; // 1-10

// Set by the game: playing (in a level, with no menu).
extern boolean (*N3DS_InGame)(void);

void N3DS_InitControls(void);
void N3DS_ResetBindings(void);
void N3DS_DescribeBinding(int action, char *text, size_t size);
void N3DS_BindKey(int action, int key);

// A bindable button pressed since the last call, -1 for START, or 0.
int N3DS_TakeNewKey(void);

// The bottom screen map's zoom buttons, in the lower left corner of the map
// (its rows map_y to map_y + map_height): reads the touch screen and
// returns the zoom (n3ds_map_zoom); then draws them over the map, with
// three colors of the game's palette.
int N3DS_UpdateMapZoom(int map_y, int map_height);
void N3DS_DrawMapButtons(pixel_t *screen, int map_y, int map_height,
                         int gray, int black, int white);

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
