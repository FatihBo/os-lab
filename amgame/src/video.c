#include <game.h>

int w, h;


void gpu_init() {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width;
  h = info.height;
}



void draw_line(int x, int y, int w, int h, uint32_t color) {
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
