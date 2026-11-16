#include "app_palette.h"
#include "component_registry.h"

int is_gate(PartType type)
{
    return component_has_flag(type, COMPONENT_GATE_SYMBOL);
}
int palette_count(int page)
{
    return page == 2 ? 8 : page == 1 ? 13 : 24;
}

PartType palette_part(int page, int item)
{
    static const PartType digital[] = {PART_AND, PART_NAND,  PART_OR,  PART_XOR,
                                       PART_NOT, PART_CLOCK, PART_DFF, PART_74HC00, PART_NOR, PART_XNOR, PART_74HC08, PART_74HC32, PART_74HC86};
    static const PartType basic[] = {PART_SOURCE, PART_SWITCH, PART_LED, PART_BREADBOARD, PART_RESISTOR,
                                     PART_CAPACITOR, PART_INDUCTOR, PART_AND, PART_BUTTON, PART_POTENTIOMETER, PART_DIODE, PART_LDR, PART_FLEX, PART_FORCE, PART_TILT, PART_NPN, PART_PNP, PART_BATTERY_9V, PART_BATTERY_AA, PART_BATTERY_COIN, PART_PHOTODIODE, PART_AMBIENT_LIGHT, PART_TEMPERATURE, PART_SOIL};
    return page ? digital[item] : basic[item];
}
const char *part_name(PartType type)
{
    const ComponentInfo *info = component_info(type);
    return info ? info->display_name : "Componente desconhecido";
}
