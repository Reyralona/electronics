#ifndef BENCH_PATH_H
#define BENCH_PATH_H
#include <stddef.h>
#include "workbench.h"

typedef enum
{
    BENCH_PATH_OK,
    BENCH_PATH_INVALID,
    BENCH_PATH_RANGE,
    BENCH_PATH_TOO_LARGE,
    BENCH_PATH_NO_MEMORY
} BenchPathStatus;

/* Initialize to {0}. Owns points: use clone instead of assignment; clear before destruction.
 * Every failing mutation preserves the existing path. Successful growth invalidates point pointers. */
typedef struct
{
    WirePoint *points;
    size_t count;
    size_t capacity;
} BenchPath;

struct BenchTerminals
{
    size_t pin_count;
    BenchPath *paths;
};

typedef struct
{
    size_t segment; /* Index of the first vertex of the selected segment. */
    WirePoint point;
    double distance_squared;
} BenchPathHit;

/* Non-owning query; accepts dynamic or stack-backed points. Miss/invalid input preserves hit. */
int bench_polyline_pick(const WirePoint *points, size_t count, WirePoint point, double tolerance, BenchPathHit *hit);
void bench_path_clear(BenchPath *path);
BenchPathStatus bench_path_reserve(BenchPath *path, size_t capacity);
BenchPathStatus bench_path_clone(BenchPath *destination, const BenchPath *source);
BenchPathStatus bench_path_insert(BenchPath *path, size_t index, WirePoint point);
BenchPathStatus bench_path_move(BenchPath *path, size_t index, WirePoint point);
BenchPathStatus bench_path_remove(BenchPath *path, size_t index);
double bench_path_length(const BenchPath *path);
const char *bench_path_status_message(BenchPathStatus status);
#endif
