# Fatal Pong (PS5 homebrew)

Two-player pong for jailbroken PS5s, written in C on SDL2: Street Fighter fireballs (hadouken), rifts that swallow the opponent, field
bonuses, calamities (the mole and its molehills, the earthquake and its cracks, the frog rain), pixel-art tennis players picked on the start screen (Agassi, Nadal, Graf, Sharapova; Pop!-figure proportions), a grass court and a
Mortal Kombat-style announcer. Runs as an ELF payload or from the websrv Homebrew Launcher, and natively on Linux (gamepads or keyboard).

## Build

Everything builds inside the `ps5` toolbox (Ubuntu 24.04), which has the PS5 payload SDK at
`/opt/ps5-payload-sdk`, ffmpeg, and a native gcc with SDL2 dev files. The host has no compiler.

```sh
toolbox run -c ps5 make               # target/install/eboot.elf + sce_sys/icon0.png
toolbox run -c ps5 make dist          # target/dist/fatal-pong-$(VERSION).zip and .elf, fatal-pong-installer-$(VERSION).elf, fatal-pong-linux-<arch>-$(VERSION).tar.gz (VERSION ?= 1.0.0)
toolbox run -c ps5 make linux         # target/linux/fatal-pong, native build with the toolbox's gcc and SDL2
toolbox run -c ps5 make run-linux     # build and run it (resizable window, F11 fullscreen)
toolbox run -c ps5 make installer     # target/fatal-pong-installer.elf: the home screen tile installer
toolbox run -c ps5 make install-shortcut  # send the installer to elfldr: adds/updates the home screen tile
toolbox run -c ps5 make screenshots   # assets/screenshots/: {start,pause,win,mole,earthquake,frog-rain,finale}.png and a GIF per scene
toolbox run -c ps5 make install       # FTP upload to /data/homebrew/fatal-pong (PS5_HOST, ftpsrv on 2121)
toolbox run -c ps5 make test          # send the bare ELF to elfldr (port 9021)
toolbox run -c ps5 make clean         # rm -rf target/
```

There are no automated tests. To check a change visually, render it on the host with the real game code
(see `tools/screenshots.c`, or a throwaway program in the same style) and look at the PNG; nothing can be
run on the console from here. Pass compiler commands to `toolbox run -c ps5 <cmd>` directly rather than
through `bash -c '...'`.

## Layout

Sources live in `src/`, grouped by package: `core/` (shared types, input, audio, drawing primitives), `gameplay/` (the match
mechanics), `render/` (player sprites, HUD and screens, the pause overlay) and `installer/` (the separate tile
installer payload). Includes are relative to `src/` (`#include "core/game.h"`, `-Isrc`); a generated `.inc` sits next to
the file that includes it.

