#include "component_registry.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int main(void)
{
    /* This is the historical file contract, not the current enum's declaration order. */
    static const char *legacy_ids[] = {"source", "switch", "led", "breadboard", "resistor", "capacitor",
                                       "inductor", "and", "nand", "or", "xor", "not", "clock", "dff", "74hc00"};
    for (int code = 0; code < 15; ++code)
    {
        const ComponentInfo *info = component_from_legacy(code, 16);
        assert(info && !strcmp(info->id, legacy_ids[code]));
        assert(component_by_id(legacy_ids[code]) == info);
        assert(component_info(info->type) == info);
        assert(info->pin_count > 0 && info->pin_count <= WORKBENCH_MAX_PINS);
        assert(!component_from_legacy(code, info->first_file_version - 1));
        for (int other = 0; other < code; ++other)
            assert(strcmp(info->id, component_from_legacy(other, 16)->id));
    }
    assert(!component_info((PartType)-1) && !component_info((PartType)1000));
    assert(!component_by_id(NULL) && !component_by_id("unknown-device"));
    assert(!component_from_legacy(-1, 16) && !component_from_legacy(1000, 16));
    assert(!component_value_valid(PART_RESISTOR, NAN));
    assert(!component_value_valid(PART_RESISTOR, INFINITY));
    assert(!component_value_valid(PART_LED, 10));
    assert(component_value_valid(PART_RESISTOR, 2200));
    assert(component_has_flag(PART_SOURCE, COMPONENT_STARTS_ON));
    assert(!component_has_flag(PART_BREADBOARD, COMPONENT_ROTATES));
    assert(component_has_flag(PART_DFF, COMPONENT_GATE_SYMBOL));
    assert(!component_has_flag(PART_74HC00, COMPONENT_GATE_SYMBOL));
    return 0;
}
