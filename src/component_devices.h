#ifndef COMPONENT_DEVICES_H
#define COMPONENT_DEVICES_H
#include <stddef.h>
#include "workbench.h"
/* Geometry, measurements and modes of multi-element devices (RGB LED, displays, motors, relays,
 * instruments and powered sensor modules). Pure data: no SDL/OpenGL, no netlist text. */
#define COMPONENT_MAX_ELEMENTS 24
#define COMPONENT_MAX_DEVICE_PINS 24

typedef struct
{
    PartType type;
    float pins[COMPONENT_MAX_DEVICE_PINS][2]; /* local terminal positions */
    float half_width, half_height;           /* selectable body */
    int element_count;                       /* measured quantities exposed per instance */
    const char *element_labels[COMPONENT_MAX_ELEMENTS];
    int mode_count;   /* > 1: double click cycles part->states through modes */
    int switch_count; /* > 0: part->states is a bit per independent contact */
} ComponentDevice;

const ComponentDevice *component_device(PartType type);
/* ngspice vector for element `element` of part `index` (current, node voltage or state). */
int component_element_vector(PartType type, int index, int element, char *vector, size_t capacity);
int component_states_valid(PartType type, unsigned states);
const char *component_mode_label(PartType type, unsigned states);
/* Contact index (0-based) under a local x coordinate for switch banks, or -1. */
int component_switch_at(PartType type, float local_x);
#define COMPONENT_DIP_POSITIONS 4
#define COMPONENT_SEGMENTS 8
#define COMPONENT_SCOPE_HISTORY 160
/* LED colors (Part.states); white uses the blue junction, orange the yellow one. */
#define COMPONENT_LED_COLORS 6
const char *component_led_model(unsigned color);
const char *component_led_color_name(unsigned color);
#endif