| File | Role |
|---|---|
| `src/main.c` | SDL setup (PS5: full-size window, software renderer; Linux: resizable window scaled with `SDL_RenderSetLogicalSize`, F11 fullscreen), controller hot-plug, game states (start screen with the character select, playing, paused, then FINISH / FATALITY / RESULT after the last point), main loop |
| `src/core/input.c` | `read_controls()`: each frame, both players' controls (`PlayerControls`: move, directions, punch, finish, confirm, pause, quit) from their gamepad, half of a shared pad, or the keyboard (P1 WASD/F/G/Space/Esc/Q, P2 arrows/Right Ctrl/Right Shift); `KEY_HINT()` adds the keys to on-screen hints outside the PS5 build |
| `src/core/game.h` | Shared constants, `BonusType`, `Paddle`, `Ball`, `rects_overlap` |
| `src/gameplay/paddle.c` | Movement (speed scale, bonus effects, stun), animation state (`stride`, `moving`, timers) |
| `src/gameplay/ball.c` | Ball physics, paddle hits, rally speed-up (`speed_scale`, on top of `ball_base_speed()`), zig-zag, grunts |
| `src/gameplay/fireball.c` | Special move input (down, forward, Square/R1: hadouken; down, back, Square/R1: rift; a shared pad is split, `PadPart`: left half D-pad/left stick + L1, right half right stick + Square/R1), fireballs, hits on paddle/ball/fireball |
| `src/gameplay/rift.c` | Rift under the opponent: half-second warning, then a caught player sinks, is gone (`vanish_timer`, ball passes), and rises back |
| `src/gameplay/fatality.c` | End-of-match fatality: thrown racket, head explosion, blood particles with gravity, stains, shake |
| `src/gameplay/calamity.c` | Calamities rolled every 10th paddle hit (`paddle_hits` in ball.c): the mole (molehills), the earthquake (tremors that shake the screen and open cracks; a ball that runs into a crack tunnels out of another one) or the frog rain (frogs landing in the way that swallow the ball, also with their tongue from a distance, and spit it out half a second later via `Ball.held` / `hidden`, plus decorative rain drawn in a sky layer over the players); shared obstacle timeline, ball knocks, the mud / rock / slime titles |
| `src/gameplay/bonus.c` | Bonus spawning, pickup, paddle effects, tiles and icons (textures from `bonus_icons.inc`), names/descriptions |
| `src/render/players.c` | Player sprites: builds textures from `player_sprites.inc` for each `PlayerLook`, picks poses, ground shadow, the slow bonus ball and chain (`draw_ball_and_chain`), the full bonus force users (Vader or Luke, `Paddle.force_user`, Force waves from `force_push`), the zig-zag bonus crossed eyes (`CROSSED_EYES` overlay), the grow bonus hats (normal-size player at the bottom of the paddle, `TOP_HAT` / `LADY_HAT` above), the `_SCARED` face (white, wide eyes, raised brows, open mouth, trembling) of a player level with a ghost opponent (`scared_of_ghost` in main.c), names, hair colours |
| `src/render/hud.c` | Grass court texture, scores, dripping blood, mud, rock and slime lettering, start screen with the character select (`draw_player_portrait`), finish-him / finish-her, fatality and win screens |
| `src/render/pause.c` | Options help overlay (move list, bonus legend) and the settings sliders (up/down to pick, left/right to change while paused): ball and player speed (`start_speed_percent` in ball.c, scales the serve and `PADDLE_SPEED`) and calamity chance (`calamity_chance` in calamity.c) |
| `src/core/draw.c` | Shared drawing helpers (`fill_pixel_oval`, used by the rift and the molehills) |
| `src/core/text.c` | 5x7 bitmap font (the SDK has no SDL_ttf) |
| `src/core/particles.c` | Fire trails and explosions |
| `src/core/audio.c` | Embedded WAV clips (announcer, hadouken, racket boomerang, fatality scream, grunts), synthesized hadouken fallback, lightsaber and Force, splat, rift, mole, quake, croak, tongue and spit sounds, software mixer |
| `src/installer/installer.c` | Separate payload (`make installer`): embeds eboot.elf, the icon and `assets/shortcut/` (`param.json`, `launch.html`), writes them to `/user/app/PONG00001/` and registers the home screen tile (`sceAppInstUtilAppInstallTitleDir`, resolved by NID); the tile's deeplink opens `launch.html` through websrv, which starts the game via `/hbldr`, so websrv must be running |
| `tools/gen_sprites.py` | Generates `src/render/player_sprites.inc` (sprites + palette) and the `assets/` previews |
| `tools/gen_bonus_icons.py` | Generates `src/gameplay/bonus_icons.inc` (the eight 16x16 bonus icons + palette, in `BonusType` order) and `assets/bonus_icons.png` |
| `tools/gen_voice.sh` | Generates the announcer clips (`agassi-wins`, `nadal-wins`, `graf-wins`, `sharapova-wins`, `finish-him`, `finish-her`, `fatality` .mp3) with Piper TTS (voice `en_US-ryan-high`) + ffmpeg; pass clip names to regenerate only those |
| `tools/gen_icon.py` | Generates the launcher icon `assets/icon0.png` (a shaded tennis ball over a blood splatter, blood dripping off it, "FATAL PONG" in Anton); run from the project root (needs Pillow) |
| `tools/fonts/` | Anton (SIL Open Font License, `OFL.txt`), the icon's typeface |
| `tools/screenshots.c` | Off-screen promo renders, driven by `make screenshots` |
| `assets/` | Launcher icon, player README shipped in the zip, sprite previews, screenshots |

