Crispy Doom for the New Nintendo 3DS, based on [Crispy Doom](https://github.com/fabiangreffrath/crispy-doom) 7.1: Doom as it was on DOS, in widescreen, at 60 FPS, with the status bar and the automap on the bottom screen.

## Downloads

| File | Use |
|---|---|
| `crispy-doom.cia` | Install with FBI; it appears on the HOME Menu |
| `crispy-doom.3dsx` | Copy to `/3ds/` and open it from the Homebrew Launcher |

Copy the game data (IWADs) to `/3ds/crispy-doom/` (game data is not included: use your own copy, the free shareware `doom1.wad` or [Freedoom](https://freedoom.github.io/)):

| Game | File |
|---|---|
| The Ultimate Doom | `doom.wad` |
| Doom shareware (free) | `doom1.wad` |
| Doom II | `doom2.wad` |
| Final Doom | `plutonia.wad`, `tnt.wad` |
| Freedoom (free) | `freedoom1.wad`, `freedoom2.wad`, `freedm.wad` |
| Also | `chex.wad`, `hacx.wad`, `rekkrsa.wad` |

Mods (`.wad`, `.deh`, `.bex`) go in `/3ds/crispy-doom/mods/`. With more than one game or any mod, a menu chooses them at startup and remembers the last choice; in the `.cia`, Quit goes back to it. Crispy Doom plays vanilla and limit-removing mods; Boom/MBF mods need another port.

## Features

- **60 FPS** with the whole top screen (400x240) for the view, uncapped with interpolation. The screens are composed on the system core, so the game keeps its own.
- **Bottom screen:** status bar and automap (only what you have seen, centered on you), with **+** / **-** zoom buttons. Status bar above or below the map; with a smaller view the bar goes back to the top screen and the map fills the bottom one. A tap turns the bottom screen off when it shows nothing.
- **Controls:** Circle Pad moves and strafes, C-stick turns (or classic: the Circle Pad turns and the C-stick strafes); with "Allow Free Look", the C-stick also looks up and down. R / ZR fire, A uses, B / ZL run, L strafes, X / Y change weapons, SELECT opens the automap. Every action can be rebound (Crispness → Buttons), with several buttons each.
- **Run:** hold the button, press it once (until you stop), or tilt the Circle Pad all the way.
- **Touch turning:** slide on the bottom screen to turn (optional, with its own speed); the map's zoom buttons still work.
- **Menus:** A accepts, B goes back (and closes the main menu), START opens and closes the menu.
- **Nintendo 3DS options** (Options → Crispness, first page): automap, status bar position, frame counter, smooth scaling, controls.
- Starts in about 1.3 seconds; the settings are saved when you press HOME, so closing from there is instant; saves never stall on the SD card.
- Old 3DS: detected (no high resolution, music at 22050 Hz), but untested.

Port by AlexanderV387 for [n3ds-ports](https://github.com/AlexanderV387/n3ds-ports). Crispy Doom by Fabian Greffrath and contributors, based on Chocolate Doom; GPL-2.0.
