// ref-main.cpp -- the original C++ game, unchanged, on the 32blit SDK's own engine
// and drawing code, with no window: this file is all that is new. It plays
// c4/demo.c as the rewrite's -demo does, on the same clock and with the same
// random numbers, and prints the same line for each frame (ref/README.md).
// In-memory saves, memory that starts at zero (upstream draws its minimap from
// a buffer it has not filled yet for one frame), and libm from C4IX's math.c
// (c4/host-math.c) so nothing differs but the code being compared.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "32blit.hpp"
#include "engine/api_private.hpp"

void init();
void update(uint32_t time);
void render(uint32_t time);

extern "C" int sbk_demo[];
extern "C" int sbk_demo_len;

void *operator new(std::size_t n) { void *p = calloc(1, n ? n : 1); if (!p) abort(); return p; }
void *operator new[](std::size_t n) { void *p = calloc(1, n ? n : 1); if (!p) abort(); return p; }
void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete(void *p, std::size_t) noexcept { free(p); }
void operator delete[](void *p, std::size_t) noexcept { free(p); }

namespace blit {
  static APIConst api_const;
  static APIData api_data_store;
  const APIConst &api = api_const;
  APIData &api_data = api_data_store;
}
using namespace blit;

static uint8_t framebuffer[320 * 240 * 3];
static uint32_t cur_time;
static uint32_t rng = 12345;

static uint32_t api_now() { return cur_time; }
static uint32_t api_random() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}

static bool set_mode(ScreenMode mode, SurfaceTemplate &t) {
  t.data = framebuffer;
  t.bounds = Size(320, 240);
  t.format = PixelFormat::RGB;
  return true;
}

// saves, as files in memory
struct MemFile { std::vector<char> data; };
static std::map<std::string, MemFile *> files;
struct Handle { MemFile *f; };

static void *open_file(const std::string &name, int mode) {
  auto it = files.find(name);
  if (mode & int(OpenMode::write)) {
    MemFile *f = it == files.end() ? (files[name] = new MemFile) : it->second;
    return new Handle{f};
  }
  if (it == files.end()) return nullptr;
  return new Handle{it->second};
}
static int32_t read_file(void *fh, uint32_t off, uint32_t len, char *buf) {
  auto f = ((Handle *)fh)->f;
  if (off >= f->data.size()) return 0;
  uint32_t n = std::min<uint32_t>(len, f->data.size() - off);
  memcpy(buf, f->data.data() + off, n);
  return n;
}
static int32_t write_file(void *fh, uint32_t off, uint32_t len, const char *buf) {
  auto f = ((Handle *)fh)->f;
  if (f->data.size() < off + len) f->data.resize(off + len);
  memcpy(f->data.data() + off, buf, len);
  return len;
}
static int32_t close_file(void *fh) { delete (Handle *)fh; return 0; }
static uint32_t file_length(void *fh) { return ((Handle *)fh)->f->data.size(); }
static const char *save_path() { return ""; }

static uint32_t frame_hash() {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 320 * 240; i++) {
    uint8_t *p = framebuffer + i * 3;
    h = (h ^ ((uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2])) * 16777619u;
  }
  return h;
}

static void shot(const char *file) {
  FILE *f = fopen(file, "wb");
  if (!f) return;
  fprintf(f, "P6\n320 240\n255\n");
  fwrite(framebuffer, 1, sizeof(framebuffer), f);
  fclose(f);
}

int main(int argc, char **argv) {
  const int updates_per_frame = 4;
  int shot_frame = -1;
  const char *shot_file = nullptr;
  if (argc == 4 && !strcmp(argv[1], "-shot")) {
    shot_frame = atoi(argv[2]);
    shot_file = argv[3];
  }

  api_const.now = api_now;
  api_const.random = api_random;
  api_const.set_screen_mode_format = set_mode;
  api_const.open_file = open_file;
  api_const.read_file = read_file;
  api_const.write_file = write_file;
  api_const.close_file = close_file;
  api_const.get_file_length = file_length;
  api_const.get_save_path = save_path;

  blit::update = ::update;
  blit::render = ::render;
  blit::init = ::init;

  cur_time = 1000;
  blit::init();
  blit::tick(cur_time);

  int frame = 0, n = 0, i = 0;
  while (i < sbk_demo_len) {
    int count = sbk_demo[i], mask = sbk_demo[i + 1];
    i += 2;
    while (count--) {
      blit::api_data.buttons.state = mask;
      cur_time += 10;
      blit::tick(cur_time);
      if (++n == updates_per_frame) {
        n = 0;
        blit::render(cur_time);
        printf("sbk %d %08x\n", frame, frame_hash());
        if (frame == shot_frame && shot_file) shot(shot_file);
        frame++;
      }
    }
  }
  printf("sbk end %d %08x\n", frame, frame_hash());
  return 0;
}
