#ifndef WORKBENCH_VIEW_H
#define WORKBENCH_VIEW_H
#include "workbench.h"
typedef struct { float zoom, pan_x, pan_y; } BenchView;
/* Zoom limits for wheel zoom and fitting. */
#define BENCH_VIEW_ZOOM_MIN 0.12f
#define BENCH_VIEW_ZOOM_MAX 4.0f
/* Visible workspace in screen coordinates (between catalog, header and status bar). */
#define BENCH_VIEW_LEFT 220.0f
#define BENCH_VIEW_TOP 70.0f
#define BENCH_VIEW_WIDTH 1060.0f
#define BENCH_VIEW_HEIGHT 590.0f
void bench_view_to_world(const BenchView *view,float screen_x,float screen_y,float *world_x,float *world_y);
void bench_view_to_screen(const BenchView *view,float world_x,float world_y,float *screen_x,float *screen_y);
void bench_view_zoom_at(BenchView *view,float factor,float screen_x,float screen_y);
/* Edge auto-scroll while dragging: -1..1 toward the nearer edge inside `zone` pixels of [low, high]
   (and beyond it), 0 elsewhere. Positive means the pointer is near `high`. */
float bench_view_edge_push(float position, float low, float high, float zone);
/* Zoom and pan so the world rectangle fits the workspace, within the zoom limits. */
void bench_view_fit(BenchView *view, float min_x, float min_y, float max_x, float max_y);
void bench_view_fit_workbench(BenchView *view, const Workbench *bench);
/* 1:1 at the origin, or framed when the bench is larger than the window at 1:1 (the 8-bit computer). */
void bench_view_open_example(BenchView *view, const Workbench *bench);
#endif
