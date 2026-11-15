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
    PIN_CONTACT
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
    COMPONENT_CONNECTIVITY = 8
};

typedef struct
{
    PartType type;
    const char *id;
    const char *display_name;
    int pin_count;
    unsigned flags;
    int first_file_version;
    const ComponentParameter *primary;
    const ComponentPinInfo *pins;
} ComponentInfo;

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
#endif
