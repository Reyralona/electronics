#include "component_registry.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Preserve the existing model ranges; these are data per parameter, not physical ratings. */
static const ComponentParameter voltage = {"voltage", "V", 5.0, 1e-12, 1e12};
static const ComponentParameter resistance = {"resistance", "ohm", 1000.0, 1e-12, 1e12};
static const ComponentParameter capacitance = {"capacitance", "F", 1e-6, 1e-12, 1e12};
static const ComponentParameter inductance = {"inductance", "H", .01, 1e-12, 1e12};
static const ComponentParameter frequency = {"frequency", "Hz", 1000.0, 1e-12, 1e12};

static const ComponentPinInfo pins_source[] = {{"V+", PIN_SUPPLY_POSITIVE}, {"V-", PIN_SUPPLY_RETURN}};
static const ComponentPinInfo pins_passive[] = {{"1", PIN_PASSIVE}, {"2", PIN_PASSIVE}};
static const ComponentPinInfo pins_pot[] = {{"A", PIN_PASSIVE}, {"W", PIN_PASSIVE}, {"B", PIN_PASSIVE}};
static const ComponentPinInfo pins_led[] = {{"A", PIN_ANODE}, {"K", PIN_CATHODE}};
static const ComponentPinInfo pins_gate[] = {{"A", PIN_INPUT}, {"B", PIN_INPUT}, {"Y", PIN_OUTPUT}};
static const ComponentPinInfo pins_not[] = {{"A", PIN_INPUT}, {"Y", PIN_OUTPUT}};
static const ComponentPinInfo pins_clock[] = {{"OUT", PIN_OUTPUT}, {"REF", PIN_SUPPLY_RETURN}};
static const ComponentPinInfo pins_dff[] = {{"D", PIN_INPUT}, {"CLK", PIN_CLOCK_INPUT}, {"Q", PIN_OUTPUT}};
/* TI SCLS181H, table 5-1, PDIP top view; checked 2026-11-15. */
static const ComponentPinInfo pins_74hc00[] = {
    {"1A", PIN_INPUT},  {"1B", PIN_INPUT},   {"1Y", PIN_OUTPUT}, {"2A", PIN_INPUT},           {"2B", PIN_INPUT},
    {"2Y", PIN_OUTPUT}, {"GND", PIN_GROUND}, {"3Y", PIN_OUTPUT}, {"3A", PIN_INPUT},           {"3B", PIN_INPUT},
    {"4Y", PIN_OUTPUT}, {"4A", PIN_INPUT},   {"4B", PIN_INPUT},  {"VCC", PIN_SUPPLY_POSITIVE}};
static const ComponentPinInfo board_contact = {"Contato", PIN_CONTACT};

