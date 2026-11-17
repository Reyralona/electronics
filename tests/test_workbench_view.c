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
    bench_view_zoom_at(&view,.00001f,500,300); assert(view.zoom==BENCH_VIEW_ZOOM_MIN);
    /* A circuit taller than the screen at the old 0.35 limit must fit entirely. */
    bench_view_fit(&view,-624,-628,362,1292);
    assert(view.zoom<.35f && view.zoom>=BENCH_VIEW_ZOOM_MIN);
    bench_view_to_screen(&view,-624,-628,&sx,&sy);
    assert(sx>=BENCH_VIEW_LEFT && sy>=BENCH_VIEW_TOP);
    bench_view_to_screen(&view,362,1292,&sx,&sy);
    assert(sx<=BENCH_VIEW_LEFT+BENCH_VIEW_WIDTH && sy<=BENCH_VIEW_TOP+BENCH_VIEW_HEIGHT);
    /* Edge push: none in the middle, full at and beyond the edges, signed toward the edge. */
    assert(bench_view_edge_push(600,220,1280,32)==0);
    assert(bench_view_edge_push(1280,220,1280,32)==1);
    assert(bench_view_edge_push(100,220,1280,32)==-1);
    assert(bench_view_edge_push(236,220,1280,32)<0 && bench_view_edge_push(236,220,1280,32)>-1);
    return 0;
}
