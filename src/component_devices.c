#include "component_devices.h"
#include <stdio.h>

/* Terminal pitch 14 matches the breadboard; rows sit outside the body so leads stay visible. */
static const ComponentDevice devices[] = {
    {PART_RGB_LED, {{-21, 49}, {-7, 49}, {7, 49}, {21, 49}}, 22, 34, 3, {"Vermelho", "Verde", "Azul"}, 0, 0},
    {PART_SEVEN_SEGMENT,
     {{-28, -56}, {-14, -56}, {0, -56}, {14, -56}, {28, -56}, {-28, 56}, {-14, 56}, {0, 56}, {14, 56}},
     30, 44, COMPONENT_SEGMENTS, {"a", "b", "c", "d", "e", "f", "g", "dp"}, 0, 0},
    {PART_DIP_SWITCH,
     {{-21, -35}, {-7, -35}, {7, -35}, {21, -35}, {-21, 35}, {-7, 35}, {7, 35}, {21, 35}},
     30, 22, 0, {0}, 0, COMPONENT_DIP_POSITIONS},
    {PART_DPST, {{-49, -10}, {49, -10}, {-49, 10}, {49, 10}}, 30, 20, 0, {0}, 0, 0},
    {PART_RELAY, {{-28, 49}, {-14, 49}, {0, 49}, {14, 49}, {28, 49}}, 34, 30, 2, {"Bobina", "Contato"}, 0, 0},
    {PART_DC_MOTOR, {{-49, 0}, {49, 0}}, 30, 26, 2, {"Velocidade", "Corrente"}, 0, 0},
    {PART_VIBRATION_MOTOR, {{-49, 0}, {49, 0}}, 22, 22, 2, {"Velocidade", "Corrente"}, 0, 0},
    {PART_ENCODER_MOTOR,
     {{-35, 56}, {-21, 56}, {-7, 56}, {7, 56}, {21, 56}, {35, 56}},
     40, 34, 2, {"Velocidade", "Corrente"}, 0, 0},
    {PART_SERVO, {{-14, 56}, {0, 56}, {14, 56}}, 36, 30, 1, {"Ângulo"}, 0, 0},
    {PART_PIEZO, {{-10, 49}, {10, 49}}, 22, 26, 1, {"Corrente"}, 0, 0},
    {PART_FUNCTION_GENERATOR, {{-49, 0}, {49, 0}}, 42, 30, 0, {0}, 3, 0},
    {PART_MULTIMETER, {{-21, 70}, {0, 70}, {21, 70}}, 36, 52, 2, {"Tensão", "Corrente"}, 3, 0},
    {PART_OSCILLOSCOPE, {{-14, 70}, {14, 70}}, 70, 52, 1, {"Canal"}, 0, 0},
    {PART_PIR, {{-14, 49}, {0, 49}, {14, 49}}, 22, 30, 0, {0}, 0, 0},
    {PART_GAS, {{-14, 49}, {0, 49}, {14, 49}}, 24, 30, 0, {0}, 0, 0},
    {PART_IR_OBSTACLE, {{-14, 49}, {0, 49}, {14, 49}}, 24, 30, 0, {0}, 0, 0},
    {PART_DISTANCE, {{-14, 49}, {0, 49}, {14, 49}}, 34, 22, 0, {0}, 0, 0},
};

const ComponentDevice *component_device(PartType type)
{
    for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); ++i)
        if (devices[i].type == type)
            return &devices[i];
    return NULL;
}

int component_element_vector(PartType type, int index, int element, char *vector, size_t capacity)
{
    const ComponentDevice *device = component_device(type);
    if (!device || element < 0 || element >= device->element_count)
        return 0;
    static const char colors[] = "rgb";
    switch (type)
    {
    case PART_RGB_LED:
        snprintf(vector, capacity, "@d%d_%c[id]", index, colors[element]);
        return 1;
    case PART_SEVEN_SEGMENT:
        snprintf(vector, capacity, "@d%d_s%d[id]", index, element);
        return 1;
    case PART_RELAY:
        snprintf(vector, capacity, element ? "v(rly_on%d)" : "vrly%d#branch", index);
        return 1;
    case PART_DC_MOTOR:
    case PART_VIBRATION_MOTOR:
    case PART_ENCODER_MOTOR:
        snprintf(vector, capacity, element ? "vmot%d#branch" : "v(mspd%d)", index);
        return 1;
    case PART_SERVO:
        snprintf(vector, capacity, "v(sang%d)", index);
        return 1;
    case PART_PIEZO:
        snprintf(vector, capacity, "vpz%d#branch", index);
        return 1;
    case PART_MULTIMETER:
        snprintf(vector, capacity, element ? "vamm%d#branch" : "v(mmv%d)", index);
        return 1;
    case PART_OSCILLOSCOPE:
        snprintf(vector, capacity, "v(oscv%d)", index);
        return 1;
    default:
        return 0;
    }
}

int component_states_valid(PartType type, unsigned states)
{
    const ComponentDevice *device = component_device(type);
    if (!device)
        return states == 0;
    if (device->switch_count)
        return states < (1u << device->switch_count);
    return states < (unsigned)(device->mode_count > 1 ? device->mode_count : 1);
}

const char *component_mode_label(PartType type, unsigned states)
{
    static const char *generator[] = {"Senoidal", "Quadrada", "Triangular"};
    static const char *meter[] = {"Tensão (V)", "Corrente (A)", "Resistência (Ω)"};
    if (type == PART_FUNCTION_GENERATOR && states < 3)
        return generator[states];
    if (type == PART_MULTIMETER && states < 3)
        return meter[states];
    return NULL;
}

int component_switch_at(PartType type, float local_x)
{
    const ComponentDevice *device = component_device(type);
    if (!device || !device->switch_count)
        return -1;
    for (int contact = 0; contact < device->switch_count; ++contact)
    {
        float center = device->pins[contact][0];
        if (local_x >= center - 7 && local_x < center + 7)
            return contact;
    }
    return -1;
}
