#ifndef WORKBENCH_GRID_H
#define WORKBENCH_GRID_H

typedef struct
{
    double spacing;
    double origin_x;
    double origin_y;
} BenchGrid;

BenchGrid bench_grid_default(void);
int bench_grid_valid(const BenchGrid *grid);
/* Failure leaves both outputs unchanged. Coordinates are always world coordinates. */
int bench_grid_snap(const BenchGrid *grid, double x, double y, double *snapped_x, double *snapped_y);
/* Align a point to the nearest horizontal or vertical axis through anchor when within tolerance. */
int bench_grid_align_axis(float anchor_x, float anchor_y, float tolerance, float *x, float *y);
/* Drawing may skip lines, but this never changes the snapping spacing. */
double bench_grid_display_spacing(const BenchGrid *grid, double zoom, double minimum_pixels);

#endif
