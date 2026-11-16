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
static const ComponentParameter battery_9v = {"voltage", "V", 9, 1e-12, 1e12};
static const ComponentParameter battery_aa = {"voltage", "V", 1.5, 1e-12, 1e12};
static const ComponentParameter battery_coin = {"voltage", "V", 3, 1e-12, 1e12};
static const ComponentParameter flex_resistance = {"resistance", "ohm", 25000, 1, 1e9};
static const ComponentParameter force_resistance = {"resistance", "ohm", 15000, 1, 1e9};
static const ComponentPinInfo pins_bjt[] = {{"E", PIN_EMITTER}, {"B", PIN_BASE}, {"C", PIN_COLLECTOR}};
static const ComponentPinInfo pins_photo_transistor[] = {{"C", PIN_COLLECTOR}, {"E", PIN_EMITTER}};
static const ComponentPinInfo pins_sensor[] = {{"V+", PIN_SUPPLY_POSITIVE}, {"OUT", PIN_OUTPUT}, {"GND", PIN_GROUND}};
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
    [PART_DIODE] = {PART_DIODE, "diode", "Diodo de silicio", 2, COMPONENT_ROTATES, 24, NULL, pins_led},
    [PART_LDR] = {PART_LDR, "ldr", "Fotoresistor", 2, COMPONENT_ROTATES, 24, &force_resistance, pins_passive},
    [PART_FLEX] = {PART_FLEX, "flex", "Sensor flexivel", 2, COMPONENT_ROTATES, 24, &flex_resistance, pins_passive},
    [PART_FORCE] = {PART_FORCE, "force", "Sensor de forca", 2, COMPONENT_ROTATES, 24, &force_resistance, pins_passive},
    [PART_TILT] = {PART_TILT, "tilt", "Sensor de inclinacao", 2, COMPONENT_ROTATES, 24, NULL, pins_passive},
    [PART_NPN] = {PART_NPN, "npn", "Transistor NPN", 3, COMPONENT_ROTATES, 24, NULL, pins_bjt},
    [PART_PNP] = {PART_PNP, "pnp", "Transistor PNP", 3, COMPONENT_ROTATES, 24, NULL, pins_bjt},
    [PART_BATTERY_9V] = {PART_BATTERY_9V, "battery-9v", "Bateria 9 V", 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 24, &battery_9v, pins_source},
    [PART_BATTERY_AA] = {PART_BATTERY_AA, "battery-aa", "Pilha AA 1.5 V", 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 24, &battery_aa, pins_source},
    [PART_BATTERY_COIN] = {PART_BATTERY_COIN, "battery-coin", "Moeda 3 V", 2, COMPONENT_ROTATES | COMPONENT_STARTS_ON, 24, &battery_coin, pins_source},
    [PART_PHOTODIODE] = {PART_PHOTODIODE, "photodiode", "Fotodiodo", 2, COMPONENT_ROTATES, 24, NULL, pins_led},
    [PART_AMBIENT_LIGHT] = {PART_AMBIENT_LIGHT, "ambient-light", "Fototransistor", 2, COMPONENT_ROTATES, 24, NULL, pins_photo_transistor},
    [PART_TEMPERATURE] = {PART_TEMPERATURE, "temperature", "Sensor TMP36", 3, COMPONENT_ROTATES, 24, NULL, pins_sensor},
    [PART_SOIL] = {PART_SOIL, "soil", "Umidade do solo", 3, COMPONENT_ROTATES, 24, NULL, pins_sensor},
};

/* Historical integer codes are fixed by file versions 1-16, independent of enum order. */
static const PartType legacy_types[] = {PART_SOURCE,    PART_SWITCH,   PART_LED,   PART_BREADBOARD, PART_RESISTOR,
                                        PART_CAPACITOR, PART_INDUCTOR, PART_AND,   PART_NAND,       PART_OR,
                                        PART_XOR,       PART_NOT,      PART_CLOCK, PART_DFF,        PART_74HC00, PART_NOR, PART_XNOR, PART_POTENTIOMETER, PART_BUTTON, PART_74HC08, PART_74HC32, PART_74HC86, PART_DIODE, PART_LDR, PART_FLEX, PART_FORCE, PART_TILT, PART_NPN, PART_PNP, PART_BATTERY_9V, PART_BATTERY_AA, PART_BATTERY_COIN, PART_PHOTODIODE, PART_AMBIENT_LIGHT, PART_TEMPERATURE, PART_SOIL};

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
                                   "Contato sem polaridade propria",
                                   "Coletor (modelo NPN/PNP)",
                                   "Base (controle do transistor)",
                                   "Emissor (modelo NPN/PNP)"};
    if ((unsigned)role >= sizeof(labels) / sizeof(labels[0]))
        return "Funcao nao definida";
    return labels[role];
}

const char *component_pin_marker(ComponentPinRole role)
{
    static const char *marks[] = {"IO", "IN", "CLK", "OUT", "V+", "V-", "GND", "A", "K", "CONT", "C", "B", "E"};
    if ((unsigned)role >= sizeof(marks) / sizeof(marks[0]))
        return "?";
    return marks[role];
}

PartType component_electrical_type(PartType type)
{
    switch (type)
    {
    case PART_LDR:
    case PART_FLEX:
    case PART_FORCE:
        return PART_RESISTOR;
    case PART_TILT:
        return PART_SWITCH;
    case PART_BATTERY_9V:
    case PART_BATTERY_AA:
    case PART_BATTERY_COIN:
        return PART_SOURCE;
    default:
        return type;
    }
}

