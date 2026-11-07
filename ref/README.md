# The original, to hold the rewrite to

`make test-sbk-ref` builds upstream's C++ game, unchanged, with g++ on the host,
and compares every frame of the demo with the rewrite's gcc build.

It takes under a minute and needs no SDL, no cmake and no window, because the
32blit SDK keeps everything it needs from the device behind one table of
functions (`blit::api`). `ref-main.cpp` fills that table and plays the demo:

- the game is upstream's `.cpp` files at the commit `c4.mk` names, with the
  `track-info.cpp` upstream's own `extract_track_info.py` generates;
- the engine and the drawing are the SDK's own `engine/`, `graphics/` and `types/`
  sources, at the commit `c4.mk` names;
- the art comes from `c4/mkassets.py --ref`, in place of the SDK's asset tools,
  which aren't installed here. It writes raw `SPRITERW` images where the tools
  would pack them, which `Surface::load` takes the same way;
- the clock, the random numbers and the saves are `ref-main.cpp`'s: the same
  ten-millisecond ticks and the same generator as `-demo`, and saves in memory.

## What is not the original

Three things differ from upstream's build on a real device, all on purpose, so
that frames can be compared at all:

- **libm.** `sinf`, `cosf`, `atan2f` and the rest come from C4IX's `math.c`
  (`c4/host-math.c`), as in the rewrite's gcc build, because glibc's differ from
  them in the last bit now and then. A 32blit build would use newlib's. So a kart
  on a real device may drift from this by a last bit of rounding, as it would
  between any two builds of upstream's game.
- **Memory starts at zero.** `Minimap` holds an uninitialised array that upstream
  draws from for the first frame of a race, before its first update. `ref-main.cpp`
  replaces `operator new` with `calloc` to make that frame repeatable, and the
  rewrite's array starts at zero to match.
- **Fonts.** `mkassets.py` renders the TTF with FreeType through Pillow rather than
  freetype-py. Both ask FreeType for the same monochrome glyphs, but the fonts
  have not been compared with what the official tools make, because those tools
  are not installed here. The test compares drawing, with the same font bitmaps on
  both sides, not font conversion.

The original is built 64-bit, because g++'s 32-bit libraries aren't on helios; the
rewrite's reference is 32-bit. Floats are SSE on both, so they round the same.
