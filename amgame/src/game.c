#include <game.h>


typedef struct SQUARE {
	int x, y, edge, side;
} Square;
Square square;


//为了避免爆栈，所以一次只画一个单元的图像
void safe_draw_line(int x, int y, int w, int h, int color) {
	int row = x, row_end = x + w;
	while(row < row_end) {
		int col = y, col_end = y + h;
		while(col < col_end) {
			draw_line(row, col, square.side, square.side, color);
			col += square.side;
		}
		row += square.side;
	}
}






//检查左边碰壁;原始最右侧画成黑色，新的最左侧画成白色
void move_left() {
	if(square.x - 1 >= 0) {

		//(x + edge - 1, y)起点的宽度为1，高度为edge的黑线
		safe_draw_line((square.x + square.edge - 1) * square.side, square.y * square.side, square.side, square.edge * square.side, BLACK);

		//(x - 1, y)起点的宽度为1，高度为edge的白线
		safe_draw_line((square.x - 1) * square.side, square.y * square.side, square.side, square.edge * square.side, WHITE);

		--square.x;
	}
}




//检查右边碰壁;原始最左侧画成黑色，新的最右侧画成白色
void move_right() {
	if((square.x + square.edge) * square.side < w) {

		//(x, y)起点的宽度为1，高度为edge的黑线
		safe_draw_line(square.x * square.side, square.y * square.side, square.side, square.edge * square.side, BLACK);

		//(x + edge, y)起点的宽度为1，高度为edge的白线
		safe_draw_line((square.x + square.edge) * square.side, square.y * square.side, square.side, square.edge * square.side, WHITE);

		++square.x;
	}
}




//检查上边碰壁;原始最下侧画成黑色，新的最上侧画成白色
void move_up() {
	if(square.y - 1 >= 0) {

		//(x, y + edge - 1)起点的宽度为edge，高度为1的黑线
		safe_draw_line(square.x * square.side, (square.y + square.edge - 1) * square.side, square.edge * square.side, square.side, BLACK);

		//(x, y - 1)起点的宽度为edge，高度为1的白线
		safe_draw_line(square.x * square.side, (square.y - 1) * square.side, square.edge * square.side, square.side, WHITE);

		--square.y;
	}
}




//检查下边碰壁;原始最上侧画成黑色，新的最下侧画成白色
void move_down() {
	if((square.y + square.edge) * square.side < h) {

		//(x, y)起点的宽度为edge，高度为1的黑线
		safe_draw_line(square.x * square.side, square.y * square.side, square.edge * square.side, square.side, BLACK);

		//(x, y + edge)起点的宽度为edge，高度为1的白线
		safe_draw_line(square.x * square.side, (square.y + square.edge) * square.side, square.edge * square.side, square.side, WHITE);

		++square.y;
	}
}



//检查边长不能小于1;将最下方和最右方的边画成黑色
void figure_small() {
	if(square.edge - 1 > 0) {

		//(x, y + square.edge - 1)起点的宽度为edge，高度为1的黑线
		safe_draw_line(square.x * square.side, (square.y + square.edge - 1) * square.side, square.edge * square.side, square.side, BLACK);

		//(x + square.edge - 1, y)起点的宽度为1， 高度为edge - 1的黑线
		safe_draw_line((square.x + square.edge - 1) * square.side, square.y * square.side, square.side, (square.edge - 1) * square.side, BLACK);

		--square.edge;
	}
}




//检查增加的边长不能越界;将新的下方和最右方的边画成白色
void figure_big() {
	if((square.x + square.edge) * square.side < w && (square.y + square.edge) * square.side < h) {

		//(x, y + edge)起点的宽度为edge + 1，高度为1的白线
		safe_draw_line(square.x * square.side, (square.y + square.edge) * square.side, (square.edge + 1) * square.side, square.side, WHITE);

		//(x + square.edge, y)起点的宽度为1， 高度为edge的白线
		safe_draw_line((square.x + square.edge) * square.side, square.y * square.side, square.side, square.edge * square.side, WHITE);

		++square.edge;
	}
}



void square_init() {
	square.x = square.y = 0;
	square.edge = 1;
	square.side = 16;

	//画一条宽度为edge， 高度为edge的线即可
	safe_draw_line(square.x * square.side, square.y * square.side, square.edge * square.side, square.edge * square.side, WHITE);
}



// Operating system is a C program!
int main(const char *args) {
  ioe_init();
  gpu_init();

  puts("mainargs = \"");
  puts(args); // make run mainargs=xxx
  puts("\"\n");


  //初始化方块
  square_init();
  puts("Press any key to see its key code...\n");

  while (1) {
    const char *key = print_key();
  

    //方便肉眼观察
    int delta = 262;
    while(delta--) {;}


    if(!strcmp(key, "W")) { move_up(); }
    else if(!strcmp(key, "S")) { move_down(); }
    
    else if(!strcmp(key, "A")) { move_left(); }
    else if(!strcmp(key, "D")) { move_right(); }
    else if(!strcmp(key, "J")) { figure_big(); }
    else if(!strcmp(key, "K")) { figure_small(); }
    else if(!strcmp(key, "ESCAPE")) { halt(0); }

  }
  return 0;
}
