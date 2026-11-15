#include "app_palette.h"
#include "component_registry.h"

int is_gate(PartType type)
{
    return component_has_flag(type, COMPONENT_GATE_SYMBOL);
}
PartType palette_part(int page, int item)
{
    static const PartType digital[] = {PART_AND, PART_NAND,  PART_OR,  PART_XOR,
                                       PART_NOT, PART_CLOCK, PART_DFF, PART_74HC00};
    return page ? digital[item] : (PartType)item;
}
const char *part_name(PartType type)
{
    const ComponentInfo *info = component_info(type);
    return info ? info->display_name : "Componente desconhecido";
}
