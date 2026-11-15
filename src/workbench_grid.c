#include "workbench_grid.h"
#include <math.h>

BenchGrid bench_grid_default(void)
{
    return (BenchGrid){24, 0, 20};
}

int bench_grid_valid(const BenchGrid *grid)
{
    return grid && isfinite(grid->spacing) && grid->spacing > 0 &&
           isfinite(grid->origin_x) && isfinite(grid->origin_y);
}

int bench_grid_snap(const BenchGrid *grid, double x, double y, double *snapped_x, double *snapped_y)
{
    if (!bench_grid_valid(grid) || !isfinite(x) || !isfinite(y) || !snapped_x || !snapped_y)
        return 0;
    double result_x = grid->origin_x + round((x - grid->origin_x) / grid->spacing) * grid->spacing;
    double result_y = grid->origin_y + round((y - grid->origin_y) / grid->spacing) * grid->spacing;
    if (!isfinite(result_x) || !isfinite(result_y))
        return 0;
    *snapped_x = result_x;
    *snapped_y = result_y;
    return 1;
}

double bench_grid_display_spacing(const BenchGrid *grid, double zoom, double minimum_pixels)
{
    if (!bench_grid_valid(grid) || !isfinite(zoom) || zoom <= 0 ||
        !isfinite(minimum_pixels) || minimum_pixels <= 0)
        return 0;
    double spacing = grid->spacing;
    while (spacing * zoom < minimum_pixels)
    {
        double next = spacing * 2;
        if (!isfinite(next))
            return 0;
        spacing = next;
    }
    return spacing;
}
