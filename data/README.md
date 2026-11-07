# Super Blit Kart's art

This directory is ignored by git. `make sbk-data` fills it: the PNGs, the two
`.tmx` tracks, the tilesets, the font and the kart palette, each fetched from
[Daft-Freak/super-blit-kart](https://github.com/Daft-Freak/super-blit-kart)'s
`assets/` at commit `93b45a0689e30215ec68f04baf75af88ec688a45` and checked by
SHA-256 against `c4.mk`.

**The licence on these files is not settled.** So they are not committed here,
and this repository has none of upstream's history, which carries them.
`README.c4.md` has the question and what is known so far.

The game cannot run without these. With none of them here, c4 builds everything
else and `make test-sbk` says it was skipped.
