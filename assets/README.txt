PS5 FATAL PONG
==============

Two-player pong with Street Fighter fireballs and field bonuses, for jailbroken PS5s.


INSTALL
-------

Homebrew Launcher (websrv):
  1. Unzip so you have a "fatal-pong" folder containing eboot.elf.
  2. Put that folder in "homebrew" at the root of a USB drive
     (USB drive -> homebrew/fatal-pong/eboot.elf), or upload it over FTP to
     /data/homebrew/fatal-pong/.
  3. Open the Homebrew Launcher and start PS5 Fatal Pong.

Bare payload:
  Send fatal-pong-<version>.elf to your ELF loader (port 9021), for example:
      socat -t 99999999 - TCP:<ps5-ip>:9021 < fatal-pong-<version>.elf

Everything (sounds included) is inside the executable; no other files are needed.


CONTROLS
--------

  Start game         Cross (player 1, on the title screen)
  Move paddle        D-pad up/down or left stick
  Hadouken           Down, Forward, then Square or R1
  Rift               Down, Back, then Square or R1
  Fatality           Winner only: Triangle 3 times within 5 seconds of match point
  Pause / help       Options
  Quit               Touchpad click + Options
  New match          Cross (after a player wins)

With one controller, player 1 uses the left stick and player 2 the right stick.


RULES
-----

  - First to 10 points wins.
  - The ball and both paddles speed up with every hit, until a point is scored.
  - A fireball that hits the other paddle freezes it for 1 second.
  - A fireball that hits the ball sends it toward the opponent.
  - One fireball per player at a time; two fireballs cancel each other out.
  - A rift opens under the opponent's feet. If they haven't stepped off it within half a
    second, they fall through and are gone for 1 second: the ball goes straight past.
  - Every 10th paddle hit has a 26% chance of bringing a calamity, for 20 seconds:
      The mole        Digs 3 to 5 molehills around the court and keeps moving them.
      The earthquake  Every tremor shakes the ground and opens a crack, 3 to 5 at a
                      time, old ones closing as new ones open.
      The frog rain   Frogs fall from the sky. 3 to 5 big ones sit on the court, hopping
                      off as new ones land; the small ones only pass through. A big frog
                      swallows the ball, or catches it from a distance with its tongue,
                      keeps it half a second and spits it out in a random direction.
    A ball that hits a molehill or a crack is knocked off at an angle or bounces back.
  - When the match is won: FINISH HIM! The winner can throw the racket into the
    loser's face with a fatality.


BONUSES
-------

Bonuses appear in the paddle lanes; move onto one to collect it. Effects last
10 seconds and a new bonus replaces the current one.

  Green (good)                      Red (bad)
  GROW     double paddle height     SHRINK   half paddle height
  FULL     paddle fills the screen  GHOST    paddle invisible (still blocks)
  FAST     faster paddle            SLOW     slower paddle
  ZIG-ZAG  balls you hit zig-zag    INVERT   up and down swapped
