#include "workbench_view.h"
#include <math.h>

#define DESIGN_VIEW_WIDTH 1060.0f
#define DESIGN_VIEW_HEIGHT 590.0f
static float area_width = DESIGN_VIEW_WIDTH, area_height = DESIGN_VIEW_HEIGHT;

float bench_view_width(void)
{
    return area_width;
}

float bench_view_height(void)
{
    return area_height;
}

void bench_view_set_area(float width, float height)
{
    area_width = isfinite(width) && width >= DESIGN_VIEW_WIDTH ? width : DESIGN_VIEW_WIDTH;
    area_height = isfinite(height) && height >= DESIGN_VIEW_HEIGHT ? height : DESIGN_VIEW_HEIGHT;
}
void bench_view_to_world(const BenchView *v,float sx,float sy,float *x,float *y)
{
    *x=220+(sx-220-v->pan_x)/v->zoom;
    *y=70+(sy-70-v->pan_y)/v->zoom;
}
void bench_view_to_screen(const BenchView *v,float x,float y,float *sx,float *sy)
{
    *sx=220+v->pan_x+(x-220)*v->zoom;
    *sy=70+v->pan_y+(y-70)*v->zoom;
}
void bench_view_zoom_at(BenchView *v,float factor,float sx,float sy)
{
    float x,y,new_zoom;
    if(!isfinite(factor) || factor<=0) return;
    bench_view_to_world(v,sx,sy,&x,&y);
    new_zoom=fminf(BENCH_VIEW_ZOOM_MAX,fmaxf(BENCH_VIEW_ZOOM_MIN,v->zoom*factor));
    v->zoom=new_zoom;
    v->pan_x=sx-220-(x-220)*new_zoom;
    v->pan_y=sy-70-(y-70)*new_zoom;
}

void bench_view_fit(BenchView *view, float min_x, float min_y, float max_x, float max_y)
{
    const float margin = 40.0f;
    float width = fmaxf(1.0f, max_x - min_x);
    float height = fmaxf(1.0f, max_y - min_y);
    float zoom_x = (BENCH_VIEW_WIDTH - 2 * margin) / width;
    float zoom_y = (BENCH_VIEW_HEIGHT - 2 * margin) / height;
    float zoom = fminf(BENCH_VIEW_ZOOM_MAX, fmaxf(BENCH_VIEW_ZOOM_MIN, fminf(zoom_x, zoom_y)));
    float center_x = (min_x + max_x) / 2, center_y = (min_y + max_y) / 2;
    view->zoom = zoom;
    view->pan_x = BENCH_VIEW_LEFT + BENCH_VIEW_WIDTH / 2 - BENCH_VIEW_LEFT - (center_x - BENCH_VIEW_LEFT) * zoom;
    view->pan_y = BENCH_VIEW_TOP + BENCH_VIEW_HEIGHT / 2 - BENCH_VIEW_TOP - (center_y - BENCH_VIEW_TOP) * zoom;
}

float bench_view_edge_push(float position, float low, float high, float zone)
{
    if (!isfinite(position) || zone <= 0 || high - low <= 2 * zone)
        return 0;
    if (position < low + zone)
        return -fminf(1, (low + zone - position) / zone);
    if (position > high - zone)
        return fminf(1, (position - (high - zone)) / zone);
    return 0;
}
