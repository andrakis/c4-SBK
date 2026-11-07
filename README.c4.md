# Super Blit Kart on c4

Charlie Birks's Mode 7 kart racer for the 32blit handheld, rewritten in C for
c4lc, because there is no C++ compiler here. It runs on C4IX, the bare machine and
C4DOS. Build it with c4's Makefile: this repository's `c4.mk`, `c4-images.sh` and
`c4ix.vfs.txt` are its part there (c4's `games/README.md`).

It is a rewrite held to the original frame by frame. `make test-sbk-ref` builds
upstream's own C++ and compares every frame's checksum, and the two are the same
(`ref/README.md` says how, and what that doesn't cover).

## A question for the user: the art

**The art's licence is not settled, so none of it is in this repository.**
`make sbk-data` fetches it from upstream's commit `93b45a0` and checks each file's
SHA-256. This repository has a history of its own rather than upstream's, because
upstream's commits carry the art, and pushing them would publish it. Remote
`upstream` is set, so it can be looked at from here.

What is known, from reading upstream's repository on 2026-11-08:

- `LICENSE` is MIT, Copyright (c) 2021 Charlie Birks, at the top of the repository,
  with no exception for `assets/`, and no separate licence or credit.
- All 36 commits that touch `assets/` are by Charlie Birks. The commit messages read
  as their own work ("Improvements to kart sprite", "Add a logo", "Desert cactus"),
  but a commit doesn't say who drew it.
- `RandomFont3.ttf` says inside: "Copyright (c) 2011 Daft Games. Created with
  FontForge". "Daft" matches the author's handle, Daft-Freak, but nothing says they
  are the same, and this font's licence is stated nowhere.

So it is probably all MIT, as the survey said, but that is a reading, not a
statement. To settle it: ask the author (a GitHub issue would do), or draw our own,
which is small work: `c4/mkassets.py` says what's needed. Until then the game
should not go on a public disk. Nothing has been pushed, and the Pages site hasn't
been touched.

## Playing

`sbk` is on the `PATH` in C4IX and on the games disk under C4DOS (`\GAMES\SBK`). It
takes the display; `sbk -window` plays in the Terminal it was started from. Under
grin it opens a window of grin's.

| key | button | does |
|---|---|---|
| arrows, or WASD | D-pad | steer; move in menus |
| Z, U or Enter | A | accelerate; choose |
| X, I or Backspace | B | brake and reverse; back |
| C, O or Space | X | use your item |
| Esc or 2 | Menu | pause; on the main menu, quit |

A race is three laps against seven of the machine's karts. A time trial is three
laps, against the ghost of your best.

Saves are files in the current directory, one for each slot, named `S<slot>.SAV`;
`-savetest` tries that where you are. Only C4IX has a disk to write on here. The bare
machine and C4DOS keep saves in memory, so a ghost or a best time lasts until the
game ends.

## What is here

`src/` has the game. Each unit has its upstream file's name and keeps its order, so
the two can be read side by side:

| here | upstream |
|---|---|
| `kart.c` | `kart.cpp`, the game's feel |
| `race.c` | `race.cpp`, `minimap.cpp` |
| `track.c` | `track.cpp`, and the SDK's `tilemap.cpp` |
| `sprite3d.c` | `sprite3d.cpp`, `camera.hpp` |
| `states.c` | `main-menu.cpp`, `kart-select.cpp`, `track-select.cpp` |
| `menu.c` | `menu.cpp`, and the SDK's `engine/menu.hpp` |
| `game.c` | `game.cpp`, the SDK's `blit::tick`, and `main` |
| `blit.c` | the SDK's `surface.cpp`, `sprite.cpp`, `blend.cpp`, `primitive.cpp`, `text.cpp`, `color.cpp` |
| `assets.c` | the SDK's `Surface::load` and `load_tmx`, and what `extract_track_info.py` generates |
| `sbkmath.c` | sines and arctangents in float, from fdlibm |

`c4/` has what only c4 needs: `mkassets.py` (the art, as C, the way 32blit's tools make
it), `video.c` and `plat-*.c` (the display, windows, the clock), `record.c` and
`demo.c` (the test's input), and `web-sbk.mjs` (the browser check). `ref/` builds
the original.

Upstream's code is MIT (`LICENSE`), and so is the SDK's, Pimoroni's
(`LICENSE.32blit`). `sbkmath.c` keeps fdlibm's notice.

## What changed from upstream

- **The screen is 32-bit words, not RGB bytes.** Blending rounds exactly as the SDK's
  does.
- **Upstream's PicoSystem paths are gone** (`LORES_RACE`, `PICO_BUILD`), as is the
  joystick, which only ever read as centred on a keyboard.
- **Added:** Esc on the main menu quits, because a program here has to end. Enter,
  Space and Backspace are extra keys.
- **Random numbers** are a small xorshift, seeded from the clock.
- **Two things happen when a frame is drawn rather than at every update:** culling
  and sorting the sprites, and working out the minimap. They use the same numbers
  at the same moment, so the frames are the same.

### Upstream's oddities, kept

These aren't bugs of the rewrite. The test holds them to upstream's behaviour, so
fixing one is a decision to stop matching it:

- The lap check in `kart.c` uses `vel.y` where `vel.z` is meant. Laps count by it.
- Choosing Restart on the end-of-race menu saves the new race's empty time over your
  best. A time of zero then looks like the best.
- After that Restart, the hidden end menu still listens, so letting go of A restarts
  the race again.
- Letting go of A in the same update as pressing Esc chooses the pause menu's
  Continue at once.
- A time trial with no ghost shows "Best: 00:00.00".
- The first frame of a race draws the minimap's first update's worth of nothing, from
  a buffer upstream hasn't filled yet. Here it starts at zero.

## What it costs

The machine's instructions, which are the same on every machine, from
`make bench-sbk` on the bare machine over the demo's 8,158 frames (2026-11-08):

| | rainbow | desert |
|---|---|---|
| frames | 6,287 | 1,186 |
| an update (one every 10 ms) | 38 K | 146 K |
| a frame: mode 7 | 2.31 M | 2.22 M |
| a frame: clearing and sprites | 0.53 M | 1.16 M |
| a frame: the HUD and minimap | 0.75 M | 0.72 M |
| a frame: culling and sorting sprites | 0.06 M | 0.25 M |

Over the whole demo, menus included, a frame averages 3.58 M and the worst is 8.8 M,
a race's first frame. Rainbow's updates include the time trials, with one or two
karts; a race of eight on the desert is the heavier case.

**Mode 7 costs about 2.2 M a frame, not the survey's estimate of under 2 M.** That is
about 40 instructions for each pixel of ground, which is close to what a stack machine
can do. Each pixel is two shifts, an add, two loads and a store, plus the steps. The
rest of a frame is in the table.

In Chrome 159, through the https proxy, C4IX ran at 99 M instructions a second. Racing
the desert on the display that comes to 20 to 24 frames a second, at 4.1 to 4.8 M
instructions a frame with its updates. The menus run at 59, the cap. Rainbow, at
about 3.7 M a frame, should be near 27. The game keeps time by the clock, as upstream
does: a slow frame still gets every update, so it plays at the right speed with fewer
frames.

The first version was 8.3 M a frame and 96 K an update. Every step from there kept all
8,158 frames the same as the original's:

- **libm.** C4IX's `atan2f` was 4,900 instructions and `sinf` and `cosf` 2,000, by way
  of doubles. `sbkmath.c` is float, about 500 each. `sqrtf` is softfp's float helper,
  not a double's.
- **Mode 7.** The map is one byte texture with the tiles' flips done (`track_world`, 1
  MB for rainbow and 4 MB for the desert, made once). A scanline wholly on the map draws
  without a check per pixel, and one partly off it draws only the part on it. Where a
  texel covers several pixels, the run is worked out by division. Fog is a table per
  alpha. Only the scanlines a fallen kart shows through go the slow way.
- **Clearing.** `memcpy` and `memset` are VM opcodes that cost about 45 instructions
  whatever the size, so a fill is a few words written by hand and then doubled. A plain
  loop clearing the screen was 1.8 M a frame.
- **Sprites.** A zoomed sprite's stepping through its source is the same for every row,
  so it's worked out once. A source row's visible runs are found once for all the rows
  that show it.
- **Updates.** Which route segment is nearest, and which objects a kart touches: whole
  numbers rule most of them out before any floats.
- **The rest.** The minimap is blended a run of one colour at a time, and text goes down
  each column, as the font is held.

## The test

`-demo` plays `c4/demo.c`, a recorded stream of buttons, on a clock of its own with a
fixed seed: 25 frames a second, with an update every 10 ms, as the 32blit's engine
has it. For each frame it prints a line with an FNV checksum of all its pixels.

`make test-sbk` holds the c4lc build to a gcc `-m32` build of the same C: on the bare
machine, under C4DOS and in C4IX. `make test-sbk-ref` holds that gcc build to upstream's
C++. So the chain is unbroken from the original to C4IX.

The course (`c4/record.c`, which drove while recording) goes through the intro,
every menu and both karts, then:

- a race on rainbow, to the end, with a pause and whatever items turn up;
- a time trial on rainbow, three laps, saved as a ghost;
- a second, against that ghost, with a pause, a restart and a quit;
- a race on the desert for a while: boost strips, cacti, walls and karts leaving the
  road.

`make sbk-record` makes the course again, and only needs doing if the course or the
game's behaviour changes.

`c4/web-sbk.mjs` plays it in a browser with real keys over CDP: on C4IX's display,
in a C4IX window with `-window`, and in a window of grin's. It is run by hand against
a page served from c4's tree, and is not part of `test-sbk`.

`make bench-sbk` prints the costs above.

## What is not here

- Sound: upstream has none. The survey says it would want some of our own.
- Multiplayer: upstream's menu says "Maybe later : )", and so does this one.
- Time trials are not tested on the desert, only rainbow, because three laps
  there take about four minutes of test. The code is the same.
- The joystick and the second button's `Y`, which the game doesn't read.