/* Enum indexes remain the legacy file codes; id is the durable external identity. */
static const ComponentInfo components[] = {
    [PART_SOURCE] = {PART_SOURCE, "source", "Fonte", 2,
                     COMPONENT_ROTATES | COMPONENT_STARTS_ON | COMPONENT_CONNECTIVITY, 1, &voltage, pins_source},
    [PART_SWITCH] = {PART_SWITCH, "switch", "Interruptor", 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 1, NULL,
                     pins_passive},
    [PART_LED] = {PART_LED, "led", "LED", 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 1, NULL, pins_led},
    [PART_BREADBOARD] = {PART_BREADBOARD, "breadboard", "Breadboard", BREADBOARD_PINS, COMPONENT_CONNECTIVITY, 1, NULL,
                         NULL},
    [PART_RESISTOR] = {PART_RESISTOR, "resistor", "Resistor", 2, COMPONENT_ROTATES, 2, &resistance, pins_passive},
    [PART_CAPACITOR] = {PART_CAPACITOR, "capacitor", "Capacitor", 2, COMPONENT_ROTATES, 2, &capacitance, pins_passive},
    [PART_INDUCTOR] = {PART_INDUCTOR, "inductor", "Indutor", 2, COMPONENT_ROTATES, 3, &inductance, pins_passive},
    [PART_AND] = {PART_AND, "and", "Porta AND", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 11, NULL, pins_gate},
    [PART_NAND] = {PART_NAND, "nand", "Porta NAND", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_gate},
    [PART_OR] = {PART_OR, "or", "Porta OR", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_gate},
    [PART_XOR] = {PART_XOR, "xor", "Porta XOR", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_gate},
    [PART_NOT] = {PART_NOT, "not", "Porta NOT", 2, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 12, NULL, pins_not},
    [PART_CLOCK] = {PART_CLOCK, "clock", "Clock", 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 13, &frequency,
                    pins_clock},
    [PART_DFF] = {PART_DFF, "dff", "Bloco DFF", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 14, NULL, pins_dff},
    [PART_NOR] = {PART_NOR, "nor", "Porta NOR", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 21, NULL, pins_gate},
    [PART_XNOR] = {PART_XNOR, "xnor", "Porta XNOR", 3, COMPONENT_ROTATES | COMPONENT_GATE_SYMBOL, 21, NULL, pins_gate},
    [PART_POTENTIOMETER] = {PART_POTENTIOMETER, "potentiometer", "Potenciometro", 3, COMPONENT_ROTATES, 21, &resistance, pins_pot},
    [PART_BUTTON] = {PART_BUTTON, "button", "Botao", 2, COMPONENT_ROTATES | COMPONENT_CONNECTIVITY, 21, NULL, pins_passive},
    [PART_74HC08] = {PART_74HC08, "74hc08", "CI 74HC08", 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE, 22, NULL, pins_74hc00},
    [PART_74HC32] = {PART_74HC32, "74hc32", "CI 74HC32", 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE, 22, NULL, pins_74hc00},
    [PART_74HC86] = {PART_74HC86, "74hc86", "CI 74HC86", 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE, 22, NULL, pins_74hc00},
    [PART_74HC00] = {PART_74HC00, "74hc00", "CI 74HC00", 14, COMPONENT_ROTATES | COMPONENT_QUAD_GATE, 15, NULL, pins_74hc00},
};

/* Historical integer codes are fixed by file versions 1-16, independent of enum order. */
static const PartType legacy_types[] = {PART_SOURCE,    PART_SWITCH,   PART_LED,   PART_BREADBOARD, PART_RESISTOR,
                                        PART_CAPACITOR, PART_INDUCTOR, PART_AND,   PART_NAND,       PART_OR,
                                        PART_XOR,       PART_NOT,      PART_CLOCK, PART_DFF,        PART_74HC00, PART_NOR, PART_XNOR, PART_POTENTIOMETER, PART_BUTTON, PART_74HC08, PART_74HC32, PART_74HC86};

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

const ComponentPinInfo *component_pin_info(PartType type, int pin)
{
    const ComponentInfo *info = component_info(type);
    if (!info || pin < 0 || pin >= info->pin_count)
        return NULL;
    return info->pins ? &info->pins[pin] : &board_contact;
}

const char *component_pin_role_label(ComponentPinRole role)
{
    static const char *labels[] = {"Sem polaridade / bidirecional",
                                   "Entrada",
                                   "Entrada de clock",
                                   "Saida",
                                   "Alimentacao positiva",
                                   "Retorno / referencia",
                                   "Terra (GND)",
                                   "Anodo",
                                   "Catodo",
                                   "Contato sem polaridade propria"};
    if ((unsigned)role >= sizeof(labels) / sizeof(labels[0]))
        return "Funcao nao definida";
    return labels[role];
}

const char *component_pin_marker(ComponentPinRole role)
{
    static const char *marks[] = {"IO", "IN", "CLK", "OUT", "V+", "V-", "GND", "A", "K", "CONT"};
    if ((unsigned)role >= sizeof(marks) / sizeof(marks[0]))
        return "?";
    return marks[role];
}

PartType component_electrical_type(PartType type)
{
    return type;
}

double component_effective_value(const Part *part)
{
    return part->value;
}

int component_effective_on(const Part *part)
{
    return part->on;
}

double component_default_control(PartType type)
{
    (void)type;
    return .5;
}

int component_current_vector(PartType type, int index, char *vector, size_t capacity)
{
    if (type == PART_LED)
        snprintf(vector, capacity, "@d%d[id]", index);
    else if (component_electrical_type(type) == PART_RESISTOR)
        snprintf(vector, capacity, "@r%d[i]", index);
    else
        return 0;
    return 1;
}
