#include <assert.h>
#include <math.h>
#include "workbench_view.h"
int main(void)
{
    BenchView view={1,0,0}; float x,y,sx,sy;
    bench_view_zoom_at(&view,2,500,300);
    bench_view_to_world(&view,500,300,&x,&y);
    assert(fabsf(x-500)<.001f && fabsf(y-300)<.001f);
    bench_view_to_screen(&view,x,y,&sx,&sy);
    assert(fabsf(sx-500)<.001f && fabsf(sy-300)<.001f);
    view.pan_x+=123; view.pan_y-=45;
    bench_view_to_screen(&view,400,250,&sx,&sy);
    bench_view_to_world(&view,sx,sy,&x,&y);
    assert(fabsf(x-400)<.001f && fabsf(y-250)<.001f);
    bench_view_zoom_at(&view,100,500,300); assert(view.zoom==4);
    bench_view_zoom_at(&view,.00001f,500,300); assert(view.zoom==.35f);
    return 0;
}
