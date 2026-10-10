//
// Copyright(C) 2005-2014 Simon Howard
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
//       SDL Joystick code.
//


#include "SDL.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "doomtype.h"
#include "d_event.h"
#include "i_joystick.h"
#include "i_system.h"

#include "m_config.h"
#include "m_fixed.h"
#include "m_misc.h"
#ifdef __3DS__
#include "crispy.h"
#include "doomkeys.h"
#endif

static SDL_GameController *gamepad = NULL;
static SDL_Joystick *joystick = NULL;

// Configuration variables:

// Standard default.cfg Joystick enable/disable

#ifdef __3DS__
// The 3DS buttons and sticks, through SDL's game controller mapping.
static int usejoystick = 1;
static int use_gamepad = 1;
#else
static int usejoystick = 0;

// Use SDL_gamecontroller interface for the selected device
static int use_gamepad = 0;
#endif

// SDL_GameControllerType of gamepad
static int gamepad_type = 0;

// SDL GUID and index of the joystick to use.
static char *joystick_guid = "";
static int joystick_index = -1;

// Which joystick axis to use for horizontal movement, and whether to
// invert the direction:

#ifdef __3DS__
// Dual stick: the Circle Pad moves and strafes, the C-stick turns.
static int joystick_x_axis = SDL_CONTROLLER_AXIS_RIGHTX;
#else
static int joystick_x_axis = 0;
#endif
static int joystick_x_invert = 0;

// Which joystick axis to use for vertical movement, and whether to
// invert the direction:

static int joystick_y_axis = 1;
static int joystick_y_invert = 0;

// Which joystick axis to use for strafing?

#ifdef __3DS__
static int joystick_strafe_axis = SDL_CONTROLLER_AXIS_LEFTX;
#else
static int joystick_strafe_axis = -1;
#endif
static int joystick_strafe_invert = 0;

// Which joystick axis to use for looking?

static int joystick_look_axis = -1;
static int joystick_look_invert = 0;

// Configurable dead zone for each axis, specified as a percentage of the axis
// max value.
#ifdef __3DS__
// The Circle Pad and the C-stick rest close to the center.
static int joystick_x_dead_zone = 15;
static int joystick_y_dead_zone = 15;
static int joystick_strafe_dead_zone = 15;
#else
static int joystick_x_dead_zone = 33;
static int joystick_y_dead_zone = 33;
static int joystick_strafe_dead_zone = 33;
#endif
static int joystick_look_dead_zone = 33;

int use_analog = 1; // [crispy] use analog by default

int joystick_turn_sensitivity = 10;
int joystick_move_sensitivity = 10;
int joystick_look_sensitivity = 10;

// Virtual to physical button joystick button mapping. By default this
// is a straight mapping.
static int joystick_physical_buttons[NUM_VIRTUAL_BUTTONS] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

void I_ShutdownGamepad(void)
{
    if (gamepad != NULL)
    {
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
}

static int FindFirstGamepad(void)
{
    int i;
    int gamepadindex = -1;

    for (i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            gamepadindex = i;
            break;
        }
    }

    return gamepadindex;
}

static int FindSpecificGamepad(SDL_JoystickGUID guid)
{
    SDL_JoystickGUID dev_guid;
    int i;
    int gamepadindex = -1;

    for (i = 0; i < SDL_NumJoysticks(); ++i)
    {
        dev_guid = SDL_JoystickGetDeviceGUID(i);
        if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
        {
            gamepadindex = i;
            break;
        }
    }

    return gamepadindex;
}

static int DeviceIndexGamepad(void)
{
    SDL_JoystickGUID guid, dev_guid;
    int index = -1;

    if (strcmp(joystick_guid, ""))
    {
        guid = SDL_JoystickGetGUIDFromString(joystick_guid);

        // First, look for the gamepad at the previously-used index.
        if (joystick_index >= 0 && joystick_index < SDL_NumJoysticks())
        {
            dev_guid = SDL_JoystickGetDeviceGUID(joystick_index);
            if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
            {
                return joystick_index;
            }
        }

        // Maybe the index has moved?
        index = FindSpecificGamepad(guid);
    }

    // If the previous gamepad isn't present, see if a different one is
    // available.
    if (index < 0)
    {
        index = FindFirstGamepad();
    }

    return index;
}

