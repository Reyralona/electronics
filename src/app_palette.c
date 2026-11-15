#include "app_palette.h"

int is_gate(PartType type)
{
    return (type >= PART_AND && type <= PART_NOT) || type == PART_DFF;
}
PartType palette_part(int page, int item)
{
    static const PartType digital[] = {PART_AND, PART_NAND,  PART_OR,  PART_XOR,
                                       PART_NOT, PART_CLOCK, PART_DFF, PART_74HC00};
    return page ? digital[item] : (PartType)item;
}
const char *part_name(PartType type)
{
    static const char *names[] = {"Fonte",     "Interruptor", "LED",       "Breadboard", "Resistor",
                                  "Capacitor", "Indutor",     "Porta AND", "Porta NAND", "Porta OR",
                                  "Porta XOR", "Porta NOT",   "Clock",     "Bloco DFF",  "CI 74HC00"};
    return names[type];
}
