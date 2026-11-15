#include "bench_path.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_PATH_CAPACITY 8

static int point_valid(WirePoint point)
{
    return isfinite(point.x) && isfinite(point.y);
}

int bench_polyline_pick(const WirePoint *points, size_t count, WirePoint point, double tolerance, BenchPathHit *hit)
{
    if (!points || count < 2 || !hit || !point_valid(point) || !isfinite(tolerance) || tolerance < 0)
        return 0;
    for (size_t i = 0; i < count; ++i)
        if (!point_valid(points[i]))
            return 0;
    BenchPathHit nearest = {0};
    double best = tolerance * tolerance;
    int found = 0;
    for (size_t i = 1; i < count; ++i)
    {
        double dx = (double)points[i].x - points[i - 1].x;
        double dy = (double)points[i].y - points[i - 1].y;
        double length = dx * dx + dy * dy;
        double t = 0;
        if (length > 0)
            t = fmin(1, fmax(0, ((point.x - points[i - 1].x) * dx + (point.y - points[i - 1].y) * dy) / length));
        WirePoint projected = {(float)(points[i - 1].x + t * dx), (float)(points[i - 1].y + t * dy)};
        double offset_x = (double)point.x - projected.x;
        double offset_y = (double)point.y - projected.y;
        double distance = offset_x * offset_x + offset_y * offset_y;
        if (distance <= best && (!found || distance < best))
        {
            best = distance;
            nearest = (BenchPathHit){i - 1, projected, distance};
            found = 1;
        }
    }
    if (found)
        *hit = nearest;
    return found;
}

void bench_path_clear(BenchPath *path)
{
    if (!path)
        return;
    free(path->points);
    *path = (BenchPath){0};
}

BenchPathStatus bench_path_reserve(BenchPath *path, size_t requested)
{
    if (!path)
        return BENCH_PATH_INVALID;
    if (requested > SIZE_MAX / sizeof(*path->points))
        return BENCH_PATH_TOO_LARGE;
    if (requested <= path->capacity)
        return BENCH_PATH_OK;
    size_t capacity = path->capacity ? path->capacity : INITIAL_PATH_CAPACITY;
    while (capacity < requested)
    {
        if (capacity > SIZE_MAX / sizeof(*path->points) / 2)
        {
            capacity = requested;
            break;
        }
        capacity *= 2;
    }
    WirePoint *points = realloc(path->points, capacity * sizeof(*points));
    if (!points)
        return BENCH_PATH_NO_MEMORY;
    path->points = points;
    path->capacity = capacity;
    return BENCH_PATH_OK;
}

BenchPathStatus bench_path_clone(BenchPath *destination, const BenchPath *source)
{
    if (!destination || !source)
        return BENCH_PATH_INVALID;
    if (destination == source)
        return BENCH_PATH_OK;
    BenchPath copy = {0};
    BenchPathStatus status = bench_path_reserve(&copy, source->count);
    if (status != BENCH_PATH_OK)
        return status;
    if (source->count)
        memcpy(copy.points, source->points, source->count * sizeof(*copy.points));
    copy.count = source->count;
    bench_path_clear(destination);
    *destination = copy;
    return BENCH_PATH_OK;
}

BenchPathStatus bench_path_insert(BenchPath *path, size_t index, WirePoint point)
{
    if (!path || !point_valid(point))
        return BENCH_PATH_INVALID;
    if (index > path->count)
        return BENCH_PATH_RANGE;
    if (path->count == SIZE_MAX)
        return BENCH_PATH_TOO_LARGE;
    BenchPathStatus status = bench_path_reserve(path, path->count + 1);
    if (status != BENCH_PATH_OK)
        return status;
    memmove(path->points + index + 1, path->points + index, (path->count - index) * sizeof(*path->points));
    path->points[index] = point;
    ++path->count;
    return BENCH_PATH_OK;
}

BenchPathStatus bench_path_move(BenchPath *path, size_t index, WirePoint point)
{
    if (!path || !point_valid(point))
        return BENCH_PATH_INVALID;
    if (index >= path->count)
        return BENCH_PATH_RANGE;
    path->points[index] = point;
    return BENCH_PATH_OK;
}

BenchPathStatus bench_path_remove(BenchPath *path, size_t index)
{
    if (!path)
        return BENCH_PATH_INVALID;
    if (index >= path->count)
        return BENCH_PATH_RANGE;
    memmove(path->points + index, path->points + index + 1, (path->count - index - 1) * sizeof(*path->points));
    --path->count;
    return BENCH_PATH_OK;
}

double bench_path_length(const BenchPath *path)
{
    double length = 0;
    if (!path)
        return length;
    for (size_t i = 1; i < path->count; ++i)
        length += hypot((double)path->points[i].x - path->points[i - 1].x,
                        (double)path->points[i].y - path->points[i - 1].y);
    return length;
}

const char *bench_path_status_message(BenchPathStatus status)
{
    switch (status)
    {
    case BENCH_PATH_OK:
        return "Caminho atualizado.";
    case BENCH_PATH_RANGE:
        return "Ponto fora do caminho.";
    case BENCH_PATH_TOO_LARGE:
        return "Caminho excede o tamanho representavel na memoria.";
    case BENCH_PATH_NO_MEMORY:
        return "Memoria insuficiente; caminho anterior preservado.";
    default:
        return "Caminho ou coordenada invalida.";
    }
}
