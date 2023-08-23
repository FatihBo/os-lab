#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>

#define BLACK (0x0)
#define WHITE (0xffffff)

extern int w, h;

void gpu_init();
const char* print_key();
void draw_line(int x, int y, int w, int h, uint32_t color);
static inline void puts(const char *s) {
  for (; *s; s++) putch(*s);
}
