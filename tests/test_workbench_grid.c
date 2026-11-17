#include "workbench_grid.h"
#include <assert.h>
#include <float.h>
#include <math.h>

int main(void)
{
    BenchGrid grid = bench_grid_default();
    double x = 0, y = 0;
    assert(bench_grid_snap(&grid, 25, 45, &x, &y));
    assert(x == 24 && y == 44);
    assert(bench_grid_snap(&grid, -25, -5, &x, &y));
    assert(x == -24 && y == -4);
    grid = (BenchGrid){10, 3, -7};
    assert(bench_grid_snap(&grid, 8, -12, &x, &y));
    assert(x == 13 && y == -17);
    assert(bench_grid_snap(&grid, x, y, &x, &y));
    {
        float aligned_x = 101, aligned_y = 157;
        assert(bench_grid_align_axis(100, 50, 8, &aligned_x, &aligned_y));
        assert(aligned_x == 100 && aligned_y == 157);
        aligned_x = 160;
        aligned_y = 53;
        assert(bench_grid_align_axis(100, 50, 8, &aligned_x, &aligned_y));
        assert(aligned_x == 160 && aligned_y == 50);
        aligned_x = 140;
        aligned_y = 90;
        assert(!bench_grid_align_axis(100, 50, 8, &aligned_x, &aligned_y));
    }
    assert(x == 13 && y == -17);
    assert(bench_grid_display_spacing(&grid, .25, 10) == 40);
    assert(grid.spacing == 10);
    assert(bench_grid_display_spacing(&grid, 2, 10) == 10);
    assert(!bench_grid_snap(&grid, INFINITY, 0, &x, &y));
    assert(x == 13 && y == -17);
    grid.spacing = 0;
    assert(!bench_grid_snap(&grid, 1, 1, &x, &y));
    grid.spacing = NAN;
    assert(!bench_grid_valid(&grid));
    grid.spacing = DBL_MIN;
    assert(!bench_grid_snap(&grid, DBL_MAX, 1, &x, &y));
    assert(!bench_grid_display_spacing(&grid, 0, 10));
    return 0;
}
