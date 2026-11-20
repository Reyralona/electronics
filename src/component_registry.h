#ifndef COMPONENT_REGISTRY_H
#define COMPONENT_REGISTRY_H
#include "workbench.h"
#include <stddef.h>

typedef struct
{
    const char *key;
    const char *unit;
    double initial, minimum, maximum;
} ComponentParameter;

typedef enum
{
    PIN_PASSIVE,
    PIN_INPUT,
    PIN_CLOCK_INPUT,
    PIN_OUTPUT,
    PIN_SUPPLY_POSITIVE,
    PIN_SUPPLY_RETURN,
    PIN_GROUND,
    PIN_ANODE,
    PIN_CATHODE,
    PIN_CONTACT,
    PIN_COLLECTOR,
    PIN_BASE,
    PIN_EMITTER,
    PIN_BIDIRECTIONAL /* bus terminal that can be input or output */
} ComponentPinRole;

typedef struct
{
    const char *name;
    ComponentPinRole role;
} ComponentPinInfo;

enum
{
    COMPONENT_ROTATES = 1,
    COMPONENT_STARTS_ON = 2,
    COMPONENT_GATE_SYMBOL = 4,
    COMPONENT_CONNECTIVITY = 8,
    COMPONENT_QUAD_GATE = 16,
    COMPONENT_DIP = 32 /* dual in-line IC: N/2 pins per side at the 74HC pitch */
};

typedef struct
{
    PartType type;
    const char *id;
    const char *display_name; /* in the current interface language */
    int pin_count;
    unsigned flags;
    int first_file_version;
    const ComponentParameter *primary;
    const ComponentPinInfo *pins;
} ComponentInfo;

PartType component_electrical_type(PartType type);
double component_effective_value(const Part *part);
int component_effective_on(const Part *part);
double component_default_control(PartType type);
int component_current_vector(PartType type, int index, char *vector, size_t capacity);
typedef struct
{
    const char *label;
    const char *unit;
    double minimum, maximum, initial;
    const char *note;
} ComponentEnvironment;

typedef struct
{
    double offset, volts_per_degree;
    double supply_minimum, supply_maximum, quiescent_current;
    double output_resistance, current_limit, limiting_resistance, off_resistance;
} ComponentTemperatureModel;

const ComponentTemperatureModel *component_temperature_model(void);
double component_photocurrent(const Part *part);
const ComponentEnvironment *component_environment(PartType type);
double component_environment_value(const Part *part);
double component_source_resistance(PartType type);
int component_radial_pins(PartType type);
void component_body_extent(PartType type, float *half_width, float *half_height);
size_t component_registry_count(void);
const ComponentInfo *component_info(PartType type);
const ComponentInfo *component_from_legacy(int code, int version);
const ComponentInfo *component_by_id(const char *id);
int component_value_valid(PartType type, double value);
int component_has_flag(PartType type, unsigned flag);
int component_legacy_supported(PartType type, int version);
const ComponentPinInfo *component_pin_info(PartType type, int pin);
const char *component_pin_role_label(ComponentPinRole role);
const char *component_pin_marker(ComponentPinRole role);
/* Short label drawn on gate and IC bodies ("AND", "DFF", "74HC00"), the same in every language; NULL
 * for other parts. Use it instead of cutting a prefix off display_name. */
const char *component_symbol(PartType type);
#endif
