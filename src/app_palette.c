#include "app_palette.h"
#include "component_registry.h"
#include "ui_text.h"

/* Page indices are stable: 0 basics, 1 digital, 2 chips (packing), 3 sensors, 4 outputs, 5 instruments. */
static const PartType basic[] = {PART_SOURCE,    PART_SWITCH,     PART_LED,        PART_BREADBOARD, PART_RESISTOR,
                                 PART_CAPACITOR, PART_INDUCTOR,   PART_AND,        PART_BUTTON,     PART_POTENTIOMETER,
                                 PART_DIODE,     PART_NPN,        PART_PNP,        PART_BATTERY_9V, PART_BATTERY_AA,
                                 PART_BATTERY_COIN, PART_DPST,    PART_DIP_SWITCH, PART_RELAY};
static const PartType digital[] = {PART_AND, PART_NAND,   PART_OR,  PART_XOR,    PART_NOT,    PART_CLOCK, PART_DFF,
                                   PART_74HC00, PART_NOR, PART_XNOR, PART_74HC08, PART_74HC32, PART_74HC86,
                                   PART_ARDUINO_UNO, PART_ESP32, PART_ATTINY85, PART_LCD_I2C, PART_JUNCTION};
static const PartType sensors[] = {PART_LDR,  PART_FLEX, PART_FORCE,       PART_TILT,    PART_PHOTODIODE, PART_AMBIENT_LIGHT,
                                   PART_TEMPERATURE, PART_SOIL, PART_PIR, PART_GAS, PART_IR_OBSTACLE, PART_DISTANCE};
static const PartType outputs[] = {PART_LAMP,  PART_RGB_LED, PART_SEVEN_SEGMENT, PART_DC_MOTOR,
                                   PART_VIBRATION_MOTOR, PART_ENCODER_MOTOR, PART_SERVO, PART_PIEZO};
static const PartType instruments[] = {PART_MULTIMETER, PART_OSCILLOSCOPE, PART_FUNCTION_GENERATOR};

#define COUNT(list) ((int)(sizeof(list) / sizeof(list[0])))

int is_gate(PartType type)
{
    return component_has_flag(type, COMPONENT_GATE_SYMBOL);
}

int palette_count(int page)
{
    static const int counts[] = {COUNT(basic), COUNT(digital), 8, COUNT(sensors), COUNT(outputs), COUNT(instruments)};
    return page >= 0 && page < PALETTE_PAGES ? counts[page] : 0;
}

PartType palette_part(int page, int item)
{
    static const PartType *lists[] = {basic, digital, NULL, sensors, outputs, instruments};
    if (page < 0 || page >= PALETTE_PAGES || !lists[page] || item < 0 || item >= palette_count(page))
        return PART_RESISTOR;
    return lists[page][item];
}

const char *palette_page_name(int page)
{
    static const TextId names[PALETTE_PAGES] = {TXT_UI_PAGE_BASICS,  TXT_UI_PAGE_DIGITAL, TXT_UI_PAGE_CHIPS,
                                                TXT_UI_PAGE_SENSORS, TXT_UI_PAGE_OUTPUTS, TXT_UI_PAGE_METERS};
    return page >= 0 && page < PALETTE_PAGES ? tr(names[page]) : "";
}

const char *part_name(PartType type)
{
    const ComponentInfo *info = component_info(type);
    return info ? info->display_name : tr(TXT_UI_UNKNOWN_PART);
}
