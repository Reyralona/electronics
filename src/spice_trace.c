#include "spice_trace.h"
#include <math.h>

int spice_trace_replay_index(const SpiceTrace *trace, int start_index, uint64_t elapsed_ms, int *finished)
{
    int low, high;
    double first, last, target;
    if (finished)
        *finished = 0;
    if (!trace || !finished || trace->count < 2 || trace->count > SPICE_TRANSIENT_MAX_SAMPLES || start_index < 0 ||
        start_index >= trace->count)
        return -1;
    first = trace->samples[0].time;
    last = trace->samples[trace->count - 1].time;
    if (!isfinite(first) || !isfinite(last) || last <= first || !isfinite(trace->samples[start_index].time))
        return -1;
    target = trace->samples[start_index].time + (last - first) * ((double)elapsed_ms / 5000.0);
    if (target >= last)
    {
        *finished = 1;
        return trace->count - 1;
    }
    low = start_index;
    high = trace->count - 1;
    while (low < high)
    {
        int mid = low + (high - low + 1) / 2;
        if (trace->samples[mid].time <= target + (last - first) * 1e-12)
            low = mid;
        else
            high = mid - 1;
    }
    return low;
}

int spice_trace_nearest(const SpiceTrace *trace, double fraction)
{
    double target, best;
    int i, index = 0;
    if (fraction < 0)
        fraction = 0;
    if (fraction > 1)
        fraction = 1;
    target = trace->samples[0].time + (trace->samples[trace->count - 1].time - trace->samples[0].time) * fraction;
    best = fabs(trace->samples[0].time - target);
    for (i = 1; i < trace->count; ++i)
    {
        double distance = fabs(trace->samples[i].time - target);
        if (distance < best)
        {
            best = distance;
            index = i;
        }
    }
    return index;
}
