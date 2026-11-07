// sbk.h -- Super Blit Kart in C. The game is Charlie Birks's C++ (MIT), and
// blit.c is the parts of Pimoroni's 32blit SDK it draws with (MIT); each unit
// here has the upstream file's name and keeps its order, so the two can be read
// side by side. Everything is word-sized and explicit for c4lc, and the float
// expressions keep upstream's order of evaluation, because make test-sbk
// holds them to it bit for bit (README.c4.md).
#ifndef SBK_H
#define SBK_H

enum { SCREEN_W = 320, SCREEN_H = 240 };

// blit::Button
enum { BTN_LEFT = 1, BTN_RIGHT = 2, BTN_UP = 4, BTN_DOWN = 8, BTN_A = 16, BTN_B = 32,
       BTN_X = 64, BTN_Y = 128, BTN_MENU = 256, BTN_HOME = 512, BTN_JOYSTICK = 1024 };

typedef struct Vec3 { float x, y, z; } Vec3;

// A pen is a word: alpha, red, green, blue, high byte first.
#define PEN(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define RGB(r, g, b) PEN(r, g, b, 255)

// blit::Surface, for what is only ever read: 8 bits through a palette
typedef struct Surf {
	unsigned char *data;
	int w, h;
	int *pal;
} Surf;

typedef struct Font {
	unsigned char *widths, *data;
	int char_w, char_h, spacing;
} Font;

// blit::TextAlign
enum { ALIGN_TOP_LEFT = 0, ALIGN_CENTER_V = 1, ALIGN_BOTTOM = 2, ALIGN_CENTER_H = 4,
       ALIGN_CENTER = 5, ALIGN_CENTER_LEFT = 1, ALIGN_BOTTOM_LEFT = 2, ALIGN_TOP_CENTER = 4 };

enum { XFORM_HORIZONTAL = 1 };

// blit.c: the screen, which is a framebuffer of 0x00RRGGBB
extern int *fb;
extern int clip_x, clip_y, clip_w, clip_h;
extern int scr_alpha, scr_pen;
void scr_init(void);
void scr_clip(int x, int y, int w, int h);
void pen_span(int pen, int off, int c);
void scr_clear(void);
void scr_rect(int x, int y, int w, int h);
void scr_hspan(int x, int y, int c);
void scr_vspan(int x, int y, int c);
void scr_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void scr_blit(Surf *src, int sx, int sy, int sw, int sh, int px, int py);
void scr_sprite(Surf *src, int sx, int sy, int sw, int sh, int px, int py);
void scr_sprite_scaled(Surf *src, int sx, int sy, int sw, int sh, int px, int py, int ox, int oy, float scale, int t);
void scr_stretch(Surf *src, int sx, int sy, int sw, int sh, int rx, int ry, int rw, int rh, int t);
void scr_text(char *s, Font *f, int rx, int ry, int rw, int rh, int align);
void scr_textat(char *s, Font *f, int x, int y, int align);
int  measure_w(char *s, Font *f);
int  measure_h(Font *f);
void text_bytes(char *s, Font *f, unsigned char *buf, int w, int h);
int  hsv_to_pen(float h, float s, float v);
int  pen_alpha(int a1, int a2);

// assets.c
enum { MAX_TRACKS = 2, NUM_KARTS = 8 };
typedef struct RectF { int x, y, w, h; float fx, fy, fx2, fy2; } RectF;
typedef struct ObjInfo { int type, scale, x, y, sx, sy, sw, sh, ox, oy; } ObjInfo;
typedef struct Seg { float ax, ay, bx, by, rx, ry, dx, dy, rxs, nx, ny; } Seg;
typedef struct TrackInfo {
	char *name;
	int w, h, shift;
	unsigned char *tiles, *trans;
	int bg;
	Surf *sheet;
	int fin[4];
	int *route;            // x, y, ...
	int route_len;
	Seg *segs;             // route_len - 1 of what find_closest_route_segment works out
	RectF *rects;
	int num_rects;
	ObjInfo *sprites;
	int num_sprites;
	float *friction;       // of each of the 256 tiles
	unsigned char *mm_class;   // the minimap's colour for each
	int *tile_base;        // where each tile starts in the sheet
} TrackInfo;
extern TrackInfo track_info[];
extern int num_tracks;
extern Surf a_tileset, a_track, a_kart, a_kart_alt, a_logo, a_menu_sprites, a_kart_icons;
extern Font tall_font, big_number_font, menu_font;
void assets_load(void);

// camera
typedef struct Camera {
	Vec3 pos, look_at, forward, right, up;
	float focal, near_z, far_z;
	int vx, vy, vw, vh;
} Camera;
void cam_init(Camera *c);
void cam_update(Camera *c);

// sprite3d.c
typedef struct Sprite3D {
	Vec3 world_pos, look_dir;
	float scale, alpha;
	int origin_x, origin_y, sheet_x, sheet_y, size_w, size_h, rotation_frames;
	Surf *sheet;
	int screen_x, screen_y;
	float screen_scale, z;
} Sprite3D;
void sprite_init(Sprite3D *s);
void sprite_update(Sprite3D *s, Camera *c);
void sprite_render(Sprite3D *s, Camera *c);