void I_InitGamepad(void)
{
    SDL_JoystickGUID guid;
    int index;

    if (!use_gamepad)
    {
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
    {
        return;
    }

    index = DeviceIndexGamepad();

    if (index < 0)
    {
        printf("I_InitGamepad: No gamepad found.\n");
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        return;
    }

    gamepad = SDL_GameControllerOpen(index);

    if (gamepad == NULL)
    {
        printf("I_InitGamepad: Failed to open gamepad: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        return;
    }

    joystick_index = index;
    gamepad_type = SDL_GameControllerTypeForIndex(index);

    // Always a buffer of its own: the default is the literal "", which the
    // GUID string was written into (a crash where literals are read-only).
    joystick_guid = malloc(GUID_STRING_BUF_SIZE);

    guid = SDL_JoystickGetDeviceGUID(joystick_index);
    SDL_JoystickGetGUIDString(guid, joystick_guid, GUID_STRING_BUF_SIZE);

    // GameController events do not fire if Joystick events are disabled.
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);

    printf("I_InitGamepad: %s\n", SDL_GameControllerName(gamepad));
    I_AtExit(I_ShutdownGamepad, true);
}

static int GetTriggerStateGamepad(SDL_GameControllerAxis trigger)
{
    return (SDL_GameControllerGetAxis(gamepad, trigger) > TRIGGER_THRESHOLD);
}

// Get the state of the given virtual button.

static int ReadButtonStateGamepad(int vbutton)
{
    int physbutton, state;

    // Map from virtual button to physical (SDL) button.
    if (vbutton < NUM_VIRTUAL_BUTTONS)
    {
        physbutton = joystick_physical_buttons[vbutton];
    }
    else
    {
        physbutton = vbutton;
    }

    switch (physbutton)
    {
        case GAMEPAD_BUTTON_TRIGGERLEFT:
            state = GetTriggerStateGamepad(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            break;

        case GAMEPAD_BUTTON_TRIGGERRIGHT:
            state = GetTriggerStateGamepad(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            break;

        default:
            state = SDL_GameControllerGetButton(gamepad, physbutton);
            break;
    }

    return state;
}

// Get a bitmask of all currently-pressed buttons

static int GetButtonsStateGamepad(void)
{
    int i;
    int result = 0;

    for (i = 0; i < MAX_VIRTUAL_BUTTONS; ++i)
    {
        if (ReadButtonStateGamepad(i))
        {
            result |= 1u << i;
        }
    }

    return result;
}

// Read the state of an axis, inverting if necessary.

static int GetAxisStateGamepad(const int *axis_values, int axis, int invert,
                               int dead_zone)
{
    int result;

    // Axis -1 means disabled.

    if (axis < 0)
    {
        return 0;
    }

    // Dead zone is expressed as percentage of axis max value
    dead_zone = 32768 * dead_zone / 100;

    result = axis_values[axis];

    if (result < dead_zone && result > -dead_zone)
    {
        return 0;
    }

    if (invert)
    {
        result = -result;
    }

    result *= FRACUNIT / 32768; // Want FXP number between -1 and 1

    return result;
}

#define DIRECTION_DEADZONE (50 * 32768 / 100)

static int GetDirectionalInputGamepad(const int *axis_values)
{
    int value;
    int dpad = JOY_DIR_NONE;
    int leftstick = JOY_DIR_NONE;
    int rightstick = JOY_DIR_NONE;

    // Dpad.
    if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP))
    {
        dpad |=  JOY_DIR_UP;
    }

    if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
    {
        dpad |=  JOY_DIR_DOWN;
    }

    if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
    {
        dpad |=  JOY_DIR_LEFT;
    }

    if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
    {
        dpad |= JOY_DIR_RIGHT;
    }

    // Left stick.
    value = axis_values[SDL_CONTROLLER_AXIS_LEFTY];

    if (value > DIRECTION_DEADZONE || value < -DIRECTION_DEADZONE)
    {
        if (value > 0)
        {
            leftstick |= JOY_DIR_DOWN;
        }
        else
        {
            leftstick |= JOY_DIR_UP;
        }
    }

    value = axis_values[SDL_CONTROLLER_AXIS_LEFTX];

    if (value > DIRECTION_DEADZONE || value < -DIRECTION_DEADZONE)
    {
        if (value > 0)
        {
            leftstick |= JOY_DIR_RIGHT;
        }
        else
        {
            leftstick |= JOY_DIR_LEFT;
        }
    }

    // Right stick.
    value = axis_values[SDL_CONTROLLER_AXIS_RIGHTX];

    if (value > DIRECTION_DEADZONE || value < -DIRECTION_DEADZONE)
    {
        if (value > 0)
        {
            rightstick |= JOY_DIR_RIGHT;
        }
        else
        {
            rightstick |= JOY_DIR_LEFT;
        }
    }

    value = axis_values[SDL_CONTROLLER_AXIS_RIGHTY];

    if (value > DIRECTION_DEADZONE || value < -DIRECTION_DEADZONE)
    {
        if (value > 0)
        {
            rightstick |= JOY_DIR_DOWN;
        }
        else
        {
            rightstick |= JOY_DIR_UP;
        }
    }

    return ((dpad << DPAD_SHIFT) | (leftstick << LSTICK_SHIFT) |
            (rightstick << RSTICK_SHIFT));
}

void I_UpdateGamepad(void)
{
    if (gamepad != NULL)
    {
        event_t ev;
        int axis_values[SDL_CONTROLLER_AXIS_MAX];

        axis_values[SDL_CONTROLLER_AXIS_LEFTX] =
            SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX);

        axis_values[SDL_CONTROLLER_AXIS_LEFTY] =
            SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTY);

        axis_values[SDL_CONTROLLER_AXIS_RIGHTX] =
            SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_RIGHTX);

        axis_values[SDL_CONTROLLER_AXIS_RIGHTY] =
            SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_RIGHTY);

        ev.type = ev_joystick;
        ev.data1 = GetButtonsStateGamepad();
        ev.data2 = GetAxisStateGamepad(axis_values, joystick_x_axis,
                                       joystick_x_invert, joystick_x_dead_zone);
        ev.data3 = GetAxisStateGamepad(axis_values, joystick_y_axis,
                                       joystick_y_invert, joystick_y_dead_zone);
        ev.data4 = GetAxisStateGamepad(axis_values, joystick_strafe_axis,
                                       joystick_strafe_invert,
                                       joystick_strafe_dead_zone);
        ev.data5 = GetAxisStateGamepad(axis_values, joystick_look_axis,
                                       joystick_look_invert,
                                       joystick_look_dead_zone);
        ev.data6 = GetDirectionalInputGamepad(axis_values);

        D_PostEvent(&ev);
    }
}