Root-level `*.mp3` are the sound sources; the Makefile converts them to 48 kHz mono WAV in `target/` and
`src/core/audio.c` embeds them with `.incbin` (`EMBED_DIR` is passed by the Makefile). The `.onnx` voice models,
`tts/` (Piper venv) and `in/SDL2` (an old local copy of the headers) are local tooling, not part of the build;
the build uses the SDK's SDL2 headers.

## Conventions

- Match the surrounding style: 4-space indent, `snake_case`, short comments that explain why, one module per
  game concept with a small header. Header guards are `PONG_<NAME>_H` (a plain `FIREBALL_H` once collided
  with the fireball height constant).
- Every function, static helpers included, has a `//` comment directly above its definition: what it does, plus
  its parameters, return value and side effects (globals it changes, sounds it plays, timers it starts) when the
  name doesn't make them obvious. One or two lines, in the same tone as the existing ones; when you change a
  function, update its comment, and add one if it has none. Header declarations get a trailing comment only when
  the name alone isn't enough for a caller.
- Game logic is per-frame at a fixed 60 FPS (frame cap in `main.c`); timers and speeds are in frames and
  pixels per frame.
- Rendering is SDL's software renderer only (the PS5 port has no GPU driver). Avoid per-pixel work every
  frame: precompute into textures at startup like the court and sprites.
- Screen is 1920x1080.
- Game code reads input only through `PlayerControls` (input.c), never the pads directly, so the keyboard works
  everywhere. PS5-only code (libkernel calls, the software-only renderer, pad-only hints) goes under
  `#ifdef __PROSPERO__` (predefined by prospero-clang); the Linux build is everything else, built with the
  toolbox's native gcc.

## Sprites

- Never edit `player_sprites.inc` or `bonus_icons.inc` by hand: change `tools/gen_sprites.py` /
  `tools/gen_bonus_icons.py` and run it from the project root (needs Pillow). Each rewrites its `.inc` and its
  previews in `assets/`. The bonus icons follow the players' style: material ramps lit from the upper left, a dark
  outline; keep their order in sync with `BonusType` and the good/bad split with `bonus_is_good()`.
- Each player is four stacked 32x80 layers (body, legs, left arm, racket arm), drawn 2x wide and stretched
  to the paddle height. Art faces right; the right-hand player is mirrored. The paddle's collision rect stays
  24 px wide, its front edge on art column `FRONT_COL` (players.c).
- Players are in `PlayerLook` order everywhere: the generator's `PLAYERS`, the arrays in `players.c`, and
  `snd_wins` (audio.c). Vader and Luke (`FORCE_USERS` in the generator, `force_tex` in players.c) are a body, a
  headless body and two front-arm poses (idle, push), drawn `FORCE_SCALE` times in the middle of the lane;
  `FORCE_POINTS` follows their layout. Adding one means a `PLAYERS` entry, a `PlayerLook` value, its row in each `players.c`
  table (`player_name`, `player_hair_color`, `player_is_female`) and a `<name>-wins` clip.
- A shrunk player (`BONUS_SHRINK`) is drawn as a baby: the same layers and poses on a 32x40 canvas
  (`BABY_H`), square pixels at the half-height paddle, previewed in `assets/babies.png`. Its feature points are
  `BABY_POINTS` (players.c); keep them in sync with the baby layout in the generator.
- Pose counts and indices are shared between the generator and `players.c` (`STEP_POSES`, `LEFT_ARM_POSES`,
  `ARM_POSES`, `POSE_CHARGE`, `POSE_THRUST`); keep them in sync.
- `THROW_HAND_Y` (fireball.h) is the hands' height in the thrust pose and sets where fireballs spawn; update
  it if the thrust pose moves.

## PS5 / SDK gotchas

- The DualSense Create/Share button is never reported by the SDK's pad driver. Options pauses; touchpad
  click + Options quits.
- SDL sends `CONTROLLERDEVICEADDED` for controllers already opened at startup, and reopening returns the same
  handle; `open_pad()` guards against one pad filling both player slots.
- Cross presses are edge-detected, and the start screen ignores a Cross still held from launching the app.
- Sounds must be mono S16 at 48 kHz (the device format the mixer assumes).
- `make clean` deletes all of `target/`, including converted WAVs and screenshot frames; sources are untouched.