// track.c
enum { OBJ_REMOVED = 0, OBJ_STATIC, OBJ_ITEM, OBJ_DROPPED, OBJ_PROJECTILE };
typedef struct TrackObject {
	int type, respawn_timer;
	Sprite3D sprite;
	Vec3 vel;
} TrackObject;
typedef struct Track {
	TrackInfo *info;
	Surf *tiles;
	TrackObject *objects;
	int num_objects, cap_objects;
	float fog;
	int **fog_rows;        // tiles' colours over the background, by alpha, as they are needed
	int ncolours;
} Track;
Track *track_new(TrackInfo *info);
void track_free(Track *t);
void track_render(Track *t, Camera *c, int plain);
void track_reset_objects(Track *t);
TrackObject *track_add_object(Track *t);
int  track_closest_segment(Track *t, float px, float py, float *segment_t);
float track_friction(Track *t, float px, float py);
void rot_y(Vec3 *v, float degrees);

// save
typedef struct RaceSave { unsigned char save_version, place; unsigned short time; } RaceSave;
typedef struct GhostEntry { short pos_x, pos_z, look_ang; unsigned short pad; } GhostEntry;
enum { GHOST_MAX = 4096 };
typedef struct TimeTrialSave {
	unsigned char save_version, kart;
	unsigned short lap_time[3];
	char name[8];
	GhostEntry ghost_data[GHOST_MAX];
	unsigned short ghost_data_used;
} TimeTrialSave;
#define SAVE_SLOT(type, track, slot) (((type) << 12) | ((track) << 4) | (slot))
enum { SAVE_RACE = 0, SAVE_TIME_TRIAL = 1 };
int  read_save(void *data, int len, int slot);
void write_save(void *data, int len, int slot);

// items
enum { ITEM_NONE = -1, ITEM_DROP = 0, ITEM_PROJECTILE, ITEM_BOOST };
extern int item_sprite_x[3];

// kart.c
typedef struct RaceState RaceState;
typedef struct Kart {
	Sprite3D sprite;
	int is_player, current_place, kart_index;
	Vec3 vel, acc;
	float turn_speed;
	int boost_time, disable_time;
	float return_to_track_timer;
	Vec3 return_pos_v, return_look_v;
	int current_lap;
	unsigned lap_start_time[3];
	unsigned finish_time;
	int current_route_segment;
	float current_route_frac;
	int current_item;
	TimeTrialSave *time_trial_data;
	int ghost_offset, ghost_len, ghost_timer;
	RaceState *race_state;
} Kart;
struct RaceState {
	Kart karts[NUM_KARTS];
	Track *track;
	int countdown;
};
void kart_reset(Kart *k);
void kart_update(Kart *k);
int  kart_finished(Kart *k);
int  kart_lap_time(Kart *k, int lap);
int  kart_race_time(Kart *k);
void kart_collect_item(Kart *k);
void kart_disable(Kart *k);
#define KART_IS_GHOST(k) ((k)->time_trial_data && !(k)->is_player)
extern float kart_radius_f;
float vec3_len(Vec3 *v);
void  vec3_normalize(Vec3 *v);

// menu.c
enum { MENU_MAX = 8 };
typedef struct Menu {
	char *title;
	char *labels[MENU_MAX];
	int ids[MENU_MAX];
	int num_items;
	Font *font;
	int dx, dy, dw, dh;
	int current_item;
	float scroll_offset;
	unsigned repeat_start_time;
	int header_h, footer_h, margin_y, item_h, item_padding_x, item_adjust_y, item_spacing;
	int background, foreground, selected_bg, header_bg, header_fg;
	void (*on_item)(int id);
} Menu;
void menu_init(Menu *m, char *title, Font *font);
void menu_add(Menu *m, int id, char *label);
void menu_set_rect(Menu *m, int x, int y, int w, int h);
void menu_update(Menu *m, unsigned time);
void menu_render(Menu *m);

// game.c: the game's states, one at a time, and blit::engine's clock and buttons
typedef struct GameState {
	void (*update)(unsigned time);
	void (*render)(void);
	void (*destroy)(void);
} GameState;
extern unsigned sbk_now;
extern unsigned btn_state, btn_pressed, btn_released;
extern int sbk_quit;
// -demo -perf: what a race costs, by part
typedef struct PerfRace { unsigned frames, updates, update, lists, mode7, sprites, hud; } PerfRace;
extern int sbk_perf;
extern PerfRace perf_race[];
void game_change_state(GameState *s);
void game_update(unsigned time);
void game_render(void);
void game_init(void);
void game_tick(unsigned time);
unsigned sbk_random(void);
void sbk_seed(unsigned s);

// the states' constructors: main-menu.c, kart-select.c, track-select.c, race.c
enum { MODE_RACE = 0, MODE_TIME_TRIAL = 1 };
GameState *main_menu_new(int initial);
GameState *kart_select_new(int mode);
GameState *track_select_new(int player_kart, int mode);
GameState *race_new(int player_kart, int track_index, int mode);
// for c4/record.c, which drives while it records the demo
RaceState *race_state_now(void);
int race_end_menu_up(void);
extern int kart_info_count;
extern char *kart_info_name[2];
extern Surf *kart_info_surf[2];

// the platform (c4/plat-*.c, c4/video.c)
int  plat_ms(void);
void plat_sleep(int ms);
void plat_print(char *s);

#endif