#ifdef __3DS__
// The 3DS buttons and sticks, read directly with libctru (SDL's event pump
// calls hidScanInput). Each action of the game is a joystick button of its
// own (its bit: its index in n3ds_actions), on while any of its 3DS buttons
// is held; A, B and START are also there as themselves, for the menus. The
// axes use SDL's game controller numbering. The D-pad is the arrow keys,
// which move in the game and in the menus.

#include <3ds.h>

#include "m_controls.h"
#include "n3ds.h"

// The project's FPS standard: R fires, A uses, B runs, L strafes, X / Y
// change weapons, SELECT shows the map. The first three are the same in
// every game (N3DS_ACTION_RUN).
const n3ds_action_t n3ds_actions[N3DS_NUM_ACTIONS] =
{
#if defined(N3DS_GAME_STRIFE)
    // Strife's inventory takes Y (use the item) and ZL / ZR (choose it).
    { "Fire",            "n3ds_bind_fire",        &joybfire,        NULL, KEY_R },
    { "Use / Talk",      "n3ds_bind_use",         &joybuse,         NULL, KEY_A },
    { "Run",             "n3ds_bind_run",         &joybspeed,       NULL, KEY_B },
    { "Strafe",          "n3ds_bind_strafe",      &joybstrafe,      NULL, KEY_L },
    { "Previous Weapon", "n3ds_bind_prevweapon",  &joybprevweapon,  NULL, 0 },
    { "Next Weapon",     "n3ds_bind_nextweapon",  &joybnextweapon,  NULL, KEY_X },
    { "Automap",         "n3ds_bind_automap",     &joybautomap,     NULL, KEY_SELECT },
    { "Jump",            "n3ds_bind_jump",        &joybjump,        NULL, 0 },
    { "Strafe Left",     "n3ds_bind_strafeleft",  &joybstrafeleft,  NULL, 0 },
    { "Strafe Right",    "n3ds_bind_straferight", &joybstraferight, NULL, 0 },
    { "Use Item",        "n3ds_bind_useitem",     &joybuseartifact, NULL, KEY_Y },
    { "Previous Item",   "n3ds_bind_previtem",    &joybinvleft,     NULL, KEY_ZL },
    { "Next Item",       "n3ds_bind_nextitem",    &joybinvright,    NULL, KEY_ZR },
    { "Mission",         "n3ds_bind_mission",     NULL,             &key_mission, 0 },
#else
    { "Fire",            "n3ds_bind_fire",        &joybfire,        NULL, KEY_R | KEY_ZR },
    { "Use / Open",      "n3ds_bind_use",         &joybuse,         NULL, KEY_A },
    { "Run",             "n3ds_bind_run",         &joybspeed,       NULL, KEY_B | KEY_ZL },
    { "Strafe",          "n3ds_bind_strafe",      &joybstrafe,      NULL, KEY_L },
    { "Previous Weapon", "n3ds_bind_prevweapon",  &joybprevweapon,  NULL, KEY_Y },
    { "Next Weapon",     "n3ds_bind_nextweapon",  &joybnextweapon,  NULL, KEY_X },
    { "Automap",         "n3ds_bind_automap",     &joybautomap,     NULL, KEY_SELECT },
    { "Jump",            "n3ds_bind_jump",        &joybjump,        NULL, 0 },
    { "Strafe Left",     "n3ds_bind_strafeleft",  &joybstrafeleft,  NULL, 0 },
    { "Strafe Right",    "n3ds_bind_straferight", &joybstraferight, NULL, 0 },
#endif
};

