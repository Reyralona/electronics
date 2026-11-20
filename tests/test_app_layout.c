#include "app_layout.h"
#include "workbench_view.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int near(float a, float b)
{
    return fabsf(a - b) < .01f;
}

/* A person maximizes the window on a 1920 x 1080 screen at 100%: the interface keeps its size, the workspace
 * takes the extra room, the inspector stays at the right edge and dialogs stay centered. */
static void maximized_window_grows_the_workspace(void)
{
    app_layout_set_canvas(1920, 1080, 1);
    assert(near(app_layout_canvas_width(), 1920) && near(app_layout_canvas_height(), 1080));
    assert(near(BENCH_VIEW_WIDTH, 1700) && near(BENCH_VIEW_HEIGHT, 950) && near(BENCH_VIEW_BOTTOM, 1020));
    AppRect viewport = app_layout_viewport(1920, 1080);
    assert(near(viewport.x, 0) && near(viewport.w, 1920) && near(viewport.h, 1080));
    assert(near(app_layout_inspector(100).x + app_layout_inspector(100).w, 1900));
    AppRect examples = app_layout_examples_panel();
    assert(near(examples.x, 270 + 320) && near(examples.y, 84 + 180)); /* half the extra room on each side */
    assert(near(app_layout_header(APP_HEADER_PLAY).x + app_layout_header(APP_HEADER_PLAY).w, 1902));
    assert(APP_PALETTE_VISIBLE == 13);
}

/* Then picks 150%: the same window shows the design canvas, every unit 1.5 pixels, with no margins. */
static void larger_scale_shrinks_the_canvas(void)
{
    app_layout_set_canvas(1920, 1080, 1.5f);
    assert(near(app_layout_canvas_width(), 1280) && near(app_layout_canvas_height(), 720));
    AppRect viewport = app_layout_viewport(1920, 1080);
    assert(near(viewport.w, 1920) && near(viewport.h, 1080));
    assert(APP_PALETTE_VISIBLE == 7);
    WirePoint ui = app_layout_to_ui(1920, 1080, 960, 540), back = app_layout_from_ui(1920, 1080, ui.x, ui.y);
    assert(near(ui.x, 640) && near(ui.y, 360) && near(back.x, 960) && near(back.y, 540));
}

/* A window smaller than the design canvas keeps the proportions: uniform scale, centered, never stretched. */
static void small_window_keeps_proportions(void)
{
    app_layout_set_canvas(1000, 700, 1);
    assert(near(app_layout_canvas_width(), 1280) && near(app_layout_canvas_height(), 720));
    AppRect viewport = app_layout_viewport(1000, 700);
    assert(near(viewport.w / viewport.h, 1280.0f / 720) && near(viewport.x, 0) && viewport.y > 0);
    app_layout_set_canvas(1280, 720, 1);
}

int main(void)
{
    maximized_window_grows_the_workspace();
    larger_scale_shrinks_the_canvas();
    small_window_keeps_proportions();
    printf("Layout: maximized workspace, scale inside the window, proportional small window passed.\n");
    return 0;
}
