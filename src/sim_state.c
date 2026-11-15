#include "sim_state.h"

Pin sim_state_probe(const Workbench *bench, int selected, int probe_pin)
{
    int i;
    if (selected >= 0 && selected < bench->part_count && bench->parts[selected].type != PART_BREADBOARD &&
        probe_pin < bench_pin_count(bench->parts[selected].type))
        return (Pin){selected, probe_pin};
    for (i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == PART_SOURCE || bench->parts[i].type == PART_CLOCK)
            return (Pin){i, 0};
    for (i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type != PART_BREADBOARD)
            return (Pin){i, 0};
    return (Pin){0, 0};
}
