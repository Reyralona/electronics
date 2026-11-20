#include "component_registry.h"
#include "component_devices.h"
#include "ui_text.h"
#include <assert.h>
#include <math.h>
#include <string.h>

/* Display texts follow the interface language; persistent ids and pin symbols do not. */
static void check_localized_texts(void)
{
    const ComponentInfo *source = component_by_id("source");
    assert(!strcmp(source->display_name, tr(TXT_CAT_SOURCE)));
    assert(!strcmp(component_pin_info(PART_SERVO, 2)->name, tr(TXT_CAT_PIN_SIGNAL)));
    ui_text_set_language(UI_LANG_EN_US);
    assert(!strcmp(component_by_id("source")->display_name, tr_in(UI_LANG_EN_US, TXT_CAT_SOURCE)));
    assert(!strcmp(component_info(PART_SOURCE)->id, "source"));
    assert(!strcmp(component_pin_info(PART_BREADBOARD, 0)->name, tr_in(UI_LANG_EN_US, TXT_CAT_PIN_CONTACT)));
    assert(!strcmp(component_pin_info(PART_74HC00, 7)->name, "3Y"));
    assert(!strcmp(component_environment(PART_LDR)->note, tr_in(UI_LANG_EN_US, TXT_CAT_ENV_LIGHT_NOTE)));
    assert(!strcmp(component_mode_label(PART_MULTIMETER, 2), tr_in(UI_LANG_EN_US, TXT_CAT_METER_RESISTANCE)));
    assert(!strcmp(component_element_label(PART_SEVEN_SEGMENT, 7), "dp"));
    assert(!component_element_label(PART_ARDUINO_UNO, 0));
    assert(!strcmp(component_symbol(PART_DFF), "DFF") && !strcmp(component_symbol(PART_74HC00), "74HC00"));
    assert(!component_symbol(PART_RESISTOR));
    ui_text_set_language(UI_LANG_PT_BR);
    assert(!strcmp(source->display_name, tr_in(UI_LANG_PT_BR, TXT_CAT_SOURCE)));
}

int main(void)
{
    check_localized_texts();
    for (int type = 0; type < PART_TYPE_COUNT; ++type)
    {
        const ComponentInfo *info = component_info((PartType)type);
        assert(info && component_from_legacy(type, 38) == info);
        assert(component_by_id(info->id) == info);
    }
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
