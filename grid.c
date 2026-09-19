#include "jgk_tiles.h"

void grid_reset(App *app)
{
	app->has_first = 0;
	app->has_hover = 0;
}

int grid_hit_span(int px, int py, int span_w, int span_h, int *col, int *row)
{
	double cw, rh;

	if (span_w <= 0 || span_h <= 0)
		return 0;
	cw = (double)span_w / GRID_COLS;
	rh = (double)span_h / GRID_ROWS;
	*col = (int)(px / cw);
	*row = (int)(py / rh);
	if (*col < 0) *col = 0;
	if (*col > GRID_COLS - 1) *col = GRID_COLS - 1;
	if (*row < 0) *row = 0;
	if (*row > GRID_ROWS - 1) *row = GRID_ROWS - 1;
	return 1;
}

int grid_hit(const App *app, int px, int py, int *col, int *row)
{
	return grid_hit_span(px, py, app->work_w, app->work_h, col, row);
}

void grid_rect_span(int c1, int r1, int c2, int r2,
                    int ox, int oy, int span_w, int span_h,
                    int *x, int *y, int *w, int *h)
{
	int col_min, col_max, row_min, row_max;
	double relX, relY, relW, relH;
	int x1, y1, x2, y2, max_x, max_y;

	col_min = c1 < c2 ? c1 : c2;
	col_max = c1 > c2 ? c1 : c2;
	row_min = r1 < r2 ? r1 : r2;
	row_max = r1 > r2 ? r1 : r2;

	relX = (double)col_min / GRID_COLS;
	relY = (double)row_min / GRID_ROWS;
	relW = (double)(col_max - col_min + 1) / GRID_COLS;
	relH = (double)(row_max - row_min + 1) / GRID_ROWS;

	x1 = ox + (int)(span_w * relX + 0.5);
	y1 = oy + (int)(span_h * relY + 0.5);
	x2 = ox + (int)(span_w * (relX + relW) + 0.5);
	y2 = oy + (int)(span_h * (relY + relH) + 0.5);
	max_x = ox + span_w;
	max_y = oy + span_h;

	if (x1 < ox) x1 = ox;
	if (y1 < oy) y1 = oy;
	if (x2 > max_x) x2 = max_x;
	if (y2 > max_y) y2 = max_y;
	*x = x1;
	*y = y1;
	*w = x2 - x1;
	*h = y2 - y1;
}

void grid_rect(const App *app, int c1, int r1, int c2, int r2,
               int *x, int *y, int *w, int *h)
{
	grid_rect_span(c1, r1, c2, r2,
	               app->work_x, app->work_y, app->work_w, app->work_h,
	               x, y, w, h);
}