int n3ds_bindings[N3DS_NUM_ACTIONS];
int n3ds_dual_stick = 1;
int n3ds_run_mode = N3DS_RUN_HOLD;
int n3ds_touch_turn = 0;
int n3ds_touch_speed = 5;
int n3ds_touch_look = 1;

boolean (*N3DS_InGame)(void);

// The buttons that can be bound, in the order of their names.
static const struct { int key; const char *name; } bindable[] =
{
    { KEY_A, "A" }, { KEY_B, "B" }, { KEY_X, "X" }, { KEY_Y, "Y" },
    { KEY_L, "L" }, { KEY_R, "R" }, { KEY_ZL, "ZL" }, { KEY_ZR, "ZR" },
    { KEY_SELECT, "SELECT" },
};

#define BINDABLE_KEYS (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R \
                     | KEY_ZL | KEY_ZR | KEY_SELECT)

void N3DS_ResetBindings(void)
{
    int i;

    for (i = 0; i < N3DS_NUM_ACTIONS; ++i)
    {
        n3ds_bindings[i] = n3ds_actions[i].default_keys;
    }
}

void N3DS_DescribeBinding(int action, char *text, size_t size)
{
    int i;

    text[0] = '\0';

    for (i = 0; i < arrlen(bindable); ++i)
    {
        if ((n3ds_bindings[action] & bindable[i].key) != 0)
        {
            if (text[0] != '\0')
            {
                M_StringConcat(text, " ", size);
            }

            M_StringConcat(text, bindable[i].name, size);
        }
    }

    if (text[0] == '\0')
    {
        M_StringCopy(text, "-", size);
    }
}

// A button belongs to one action only; pressing one the action already has
// takes it away.
void N3DS_BindKey(int action, int key)
{
    int i;

    if ((n3ds_bindings[action] & key) != 0)
    {
        n3ds_bindings[action] &= ~key;
        return;
    }

    for (i = 0; i < N3DS_NUM_ACTIONS; ++i)
    {
        n3ds_bindings[i] &= ~key;
    }

    n3ds_bindings[action] |= key;
}

// Buttons pressed since the last call: one bindable button, -1 for START,
// or 0.
static u32 keys_pressed;

int N3DS_TakeNewKey(void)
{
    const u32 pressed = keys_pressed;
    int i;

    keys_pressed = 0;

    if ((pressed & KEY_START) != 0)
    {
        return -1;
    }

    for (i = 0; i < arrlen(bindable); ++i)
    {
        if ((pressed & bindable[i].key) != 0)
        {
            return bindable[i].key;
        }
    }

    return 0;
}

// After the configuration is loaded: the actions' joystick buttons are
// fixed (older configurations saved others), and so are the axes.
void N3DS_InitControls(void)
{
    static int *const other_buttons[] =
    {
        &joybuseartifact, &joybinvleft, &joybinvright,
        &joybflyup, &joybflydown, &joybflycenter,
    };
    int i;

    for (i = 0; i < arrlen(other_buttons); ++i)
    {
        *other_buttons[i] = -1;
    }

    for (i = 0; i < N3DS_NUM_ACTIONS; ++i)
    {
        if (n3ds_actions[i].joyb != NULL)
        {
            *n3ds_actions[i].joyb = i;
        }

        n3ds_bindings[i] &= BINDABLE_KEYS;
    }

    joybmenu = N3DS_JOY_START;
    joystick_y_axis = SDL_CONTROLLER_AXIS_LEFTY;
    // Up and down on the C-stick look, with "Allow Free Look" on.
    joystick_look_axis = SDL_CONTROLLER_AXIS_RIGHTY;
    n3ds_run_mode = BETWEEN(0, N3DS_NUM_RUN_MODES - 1, n3ds_run_mode);
    n3ds_touch_speed = BETWEEN(1, 10, n3ds_touch_speed);
}

