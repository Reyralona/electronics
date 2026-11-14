#include "workbench_view.h"
#include <math.h>
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
    new_zoom=fminf(4.0f,fmaxf(.35f,v->zoom*factor));
    v->zoom=new_zoom;
    v->pan_x=sx-220-(x-220)*new_zoom;
    v->pan_y=sy-70-(y-70)*new_zoom;
}
