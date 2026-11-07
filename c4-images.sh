# c4-images.sh c4ix|bare -- Super Blit Kart's part of c4's build-images.sh,
# which sources it from c4's root (games/README.md in c4). One program, with its
# art inside it: there is no data to put on a disk, and with no art fetched
# there is no game (games/sbk/data/README.md).
SBK_LINK=".sbk/game.c4o .sbk/blit.c4o .sbk/assets.c4o .sbk/sprite3d.c4o .sbk/track.c4o .sbk/kart.c4o \
 .sbk/menu.c4o .sbk/states.c4o .sbk/race.c4o .sbk/video.c4o .sbk/save.c4o .sbk/demo.c4o .sbk/sbk-assets.c4o"
case "$1" in
c4ix)
    if [ -f games/sbk/data/kart.png ] && [ -f .sbk/sbk-assets.c4o ] && [ -f $PB/lib/libc.c4s ]; then
        say "linking Super Blit Kart for C4IX"
        ./c4rlink32 $SBK_LINK .sbk/plat/plat-c4ix.c4o -l $PB/lib/libc.c4s -o $DISK/c4ix-sbk.c4r > /dev/null
    fi ;;
bare)
    if [ -f games/sbk/data/kart.png ] && [ -f .sbk/sbk-assets.c4o ]; then
        item sbk
        ./c4rlink32 $SBK_LINK .sbk/plat/plat-bare.c4o .libc-bare/libc-*.c4o -o $GAMESDISK/sbk.c4r > /dev/null
        GAMES_ON_DISK="$GAMES_ON_DISK SBK"
    fi ;;
esac