static int N3DS_AxisValue(int value)
{
    // The Circle Pad and the C-stick reach about 150 at the edge.
    value = value * 32767 / 150;
    return BETWEEN(-32767, 32767, value);
}

static void N3DS_UpdateArrowKeys(u32 held)
{
    static u32 last_held = 0;
    static const struct { u32 key; int doomkey; } arrows[] =
    {
        { KEY_DUP, KEY_UPARROW },
        { KEY_DDOWN, KEY_DOWNARROW },
        { KEY_DLEFT, KEY_LEFTARROW },
        { KEY_DRIGHT, KEY_RIGHTARROW },
    };
    int i;

    for (i = 0; i < arrlen(arrows); ++i)
    {
        const u32 key = arrows[i].key;

        if ((held & key) != (last_held & key))
        {
            event_t ev;

            ev.type = (held & key) != 0 ? ev_keydown : ev_keyup;
            ev.data1 = arrows[i].doomkey;
            ev.data2 = 0;
            ev.data3 = 0;
            D_PostEvent(&ev);
        }
    }

    last_held = held;
}

// Turning by sliding a finger on the bottom screen, as a mouse: the engine
// turns by the whole slide every frame, with no cap per frame; up and down
// look up and down (the games use it as their mouse look). Only in the game,
// and not from the bottom screen's own buttons.
static void N3DS_UpdateTouchTurn(void)
{
    static boolean sliding = false;
    static int last_x, last_y;
    int x, y;

    if (!N3DS_GetTouch(&x, &y))
    {
        sliding = false;
        return;
    }

    if (!sliding)
    {
        sliding = n3ds_touch_turn && N3DS_InGame != NULL && N3DS_InGame()
               && !N3DS_TouchReserved(x, y);
        last_x = x;
        last_y = y;
        return;
    }

    if (x != last_x || (n3ds_touch_look && y != last_y))
    {
        event_t ev;

        ev.type = ev_mouse;
        ev.data1 = 0;
        ev.data2 = (x - last_x) * n3ds_touch_speed;
        ev.data3 = n3ds_touch_look ? (last_y - y) * n3ds_touch_speed : 0;
        D_PostEvent(&ev);
        last_x = x;
        last_y = y;
    }
}

