#include "bench_path.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    BenchPath path = {0}, copy = {0};
    assert(bench_path_insert(&path, 0, (WirePoint){0, 0}) == BENCH_PATH_OK);
    assert(bench_path_insert(&path, 1, (WirePoint){6, 8}) == BENCH_PATH_OK);
    assert(bench_path_insert(&path, 1, (WirePoint){3, 4}) == BENCH_PATH_OK);
    assert(bench_path_length(&path) == 10);
    assert(bench_path_clone(&copy, &path) == BENCH_PATH_OK);
    assert(copy.points != path.points && copy.count == 3);
    assert(bench_path_move(&copy, 1, (WirePoint){3, 0}) == BENCH_PATH_OK);
    assert(path.points[1].y == 4 && copy.points[1].y == 0);
    assert(bench_path_remove(&copy, 1) == BENCH_PATH_OK);
    assert(copy.count == 2 && copy.points[1].x == 6 && bench_path_length(&copy) == 10);

    WirePoint *original = path.points;
    size_t count = path.count, capacity = path.capacity;
    assert(bench_path_reserve(&path, SIZE_MAX) == BENCH_PATH_TOO_LARGE);
    assert(path.points == original && path.count == count && path.capacity == capacity);
    assert(bench_path_insert(&path, 4, (WirePoint){1, 1}) == BENCH_PATH_RANGE);
    assert(bench_path_move(&path, 1, (WirePoint){NAN, 0}) == BENCH_PATH_INVALID);
    assert(bench_path_remove(&path, 3) == BENCH_PATH_RANGE);
    assert(path.points[1].y == 4 && path.count == count);
    assert(bench_path_clone(&path, &path) == BENCH_PATH_OK && path.points == original);

    /* Growth exceeds both the current wire limit and every initial allocation. */
    for (size_t i = path.count; i < 4096; ++i)
        assert(bench_path_insert(&path, path.count, (WirePoint){(float)i, 0}) == BENCH_PATH_OK);
    assert(path.count == 4096 && path.points[4095].x == 4095);
    assert(bench_path_clone(&copy, &path) == BENCH_PATH_OK);
    bench_path_clear(&path);
    assert(!path.points && !path.count && !path.capacity);
    assert(copy.count == 4096 && copy.points[4095].x == 4095);
    assert(bench_path_clone(&copy, &path) == BENCH_PATH_OK);
    assert(!copy.points && !copy.count && !copy.capacity);
    bench_path_clear(&copy);
    bench_path_clear(&copy);
    assert(strstr(bench_path_status_message(BENCH_PATH_NO_MEMORY), "preservado"));
    WirePoint crossing[] = {{0, 0}, {10, 10}, {0, 10}, {10, 0}};
    BenchPathHit hit = {0};
    assert(bench_polyline_pick(crossing, 4, (WirePoint){5, 5}, .1, &hit));
    assert(hit.segment == 0 && hit.point.x == 5 && hit.point.y == 5);
    assert(bench_polyline_pick(crossing, 4, (WirePoint){8, 2}, .1, &hit));
    assert(hit.segment == 2);
    BenchPathHit before = hit;
    assert(!bench_polyline_pick(crossing, 4, (WirePoint){100, 100}, 1, &hit));
    assert(hit.segment == before.segment && hit.point.x == before.point.x);
    crossing[3].x = INFINITY;
    assert(!bench_polyline_pick(crossing, 4, (WirePoint){5, 5}, 1, &hit));
    assert(hit.segment == before.segment && hit.point.x == before.point.x);
    return 0;
}
