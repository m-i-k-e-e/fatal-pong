<p align="center"><img src="assets/icon0.png" alt="Fatal Pong" width="256"></p>

# Fatal Pong

Two-player pong for jailbroken PS5s, with Street Fighter fireballs, rifts that swallow your opponent, field
bonuses, calamities and a Mortal Kombat-style finish. Pick Agassi, Nadal, Graf or Sharapova, pixel-art tennis
players with Pop!-figure proportions, and play on a grass court. It also runs natively on Linux with gamepads
or the keyboard.

Written in C on SDL2. All the graphics and sounds are built into the executable.

## Features

- **Hadouken**: down, forward, Square freezes the other player or sends the ball back at them.
- **Rift**: down, back, Square opens a hole under your opponent. If they don't step off it in time, they
  fall through and the ball goes straight past.
- **Bonuses** in the paddle lanes: grow (with a very tall hat), Vader or Luke holding the whole lane with the
  Force, speed, zig-zag balls, and bad ones like shrinking into a baby, a ghost paddle, a ball and chain or
  inverted controls.
- **Calamities**: the mole digs molehills that knock the ball around, the earthquake opens cracks the ball
  tunnels through, and frogs rain down and swallow the ball.
- **FINISH HIM!** The winner can finish the match with a fatality. An announcer calls it all.
- Ball speed, player speed and calamity chance can be changed in the pause menu.

## Install

Download a release from the [Releases](https://github.com/m-i-k-e-e/fatal-pong/releases) page.

**Homebrew Launcher (websrv)**: unzip `fatal-pong-<version>.zip` and put the `fatal-pong` folder in
`homebrew/` at the root of a USB drive, or upload it over FTP to `/data/homebrew/fatal-pong/`. Then start it
from the Homebrew Launcher.

**Bare payload**: send `fatal-pong-<version>.elf` to your ELF loader:

```sh
socat -t 99999999 - TCP:<ps5-ip>:9021 < fatal-pong-<version>.elf
```

**Home screen tile** (optional): send `fatal-pong-installer-<version>.elf` to your ELF loader once. A Fatal
Pong tile appears on the home screen (possibly in the Media category). The tile starts the game through
websrv, so websrv has to be running. Send a newer installer to update the game.

**Linux**: unpack `fatal-pong-linux-<arch>-<version>.tar.gz` and run `./fatal-pong/fatal-pong`. It only needs
SDL2.

## Controls

| Action | DualSense | Keyboard (player 1 / player 2) |
|---|---|---|
| Choose player | Left/right on the title screen | A/D / arrows |
| Start, confirm | Cross | Space or Enter |
| Move | D-pad or left stick | W/S / Up/Down |
| Hadouken | Down, forward, Square or R1 | S, D, F / Down, Left, Right Ctrl |
| Rift | Down, back, Square or R1 | S, A, F / Down, Right, Right Ctrl |
| Fatality (winner) | Triangle 3 times within 5 s | G / Right Shift |
| Pause, help, settings | Options | Esc |
| Quit | Touchpad click + Options | Q while paused |
| Fullscreen | | F11 |

With one controller, it's split in two: player 1 uses the D-pad or left stick and L1, player 2 the right
stick and Square or R1. Right Ctrl and Right Shift also work as `.` and `/`.

## Rules

- First to 10 points wins. The ball and paddles speed up with every hit until a point is scored.
- One fireball per player at a time; two fireballs cancel each other out.
- Bonuses last 10 seconds; a new one replaces the current one.
- Every 10th paddle hit has a 26% chance of starting a 20-second calamity.

## Building

Builds run in a [toolbox](https://containertoolbx.org/) container named `ps5` (Ubuntu 24.04) with the
[PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) at `/opt/ps5-payload-sdk`, ffmpeg, and gcc with the
SDL2 development files.

```sh
toolbox run -c ps5 make               # target/install/eboot.elf and its icon
toolbox run -c ps5 make linux         # target/linux/fatal-pong
toolbox run -c ps5 make run-linux     # build and play on Linux
toolbox run -c ps5 make installer     # target/fatal-pong-installer.elf
toolbox run -c ps5 make dist          # release zip, payloads and Linux tarball in target/dist/
toolbox run -c ps5 make screenshots   # promo screenshots and sprite previews in target/screenshots/
toolbox run -c ps5 make test          # send the payload to the PS5's ELF loader (PS5_HOST, port 9021)
toolbox run -c ps5 make install       # upload over FTP to /data/homebrew/fatal-pong (ftpsrv on 2121)
```

Set `PS5_HOST` to your console's address and `VERSION` for release file names.

## Project layout

| Path | Contents |
|---|---|
| `src/main.c` | SDL setup, game states, main loop |
| `src/core/` | Shared types, input (gamepads and keyboard), audio mixer, drawing, bitmap font, particles |
| `src/gameplay/` | Ball, paddles, fireballs, rifts, bonuses, calamities, fatality |
| `src/render/` | Player sprites, HUD and screens, pause overlay |
| `src/installer/` | The home screen tile installer payload |
| `sounds/` | Sound sources, converted to WAV and embedded at build time |
| `tools/` | Sprite, bonus icon, launcher icon and announcer voice generators; the screenshot renderer |
| `assets/` | Launcher icon, the player README shipped in releases, the tile's shortcut files |

The sprites and bonus icons are pixel art generated by `tools/gen_sprites.py` and `tools/gen_bonus_icons.py`
(Python with Pillow). The announcer voice is Piper TTS, processed with ffmpeg by `tools/gen_voice.sh`.

## Credits

The launcher icon uses [Anton](https://fonts.google.com/specimen/Anton) (SIL Open Font License).