static void N3DS_UpdateJoystick(void)
{
    static u32 last_held = 0;
    static boolean running = false;
    const u32 held = hidKeysHeld();
    const u32 pressed = held & ~last_held;
    circlePosition circle_pad, c_stick;
    int axis_values[SDL_CONTROLLER_AXIS_MAX] = {0};
    boolean moving;
    event_t ev;
    int i;

    last_held = held;
    keys_pressed |= pressed;

    N3DS_UpdateArrowKeys(held);
    N3DS_UpdateTouchTurn();

    hidCircleRead(&circle_pad);
    hidCstickRead(&c_stick);

    // Up is positive on the 3DS and negative for SDL.
    axis_values[SDL_CONTROLLER_AXIS_LEFTX] = N3DS_AxisValue(circle_pad.dx);
    axis_values[SDL_CONTROLLER_AXIS_LEFTY] = N3DS_AxisValue(-circle_pad.dy);
    axis_values[SDL_CONTROLLER_AXIS_RIGHTX] = N3DS_AxisValue(c_stick.dx);
    axis_values[SDL_CONTROLLER_AXIS_RIGHTY] = N3DS_AxisValue(-c_stick.dy);

    // Dual stick: the Circle Pad moves and strafes, the C-stick turns.
    // Classic: the Circle Pad moves and turns, the C-stick strafes.
    joystick_x_axis = n3ds_dual_stick ? SDL_CONTROLLER_AXIS_RIGHTX
                                      : SDL_CONTROLLER_AXIS_LEFTX;
    joystick_strafe_axis = n3ds_dual_stick ? SDL_CONTROLLER_AXIS_LEFTX
                                           : SDL_CONTROLLER_AXIS_RIGHTX;

    ev.type = ev_joystick;
    ev.data1 = 0;

    for (i = 0; i < N3DS_NUM_ACTIONS; ++i)
    {
        const boolean on = (held & n3ds_bindings[i]) != 0;

        if (n3ds_actions[i].key != NULL)
        {
            // Actions with no joystick button press their key.
            static boolean was_on[N3DS_NUM_ACTIONS];

            if (on != was_on[i])
            {
                event_t key_ev;

                key_ev.type = on ? ev_keydown : ev_keyup;
                key_ev.data1 = *n3ds_actions[i].key;
                key_ev.data2 = 0;
                key_ev.data3 = 0;
                D_PostEvent(&key_ev);
                was_on[i] = on;
            }
        }
        else if (on)
        {
            ev.data1 |= 1 << i;
        }
    }

    // Running: while the button is held; from a press until the player
    // stops moving; or with the Circle Pad (or the D-pad) all the way, the
    // tilt giving the speed up to running.
    moving = abs(circle_pad.dx) > 20 || abs(circle_pad.dy) > 20
          || (held & (KEY_DUP | KEY_DDOWN)) != 0;

    if (n3ds_run_mode == N3DS_RUN_ONCE)
    {
        if ((pressed & n3ds_bindings[N3DS_ACTION_RUN]) != 0)
        {
            running = true;
        }
        else if (!moving)
        {
            running = false;
        }

        if (running)
        {
            ev.data1 |= 1 << N3DS_ACTION_RUN;
        }
    }
    else if (n3ds_run_mode == N3DS_RUN_STICK && moving)
    {
        ev.data1 |= 1 << N3DS_ACTION_RUN;
    }

    // The menus' own buttons.
    if ((held & KEY_A) != 0)
    {
        ev.data1 |= 1 << N3DS_JOY_A;
    }
    if ((held & KEY_B) != 0)
    {
        ev.data1 |= 1 << N3DS_JOY_B;
    }
    if ((held & KEY_START) != 0)
    {
        ev.data1 |= 1 << N3DS_JOY_START;
    }

    ev.data2 = GetAxisStateGamepad(axis_values, joystick_x_axis,
                                   joystick_x_invert, joystick_x_dead_zone);
    ev.data3 = GetAxisStateGamepad(axis_values, joystick_y_axis,
                                   joystick_y_invert, joystick_y_dead_zone);
    ev.data4 = GetAxisStateGamepad(axis_values, joystick_strafe_axis,
                                   joystick_strafe_invert,
                                   joystick_strafe_dead_zone);
    ev.data5 = GetAxisStateGamepad(axis_values, joystick_look_axis,
                                   joystick_look_invert,
                                   joystick_look_dead_zone);

    // Menu directions from the Circle Pad only: the D-pad sends arrow keys.
    ev.data6 = 0;

    if (axis_values[SDL_CONTROLLER_AXIS_LEFTY] < -DIRECTION_DEADZONE)
    {
        ev.data6 |= JOY_DIR_UP << LSTICK_SHIFT;
    }
    else if (axis_values[SDL_CONTROLLER_AXIS_LEFTY] > DIRECTION_DEADZONE)
    {
        ev.data6 |= JOY_DIR_DOWN << LSTICK_SHIFT;
    }

    if (axis_values[SDL_CONTROLLER_AXIS_LEFTX] < -DIRECTION_DEADZONE)
    {
        ev.data6 |= JOY_DIR_LEFT << LSTICK_SHIFT;
    }
    else if (axis_values[SDL_CONTROLLER_AXIS_LEFTX] > DIRECTION_DEADZONE)
    {
        ev.data6 |= JOY_DIR_RIGHT << LSTICK_SHIFT;
    }

    D_PostEvent(&ev);
}
#endif

void I_ShutdownJoystick(void)
{
    if (joystick != NULL)
    {
        SDL_JoystickClose(joystick);
        joystick = NULL;
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }
}

static boolean IsValidAxis(int axis)
{
    int num_axes;

    if (axis < 0)
    {
        return true;
    }

    if (IS_BUTTON_AXIS(axis))
    {
        return true;
    }

    if (IS_HAT_AXIS(axis))
    {
        return HAT_AXIS_HAT(axis) < SDL_JoystickNumHats(joystick);
    }

    num_axes = SDL_JoystickNumAxes(joystick);

    return axis < num_axes;
}

