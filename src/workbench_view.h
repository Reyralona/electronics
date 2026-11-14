#ifndef WORKBENCH_VIEW_H
#define WORKBENCH_VIEW_H
typedef struct { float zoom, pan_x, pan_y; } BenchView;
void bench_view_to_world(const BenchView *view,float screen_x,float screen_y,float *world_x,float *world_y);
void bench_view_to_screen(const BenchView *view,float world_x,float world_y,float *screen_x,float *screen_y);
void bench_view_zoom_at(BenchView *view,float factor,float screen_x,float screen_y);
#endif
