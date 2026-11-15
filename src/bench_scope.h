#ifndef BENCH_SCOPE_H
#define BENCH_SCOPE_H
#include "workbench.h"
/* The Windows build uses GCC. Cleanup keeps snapshot ownership safe across early returns. */
#if defined(__GNUC__)
#define BENCH_AUTO_CLEAR __attribute__((cleanup(bench_snapshot_clear)))
#else
#error "Snapshot scope cleanup requires GCC-compatible cleanup support."
#endif
#endif