static int DeviceIndex(void)
{
    SDL_JoystickGUID guid, dev_guid;
    int i;

    guid = SDL_JoystickGetGUIDFromString(joystick_guid);

    // GUID identifies a class of device rather than a specific device.
    // Check if joystick_index has the expected GUID, as this can act
    // as a tie-breaker in case there are multiple identical devices.
    if (joystick_index >= 0 && joystick_index < SDL_NumJoysticks())
    {
        dev_guid = SDL_JoystickGetDeviceGUID(joystick_index);
        if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
        {
            return joystick_index;
        }
    }

    // Check all devices to look for one with the expected GUID.
    for (i = 0; i < SDL_NumJoysticks(); ++i)
    {
        dev_guid = SDL_JoystickGetDeviceGUID(i);
        if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
        {
            printf("I_InitJoystick: Joystick moved to index %d.\n", i);
            return i;
        }
    }

    // No joystick found with the expected GUID.
    return -1;
}

void I_InitJoystick(void)
{
    int index;

#ifdef __3DS__
    return; // read with libctru (N3DS_UpdateJoystick)
#endif

    if (!usejoystick || !strcmp(joystick_guid, ""))
    {
        return;
    }

    if (use_gamepad)
    {
        I_InitGamepad();
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
    {
        return;
    }

    index = DeviceIndex();

    if (index < 0)
    {
        printf("I_InitJoystick: Couldn't find joystick with GUID \"%s\": "
               "device not found or not connected?\n",
               joystick_guid);
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        return;
    }

    // Open the joystick

    joystick = SDL_JoystickOpen(index);

    if (joystick == NULL)
    {
        printf("I_InitJoystick: Failed to open joystick #%i: %s\n", index,
               SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        return;
    }

    if (!IsValidAxis(joystick_x_axis)
     || !IsValidAxis(joystick_y_axis)
     || !IsValidAxis(joystick_strafe_axis)
     || !IsValidAxis(joystick_look_axis))
    {
        printf("I_InitJoystick: Invalid joystick axis for configured joystick "
               "(run joystick setup again)\n");

        SDL_JoystickClose(joystick);
        joystick = NULL;
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }

    SDL_JoystickEventState(SDL_ENABLE);

    // Initialized okay!

    printf("I_InitJoystick: %s\n", SDL_JoystickName(joystick));

    I_AtExit(I_ShutdownJoystick, true);
}

static boolean IsAxisButton(int physbutton)
{
    if (IS_BUTTON_AXIS(joystick_x_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_x_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_x_axis))
        {
            return true;
        }
    }
    if (IS_BUTTON_AXIS(joystick_y_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_y_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_y_axis))
        {
            return true;
        }
    }
    if (IS_BUTTON_AXIS(joystick_strafe_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_strafe_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_strafe_axis))
        {
            return true;
        }
    }
    if (IS_BUTTON_AXIS(joystick_look_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_look_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_look_axis))
        {
            return true;
        }
    }

    return false;
}

// Get the state of the given virtual button.

static int ReadButtonState(int vbutton)
{
    int physbutton;

    // Map from virtual button to physical (SDL) button.
    if (vbutton < NUM_VIRTUAL_BUTTONS)
    {
        physbutton = joystick_physical_buttons[vbutton];
    }
    else
    {
        physbutton = vbutton;
    }

    // Never read axis buttons as buttons.
    if (IsAxisButton(physbutton))
    {
        return 0;
    }

    return SDL_JoystickGetButton(joystick, physbutton);
}

// Get a bitmask of all currently-pressed buttons

static int GetButtonsState(void)
{
    int i;
    int result = 0;

    for (i = 0; i < MAX_VIRTUAL_BUTTONS; ++i)
    {
        if (ReadButtonState(i))
        {
            result |= 1 << i;
        }
    }

    return result;
}

// Read the state of an axis, inverting if necessary.

