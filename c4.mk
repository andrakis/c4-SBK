# c4.mk -- this game's part of c4's Makefile. c4 includes it from
# games/sbk/c4.mk, its submodule, and runs it from c4's root, with c4's
# tools and variables (games/README.md in c4). Without it, c4 builds
# everything else, and test-sbk says it was skipped.

# Super Blit Kart, rewritten in C (games/sbk/README.c4.md): src/ with c4lc,
# the same objects for C4IX, the bare machine and C4DOS but for the platform's
# (.sbk/plat). The art is data, not in the repository: make sbk-data fetches it
# from a fixed commit of upstream's (data/README.md), and c4/mkassets.py turns it
# into .sbk/gen/sbk-assets.c, which is linked in. With no art there is nothing
# to link: no objects, no game on any disk, and test-sbk says it was skipped.
SBK        := games/sbk
SBK_UNITS  := game blit assets sprite3d track kart menu states race sbkmath
SBK_C4     := video save demo
SBK_HDRS   := $(wildcard $(SBK)/src/*.h $(SBK)/c4/*.h) \
              $(wildcard src/c4ix/libc/include/*.h src/c4ix/libc/include/sys/*.h) \
              src/c4ix/include/display.h src/c4ix/include/window.h src/c4ix/include/window.c \
              libjs/guest/gui.h libjs/guest/gui.c
SBK_CC     := ./c4sp32 -c 400000 src/c4sp/lisp/c4lc.lisp -O $(IMG_G) -mfuse -fno-builtin -c -conforming \
              -D C4SBK -D SBK_C4 -I $(SBK)/c4 -I $(SBK)/src -I src/c4ix/libc/include -I src/c4ix/include -I libjs/guest
SBK_GEN    := .sbk/gen

# the art: each file, from upstream's commit, checked by SHA-256
SBK_COMMIT := 93b45a0689e30215ec68f04baf75af88ec688a45
SBK_RAW    := https://raw.githubusercontent.com/Daft-Freak/super-blit-kart/$(SBK_COMMIT)/assets
SBK_DATA   := 8x12font.png:a2b05620961556cbe083d52f4ab04e5a6222f45b797affb613c0bf2a56881b1b \
              8x8font.png:3c8a3851149ea8a3b8ae1e7525cbf9ef4aa645b07b2a1ed231b76abffbde2946 \
              RandomFont3.ttf:c1452336cb5d3b3b5f71fa5dd4c7c90395928a4ab63f65bb20186948a696af87 \
              desert.tmx:97ad1fe41e30166ab3cab713538e74a8f628a733bc00efb08437c5bca75c2874 \
              rainbow.tmx:90c6a04aba790aac04e9066c9b157c5b96f558c31c55f74305151a0266d2ef08 \
              kart.png:a7f16be0248684e0d2a60c5dc2f15cefa39c12376087ec03a229b4afca1155c6 \
              kart_icons.png:1b5ab400dafda82747e299068fe56f5dc50424b4f47002c080edbd2361a70b43 \
              kart_robot_rust_palette.png:89a8f71233c3afe998c1d548fe88f369346c88dcd5eace71b6c027292b7f6986 \
              palettes.h:138d54c86441132b0b578073871f6d324d8f38a56e4627a33ac5bfaab921c216 \
              menu_sprites.png:02da7f0dd616ee4cb57ae8018a0a9d3ef01cc4acce46cfb6b2b4cb5a67f12287 \
              sbklogo.png:5b830bac7ac253f7561b88b84a8c7f3cfe6524afed38137cfc7791182f7e0778 \
              tileset.png:9f03918eb46638721068ef4ce40897370e4bbb4cf9538e9ad9704ad3532df08b \
              tileset.tsx:acc265488d7d969b41654d30c927f88bd0d994e24ebfa7fe88883f71815261f3 \
              track.png:11886f3b8a9d0bb8b042c6262142226f198e45d32350ef4cbe5cb92848c31765 \
              track.tsx:98e406641acfe1b646aa45c05977088632faacbcdde3debf71308e024b7bde15 \
              templates/item.tx:721021dd23057cecc191bac29f221d5930436265ada70d616e43dccd9aa557ed \
              templates/cactus.tx:0ce64227a4e964773b39f29bd689b8f34a494abfa4a40d4d75a372ddaaaab18d
SBK_FILES  := $(foreach d,$(SBK_DATA),$(SBK)/data/$(firstword $(subst :, ,$(d))))
define SBK_FETCH
$(SBK)/data/$(firstword $(subst :, ,$(1))):
	sh games/fetch.sh $$@ $(lastword $(subst :, ,$(1))) SBK_NO_COPY $(SBK_RAW)/$(firstword $(subst :, ,$(1)))
endef
$(foreach d,$(SBK_DATA),$(eval $(call SBK_FETCH,$(d))))
sbk-data: $(SBK_FILES)
PHONY += sbk-data

$(SBK_GEN)/sbk-assets.c: $(SBK)/c4/mkassets.py $(wildcard $(SBK_FILES))
	@mkdir -p $(SBK_GEN)
	python3 -I $(SBK)/c4/mkassets.py $(SBK)/data $@

.sbk/sbk-assets.c4o: $(SBK_GEN)/sbk-assets.c $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .sbk
	$(SBK_CC) $< $@ > /dev/null
.sbk/%.c4o: $(SBK)/src/%.c $(SBK_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .sbk
	$(SBK_CC) $< $@ > /dev/null
.sbk/%.c4o: $(SBK)/c4/%.c $(SBK_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .sbk
	$(SBK_CC) $< $@ > /dev/null
.sbk/plat/%.c4o: $(SBK)/c4/%.c $(SBK_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .sbk/plat
	$(SBK_CC) $< $@ > /dev/null

ifneq ($(wildcard $(SBK)/data/kart.png),)
sbk-objs: $(SBK_UNITS:%=.sbk/%.c4o) $(SBK_C4:%=.sbk/%.c4o) .sbk/sbk-assets.c4o .sbk/plat/plat-c4ix.c4o .sbk/plat/plat-bare.c4o
else
sbk-objs:
endif
PHONY += sbk-objs

# ... and by gcc for the host, with no display (plat-host.c), the reference
# test-sbk holds the c4lc build to; 32-bit, as that is, with SSE's floats, which
# round as the machine's do, and C4IX's math (host-math.c). -fno-builtin: nothing
# folded or inlined around that. The same for the recording build (c4/record.c).
SBK_HOST_CC   := gcc -m32 -O2 -std=gnu99 -w -msse2 -mfpmath=sse -ffp-contract=off -fno-builtin -fwrapv \
                 -fno-strict-aliasing -DC4SBK -I$(SBK)/c4 -I$(SBK)/src -I.
SBK_HOST_SRCS := $(SBK_UNITS:%=$(SBK)/src/%.c) $(SBK)/c4/plat-host.c $(SBK)/c4/host-math.c $(SBK_GEN)/sbk-assets.c
.sbk/host/sbk: $(SBK_HOST_SRCS) $(SBK)/c4/demo.c $(wildcard $(SBK)/src/*.h $(SBK)/c4/*.h)
	@mkdir -p .sbk/host
	$(SBK_HOST_CC) -o $@ $(SBK_HOST_SRCS) $(SBK)/c4/demo.c
.sbk/host/sbk-record: $(SBK_HOST_SRCS) $(SBK)/c4/record.c $(wildcard $(SBK)/src/*.h $(SBK)/c4/*.h)
	@mkdir -p .sbk/host
	$(SBK_HOST_CC) -DSBK_RECORD -o $@ $(SBK_HOST_SRCS) $(SBK)/c4/record.c
sbk-host: .sbk/host/sbk
# make sbk-record, which rewrites c4/demo.c: only when the course (c4/record.c)
# or the game's behaviour has changed
sbk-record: .sbk/host/sbk-record
	.sbk/host/sbk-record -record $(SBK)/c4/demo.c -perf > /dev/null
PHONY += sbk-host sbk-record

# test-sbk: the demo (c4/demo.c), one stream of buttons on a clock of its own,
# with a fixed seed, built by gcc and by c4lc -- on the bare machine, under C4DOS
# and inside C4IX -- must draw the same frames: a line for each, with a checksum
# of its pixels. The C4DOS and C4IX runs must also end and give the machine back,
# and a save written to C4IX's disk must come back (-demo's are in memory).
test-sbk: c4bb-images
	@if [ ! -f $(SBK)/data/kart.png ] || [ ! -f src/c4bb/images/games/sbk.c4r ]; then \
	   echo "test-sbk: skipped, no art (games/sbk/data/README.md)"; exit 0; fi; \
	 $(MAKE) --no-print-directory sbk-host && \
	 rm -rf .sbk_t && mkdir -p .sbk_t && \
	 .sbk/host/sbk -demo | grep '^sbk ' > .sbk_t/host.txt && \
	 ( cd src/c4bb/images/games && timeout 1800 node ../../../../libjs/cli.js -m 64 -d . sbk.c4r -demo 2>/dev/null ) > .sbk_t/bare.out; \
	 { echo "sbk -demo"; echo ver; } | \
	   ( cd src/c4bb/images/games && timeout 1800 node ../../../../libjs/cli.js -m 64 -d . ../c4dos32.c4r 2>/dev/null ) > .sbk_t/dos.out; \
	 { echo "sbk -demo"; echo "cd /tmp"; echo "sbk -savetest"; echo exit; } | \
	   timeout 1800 node libjs/cli.js -m 64 -d src/c4bb/images/disk src/c4bb/images/c4ix32.c4r 2>/dev/null > .sbk_t/c4ix.out; \
	 for w in bare dos c4ix; do sed -e 's/^A:.>//' .sbk_t/$$w.out | grep '^sbk [0-9e]' > .sbk_t/$$w.txt; \
	   cmp -s .sbk_t/host.txt .sbk_t/$$w.txt \
	   || { echo "test-sbk: $$w's frames differ from gcc's:"; diff .sbk_t/host.txt .sbk_t/$$w.txt | head -5; \
	        tail -5 .sbk_t/$$w.out; exit 1; }; done && \
	 { grep -Eq 'C4DOS version' .sbk_t/dos.out || { echo "test-sbk: C4DOS did not come back to its prompt:"; \
	     tail -5 .sbk_t/dos.out; exit 1; }; } && \
	 { grep -q 'sbk savetest OK' .sbk_t/c4ix.out || { echo "test-sbk: a save on C4IX's disk did not come back:"; \
	     tail -5 .sbk_t/c4ix.out; exit 1; }; } && \
	 echo "test-sbk: $$(wc -l < .sbk_t/host.txt) frames, gcc and c4lc on the bare machine, C4DOS and C4IX: OK" && rm -rf .sbk_t
PHONY += test-sbk

# bench-sbk: what a frame costs, in the machine's instructions, on the bare
# machine: the demo's average and worst update, and frame drawn.
bench-sbk: c4bb-images
	@( cd src/c4bb/images/games && timeout 1800 node ../../../../libjs/cli.js -m 64 -d . sbk.c4r -demo -perf 2>/dev/null ) | grep '^sbk'
PHONY += bench-sbk

# test-sbk-ref: the original, upstream's C++ on the 32blit SDK's own engine, built
# by g++ with no window (ref/ref-main.cpp), must draw the same frames as this
# rewrite's gcc build. Needs both repositories from GitHub, at fixed commits, so it
# is not part of test-sbk (ref/README.md).
SBK_SDK_COMMIT := db99cb7fad2ee163bf7251193566bfcf6781da7c
SBK_REF        := .sbk/ref
SBK_REF_SDK    := engine/engine engine/api engine/file engine/input engine/save engine/timer engine/tweening \
                  graphics/blend graphics/color graphics/font graphics/primitive graphics/sprite graphics/surface \
                  graphics/text graphics/tilemap types/mat3 types/mat4 types/vec2 types/vec3
SBK_REF_GAME   := game kart kart-select main-menu menu minimap race sprite3d track track-select
$(SBK_REF)/.got:
	@mkdir -p $(SBK_REF)
	rm -rf $(SBK_REF)/game $(SBK_REF)/sdk
	git clone -q https://github.com/Daft-Freak/super-blit-kart.git $(SBK_REF)/game
	git -C $(SBK_REF)/game checkout -q $(SBK_COMMIT)
	git clone -q https://github.com/32blit/32blit-sdk.git $(SBK_REF)/sdk
	git -C $(SBK_REF)/sdk checkout -q $(SBK_SDK_COMMIT)
	touch $@
$(SBK_REF)/sbk-ref: $(SBK_REF)/.got $(SBK)/ref/ref-main.cpp $(SBK)/c4/mkassets.py $(SBK)/c4/demo.c $(SBK)/c4/host-math.c $(SBK)/src/sbkmath.c
	python3 -I $(SBK)/c4/mkassets.py --ref $(SBK)/data $(SBK_REF)/game
	cd $(SBK_REF)/game && python3 -I extract_track_info.py . .
	for f in $(SBK_REF_GAME) track-info assets; do g++ -std=c++20 -O1 -w -ffp-contract=off -fno-builtin -fwrapv -I$(SBK_REF)/game \
	   -I$(SBK_REF)/sdk/32blit -c $(SBK_REF)/game/$$f.cpp -o $(SBK_REF)/$$f.o || exit 1; done
	for f in $(SBK_REF_SDK); do g++ -std=c++20 -O1 -w -ffp-contract=off -fno-builtin -fwrapv -I$(SBK_REF)/sdk/32blit \
	   -c $(SBK_REF)/sdk/32blit/$$f.cpp -o $(SBK_REF)/$$(echo $$f | tr / _).o || exit 1; done
	gcc -O2 -w -ffp-contract=off -fno-builtin -I. -c $(SBK)/c4/host-math.c -o $(SBK_REF)/host-math.o
	gcc -O2 -w -c $(SBK)/c4/demo.c -o $(SBK_REF)/demo.o
	gcc -O2 -w -msse2 -mfpmath=sse -ffp-contract=off -fno-strict-aliasing -I$(SBK)/src -c $(SBK)/src/sbkmath.c -o $(SBK_REF)/sbkmath.o
	g++ -std=c++20 -O1 -w -ffp-contract=off -fno-builtin -I$(SBK_REF)/sdk/32blit -o $@ $(SBK)/ref/ref-main.cpp \
	   $(SBK_REF)/*.o
sbk-ref: $(SBK_REF)/sbk-ref
test-sbk-ref: sbk-ref sbk-host
	@rm -rf .sbk_t && mkdir -p .sbk_t && \
	 .sbk/host/sbk -demo | grep '^sbk ' > .sbk_t/host.txt && \
	 $(SBK_REF)/sbk-ref | grep '^sbk ' > .sbk_t/ref.txt && \
	 { cmp -s .sbk_t/host.txt .sbk_t/ref.txt || { echo "test-sbk-ref: the rewrite's frames differ from the original's:"; \
	     diff .sbk_t/ref.txt .sbk_t/host.txt | head -5; exit 1; }; } && \
	 echo "test-sbk-ref: $$(wc -l < .sbk_t/host.txt) frames, the rewrite and the original C++: OK" && rm -rf .sbk_t
PHONY += sbk-ref test-sbk-ref

GAME_OBJS  += sbk-objs
GAME_TESTS += test-sbk
