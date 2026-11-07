// plat.h -- what the game asks of where it runs: plat-c4ix.c, plat-bare.c (the
// bare machine and C4DOS, under grin or not), plat-host.c (gcc's build, the
// reference make test-sbk holds c4lc's to, which has no display and only runs
// -demo). video.c is the display's half, for the first two.
#ifndef SBK_PLAT_H
#define SBK_PLAT_H
int  plat_ms(void);
void plat_sleep(int ms);
void plat_print(char *s);
unsigned plat_cycles(void);        // instructions, for -demo -perf
void plat_shot(char *file, int *fb);   // a frame as a .ppm: only gcc's build does
int  plat_save_read(int slot, char *data, int len);
void plat_save_write(int slot, char *data, int len);

int  video_open(int window);       // the display, or a window: 1 if there is one
int  video_poll(void);             // sets the buttons; 0 when the window has closed
void video_show(void);
void video_close(void);

// what video.c asks of plat-c4ix.c or plat-bare.c
int  plat_take_display(void);      // 1: the display is the game's
void plat_give_display(void);
int  plat_win_open(char *title, int w, int h);   // a window instead; 1 if there is one
void plat_win_frame(char *pixels, int w, int h, int pitch, int *pal);
int  plat_win_poll(int *ev);       // as gui_poll; -1: the window has closed
void plat_win_close(void);
int  plat_win_ready(void);         // 1: a frame may go now
#endif