static int GetAxisState(int axis, int invert, int dead_zone)
{
    int result;

    // Axis -1 means disabled.

    if (axis < 0)
    {
        return 0;
    }

    // Is this a button axis, or a hat axis?
    // If so, we need to handle it specially.

    result = 0;

    if (IS_BUTTON_AXIS(axis))
    {
        if (SDL_JoystickGetButton(joystick, BUTTON_AXIS_NEG(axis)))
        {
            result -= 32767;
        }
        if (SDL_JoystickGetButton(joystick, BUTTON_AXIS_POS(axis)))
        {
            result += 32767;
        }
    }
    else if (IS_HAT_AXIS(axis))
    {
        int direction = HAT_AXIS_DIRECTION(axis);
        int hatval = SDL_JoystickGetHat(joystick, HAT_AXIS_HAT(axis));

        if (direction == HAT_AXIS_HORIZONTAL)
        {
            if ((hatval & SDL_HAT_LEFT) != 0)
            {
                result -= 32767;
            }
            else if ((hatval & SDL_HAT_RIGHT) != 0)
            {
                result += 32767;
            }
        }
        else if (direction == HAT_AXIS_VERTICAL)
        {
            if ((hatval & SDL_HAT_UP) != 0)
            {
                result -= 32767;
            }
            else if ((hatval & SDL_HAT_DOWN) != 0)
            {
                result += 32767;
            }
        }
    }
    else
    {
        result = SDL_JoystickGetAxis(joystick, axis);

        // Dead zone is expressed as percentage of axis max value
        dead_zone = 32768 * dead_zone / 100;

        if (result < dead_zone && result > -dead_zone)
        {
            result = 0;
        }
    }

    if (invert)
    {
        result = -result;
    }

    result *= FRACUNIT / 32768; // Want FXP number between -1 and 1

    return result;
}

static int GetDirectionalInput(int x, int y)
{
    int retval = JOY_DIR_NONE;

    if (x > 0)
    {
        retval |= JOY_DIR_RIGHT;
    }
    else if (x < 0)
    {
        retval |= JOY_DIR_LEFT;
    }

    if (y > 0)
    {
        retval |= JOY_DIR_DOWN;
    }
    else if (y < 0)
    {
        retval |= JOY_DIR_UP;
    }

    return (retval << DPAD_SHIFT);
}

void I_UpdateJoystick(void)
{
#ifdef __3DS__
    N3DS_UpdateJoystick();
    return;
#endif

    if (use_gamepad)
    {
        I_UpdateGamepad();
        return;
    }

    if (joystick != NULL)
    {
        event_t ev;

        ev.type = ev_joystick;
        ev.data1 = GetButtonsState();
        ev.data2 = GetAxisState(joystick_x_axis, joystick_x_invert,
                                joystick_x_dead_zone);
        ev.data3 = GetAxisState(joystick_y_axis, joystick_y_invert,
                                joystick_y_dead_zone);
        ev.data4 = GetAxisState(joystick_strafe_axis, joystick_strafe_invert,
                                joystick_strafe_dead_zone);
        ev.data5 = GetAxisState(joystick_look_axis, joystick_look_invert,
                                joystick_look_dead_zone);
        ev.data6 = GetDirectionalInput(ev.data2, ev.data3);
        D_PostEvent(&ev);
    }
}

void I_BindJoystickVariables(void)
{
    int i;

    M_BindIntVariable("use_joystick",          &usejoystick);
    M_BindIntVariable("use_gamepad",           &use_gamepad);
    M_BindIntVariable("gamepad_type",          &gamepad_type);
    M_BindStringVariable("joystick_guid",      &joystick_guid);
    M_BindIntVariable("joystick_index",        &joystick_index);
    M_BindIntVariable("joystick_x_axis",       &joystick_x_axis);
    M_BindIntVariable("joystick_y_axis",       &joystick_y_axis);
    M_BindIntVariable("joystick_strafe_axis",  &joystick_strafe_axis);
    M_BindIntVariable("joystick_x_invert",     &joystick_x_invert);
    M_BindIntVariable("joystick_y_invert",     &joystick_y_invert);
    M_BindIntVariable("joystick_strafe_invert",&joystick_strafe_invert);
    M_BindIntVariable("joystick_look_axis",    &joystick_look_axis);
    M_BindIntVariable("joystick_look_invert",  &joystick_look_invert);
    M_BindIntVariable("joystick_x_dead_zone", &joystick_x_dead_zone);
    M_BindIntVariable("joystick_y_dead_zone", &joystick_y_dead_zone);
    M_BindIntVariable("joystick_strafe_dead_zone", &joystick_strafe_dead_zone);
    M_BindIntVariable("joystick_look_dead_zone", &joystick_look_dead_zone);
    M_BindIntVariable("use_analog", &use_analog);
    M_BindIntVariable("joystick_turn_sensitivity", &joystick_turn_sensitivity);
    M_BindIntVariable("joystick_move_sensitivity", &joystick_move_sensitivity);
    M_BindIntVariable("joystick_look_sensitivity", &joystick_look_sensitivity);

    for (i = 0; i < NUM_VIRTUAL_BUTTONS; ++i)
    {
        char name[32];
        M_snprintf(name, sizeof(name), "joystick_physical_button%i", i);
        M_BindIntVariable(name, &joystick_physical_buttons[i]);
    }
}

