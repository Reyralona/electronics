#include "component_registry.h"
#include <math.h>
#include <string.h>

/* Preserve the existing model ranges; these are data per parameter, not physical ratings. */
static const ComponentParameter voltage = {"voltage", "V", 5.0, 1e-12, 1e12};
static const ComponentParameter resistance = {"resistance", "ohm", 1000.0, 1e-12, 1e12};
static const ComponentParameter capacitance = {"capacitance", "F", 1e-6, 1e-12, 1e12};
static const ComponentParameter inductance = {"inductance", "H", .01, 1e-12, 1e12};
static const ComponentParameter frequency = {"frequency", "Hz", 1000.0, 1e-12, 1e12};

/* Enum indexes remain the legacy file codes; id is the durable external identity. */
static const ComponentInfo components[] = {
    [PART_SOURCE] = {PART_SOURCE, "source", "Fonte", 2,
                     COMPONENT_ROTATES | COMPONENT_STARTS_ON | COMPONENT_CONNECTIVITY, 1, &voltage},
    [PART_SWITCH] = {PART_SWITCH, "switch", "Interruptor", 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 1, NULL},
    [PART_LED] = {PART_LED, "led", "LED", 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 1, NULL},
    [PART_BREADBOARD] = {PART_BREADBOARD, "breadboard", "Breadboard", BREADBOARD_PINS, COMPONENT_CONNECTIVITY, 1, NULL},
    [PART_RESISTOR] = {PART_RESISTOR, "resistor", "Resistor", 2, COMPONENT_ROTATES, 2, &resistance},
    [PART_CAPACITOR] = {PART_CAPACITOR, "capacitor", "Capacitor", 2, COMPONENT_ROTATES, 2, &capacitance},
    [PART_INDUCTOR] = {PART_INDUCTOR, "inductor", "Indutor", 2, COMPONENT_ROTATES, 3, &inductance},
    [PART_AND] = {PART_AND, "and", "Porta AND", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 11, NULL},
    [PART_NAND] = {PART_NAND, "nand", "Porta NAND", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL},
    [PART_OR] = {PART_OR, "or", "Porta OR", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL},
    [PART_XOR] = {PART_XOR, "xor", "Porta XOR", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL},
    [PART_NOT] = {PART_NOT, "not", "Porta NOT", 2, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL},
    [PART_CLOCK] = {PART_CLOCK, "clock", "Clock", 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 13, &frequency},
    [PART_DFF] = {PART_DFF, "dff", "Bloco DFF", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 14, NULL},
    [PART_74HC00] = {PART_74HC00, "74hc00", "CI 74HC00", 14, COMPONENT_ROTATES, 15, NULL},
};

/* Historical integer codes are fixed by file versions 1-16, independent of enum order. */
static const PartType legacy_types[] = {PART_SOURCE,    PART_SWITCH,   PART_LED,   PART_BREADBOARD, PART_RESISTOR,
                                        PART_CAPACITOR, PART_INDUCTOR, PART_AND,   PART_NAND,       PART_OR,
                                        PART_XOR,       PART_NOT,      PART_CLOCK, PART_DFF,        PART_74HC00};

size_t component_registry_count(void)
{
    return sizeof(components) / sizeof(components[0]);
}

const ComponentInfo *component_info(PartType type)
{
    if ((unsigned)type >= component_registry_count() || !components[type].id)
        return NULL;
    return &components[type];
}

const ComponentInfo *component_from_legacy(int code, int version)
{
    if ((unsigned)code >= sizeof(legacy_types) / sizeof(legacy_types[0]))
        return NULL;
    const ComponentInfo *info = component_info(legacy_types[code]);
    return info && version >= info->first_file_version ? info : NULL;
}

const ComponentInfo *component_by_id(const char *id)
{
    if (!id)
        return NULL;
    for (size_t i = 0; i < component_registry_count(); ++i)
        if (components[i].id && !strcmp(components[i].id, id))
            return &components[i];
    return NULL;
}

int component_value_valid(PartType type, double value)
{
    const ComponentInfo *info = component_info(type);
    return info && info->primary && isfinite(value) && value >= info->primary->minimum &&
           value <= info->primary->maximum;
}

int component_has_flag(PartType type, unsigned flag)
{
    const ComponentInfo *info = component_info(type);
    return info && (info->flags & flag) == flag;
}

int component_legacy_supported(PartType type, int version)
{
    const ComponentInfo *info = component_info(type);
    return info && version >= info->first_file_version;
}