double component_effective_value(const Part *part)
{
    double stimulus = component_environment_value(part);
    if (part->type == PART_LDR)
        return fmin(1e8, fmax(10, part->value * pow(10 / fmax(stimulus, .001), .7)));
    if (part->type == PART_FLEX)
        return part->value * (1 + 3 * part->control);
    if (part->type == PART_FORCE)
        return stimulus < .2 ? 1e8 : part->value / stimulus;
    if (part->type == PART_SOIL)
        return 100000 * pow(100, -part->control);
    return part->value;
}

int component_effective_on(const Part *part)
{
    return part->type == PART_TILT ? component_environment_value(part) < 45 : part->on;
}

double component_default_control(PartType type)
{
    const ComponentEnvironment *environment = component_environment(type);
    return environment ? (environment->initial - environment->minimum) / (environment->maximum - environment->minimum) : .5;
}

int component_current_vector(PartType type, int index, char *vector, size_t capacity)
{
    if (type == PART_LED || type == PART_DIODE || type == PART_PHOTODIODE)
        snprintf(vector, capacity, "@d%d[id]", index);
    else if (component_electrical_type(type) == PART_RESISTOR)
        snprintf(vector, capacity, "@r%d[i]", index);
    else if (type == PART_SOIL)
        snprintf(vector, capacity, "@rsoil%d[i]", index);
    else if (type == PART_TEMPERATURE)
        snprintf(vector, capacity, "@vtmp_sense%d[i]", index);
    else if (type == PART_NPN || type == PART_PNP || type == PART_AMBIENT_LIGHT)
        snprintf(vector, capacity, "@q%d[ic]", index);
    else
        return 0;
    return 1;
}


const ComponentEnvironment *component_environment(PartType type)
{
    static const ComponentEnvironment light = {"Luz", "lux", 0, 1000, 100, "LDR generico: R10lux; gamma 0.7"};
    static const ComponentEnvironment bend = {"Dobra", "graus", 0, 90, 0, "Aproximacao: R = Rplano * (1 + 3*angulo/90)"};
    static const ComponentEnvironment force = {"Forca", "N", 0, 20, 0, "FSR generico: R1N/F; aberto abaixo de 0.2 N"};
    static const ComponentEnvironment tilt = {"Inclinacao", "graus", 0, 180, 0, "Contato generico: fechado abaixo de 45 graus"};
    static const ComponentEnvironment photo = {"Luz relativa", "%", 0, 100, 0, "Fotocorrente generica; sem calibracao espectral"};
    static const ComponentEnvironment temperature = {"Temperatura", "C", -40, 125, 25, "TMP36: 2.7..5.5 V; sem aquecimento proprio"};
    static const ComponentEnvironment soil = {"Umidade relativa", "%", 0, 100, 25, "Sonda resistiva generica; nao calibrada em VWC"};
    switch (type)
    {
    case PART_TEMPERATURE:
        return &temperature;
    case PART_SOIL:
        return &soil;
    case PART_LDR:
        return &light;
    case PART_FLEX:
        return &bend;
    case PART_FORCE:
        return &force;
    case PART_TILT:
        return &tilt;
    case PART_PHOTODIODE:
    case PART_AMBIENT_LIGHT:
        return &photo;
    default:
        return NULL;
    }
}

double component_environment_value(const Part *part)
{
    const ComponentEnvironment *environment = component_environment(part->type);
    return environment ? environment->minimum + part->control * (environment->maximum - environment->minimum) : 0;
}

double component_source_resistance(PartType type)
{
    if (type == PART_BATTERY_9V)
        return 1;
    if (type == PART_BATTERY_AA)
        return .2;
    if (type == PART_BATTERY_COIN)
        return 15;
    return 1e-6;
}

int component_radial_pins(PartType type)
{
    return component_environment(type) != NULL || type == PART_NPN || type == PART_PNP;
}


void component_body_extent(PartType type, float *half_width, float *half_height)
{
    *half_width = 44;
    *half_height = 30;
    if (type == PART_BREADBOARD)
    {
        *half_width = 227;
        *half_height = 112;
    }
    else if (component_has_flag(type, COMPONENT_QUAD_GATE))
    {
        *half_width = 50;
        *half_height = 20;
    }
    else if (type == PART_BATTERY_9V || type == PART_BATTERY_AA)
    {
        *half_width = type == PART_BATTERY_AA ? 15 : 27;
        *half_height = 48;
    }
    else if (component_radial_pins(type))
    {
        *half_width = type == PART_FLEX ? 8 : 22;
        *half_height = type == PART_FLEX || type == PART_FORCE || type == PART_SOIL ? 42 : 30;
    }
}


const ComponentTemperatureModel *component_temperature_model(void)
{
    /* ADI nominal transfer; output/limiting resistances are didactic choices. */
    static const ComponentTemperatureModel model = {.offset = .5, .volts_per_degree = .01,
        .supply_minimum = 2.7, .supply_maximum = 5.5, .quiescent_current = 50e-6,
        .output_resistance = 100, .current_limit = 250e-6, .limiting_resistance = 1e9, .off_resistance = 1e12};
    return &model;
}

double component_photocurrent(const Part *part)
{
    const double photodiode_maximum_amps = .001;
    const double phototransistor_base_maximum_amps = .00001;
    return part->control * (part->type == PART_PHOTODIODE ? photodiode_maximum_amps : phototransistor_base_maximum_amps);
}
