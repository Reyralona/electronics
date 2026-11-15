#include "component_registry.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int main(void)
{
    /* This is the historical file contract, not the current enum's declaration order. */
    static const char *legacy_ids[] = {"source",    "switch",   "led",   "breadboard", "resistor",
                                       "capacitor", "inductor", "and",   "nand",       "or",
                                       "xor",       "not",      "clock", "dff",        "74hc00"};
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
    assert(component_pin_info(PART_LED, 0)->role == PIN_ANODE);
    assert(component_pin_info(PART_LED, 1)->role == PIN_CATHODE);
    assert(component_pin_info(PART_RESISTOR, 0)->role == PIN_PASSIVE);
    assert(component_pin_info(PART_DFF, 1)->role == PIN_CLOCK_INPUT);
    assert(!strcmp(component_pin_info(PART_74HC00, 7)->name, "3Y"));
    assert(component_pin_info(PART_74HC00, 7)->role == PIN_OUTPUT);
    assert(component_pin_info(PART_74HC00, 8)->role == PIN_INPUT);
    assert(component_pin_info(PART_74HC00, 6)->role == PIN_GROUND);
    assert(component_pin_info(PART_74HC00, 13)->role == PIN_SUPPLY_POSITIVE);
    assert(component_pin_info(PART_BREADBOARD, 419)->role == PIN_CONTACT);
    assert(!component_pin_info(PART_74HC00, 14) && !component_pin_info(PART_LED, -1));
    return 0;
}
