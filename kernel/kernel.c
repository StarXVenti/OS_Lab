#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>

#define SIDE 16
#define TILE_SIZE 16

static int w, h;  // Screen size

#define KEYNAME(key) \
  [AM_KEY_##key] = #key,
static const char *key_names[] = { AM_KEYS(KEYNAME) };

static inline void puts(const char *s) {
  for (; *s; s++) putch(*s);
}

void print_key() {
  AM_INPUT_KEYBRD_T event = { .keycode = AM_KEY_NONE };
  ioe_read(AM_INPUT_KEYBRD, &event);
  if (event.keycode != AM_KEY_NONE && event.keydown) {
    puts("Key pressed: ");
    puts(key_names[event.keycode]);
    puts("\n");
  }
}

static void draw_tile(int x, int y, int w, int h, uint32_t color) {
  uint32_t pixels[w * h]; // WARNING: large stack-allocated memory
  AM_GPU_FBDRAW_T event = {
    .x = x, .y = y, .w = w, .h = h, .sync = 1,
    .pixels = pixels,
  };
  for (int i = 0; i < w * h; i++) {
    pixels[i] = color;
  }
  ioe_write(AM_GPU_FBDRAW, &event);
}

void splash() {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width;
  h = info.height;

  // A simple pattern with 10 colors
  uint32_t colors[10] = {
    0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, // 红绿蓝黄
    0xFFFF00FF, 0xFF00FFFF, 0xFF000000, 0xFFFFFFFF, // 紫红，紫，黑，白
    0xFF888888, 0xFF444444  // 灰，深灰
  };

  for (int x = 0; x * SIDE <= w; x++) {
    for (int y = 0; y * SIDE <= h; y++) {
      draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, colors[(x + y) % 10]);
    }
  }
}

// Operating system is a C program!
int main(const char *args) {
  ioe_init();

  puts("mainargs = \"");
  puts(args);  // make run mainargs=xxx
  puts("\"\n");

  splash();
  
  puts("Press ESC to exit...\n");
  while (1) {
    print_key();
    AM_INPUT_KEYBRD_T event = { .keycode = AM_KEY_NONE };
    ioe_read(AM_INPUT_KEYBRD, &event);
    if (event.keycode == 27 && event.keydown) {
      puts("ESC pressed, exiting...\n");
      break;
    }
  }
  return 0;
}
