#include "component_devices.h"
#include "component_registry.h"
#include "ui_text.h"
#include <stdio.h>

/* Terminal pitch 14 matches the breadboard; rows sit outside the body so leads stay visible. */
static const ComponentDevice devices[] = {
    {PART_RGB_LED, {{-21, 49}, {-7, 49}, {7, 49}, {21, 49}}, 22, 34, 3, 0, 0},
    {PART_SEVEN_SEGMENT,
     {{-28, -56}, {-14, -56}, {0, -56}, {14, -56}, {28, -56}, {-28, 56}, {-14, 56}, {0, 56}, {14, 56}},
     30, 44, COMPONENT_SEGMENTS, 0, 0},
    {PART_DIP_SWITCH,
     {{-21, -35}, {-7, -35}, {7, -35}, {21, -35}, {-21, 35}, {-7, 35}, {7, 35}, {21, 35}},
     30, 22, 0, 0, COMPONENT_DIP_POSITIONS},
    {PART_DPST, {{-49, -10}, {49, -10}, {-49, 10}, {49, 10}}, 30, 20, 0, 0, 0},
    {PART_RELAY, {{-28, 49}, {-14, 49}, {0, 49}, {14, 49}, {28, 49}}, 34, 30, 2, 0, 0},
    {PART_DC_MOTOR, {{-49, 0}, {49, 0}}, 30, 26, 2, 0, 0},
    {PART_VIBRATION_MOTOR, {{-49, 0}, {49, 0}}, 22, 22, 2, 0, 0},
    {PART_ENCODER_MOTOR,
     {{-35, 56}, {-21, 56}, {-7, 56}, {7, 56}, {21, 56}, {35, 56}},
     40, 34, 2, 0, 0},
    {PART_SERVO, {{-14, 56}, {0, 56}, {14, 56}}, 36, 30, 1, 0, 0},
    {PART_PIEZO, {{-10, 49}, {10, 49}}, 22, 26, 1, 0, 0},
    {PART_FUNCTION_GENERATOR, {{-49, 0}, {49, 0}}, 42, 30, 0, 3, 0},
    {PART_MULTIMETER, {{-21, 70}, {0, 70}, {21, 70}}, 36, 52, 2, 3, 0},
    {PART_OSCILLOSCOPE, {{-14, 70}, {14, 70}}, 70, 52, 1, 0, 0},
    {PART_PIR, {{-14, 49}, {0, 49}, {14, 49}}, 22, 30, 0, 0, 0},
    {PART_GAS, {{-14, 49}, {0, 49}, {14, 49}}, 24, 30, 0, 0, 0},
    {PART_IR_OBSTACLE, {{-14, 49}, {0, 49}, {14, 49}}, 24, 30, 0, 0, 0},
    {PART_DISTANCE, {{-14, 49}, {0, 49}, {14, 49}}, 34, 22, 0, 0, 0},
    {PART_ARDUINO_UNO,
     {{126, -92}, {112, -92}, {98, -92}, {84, -92}, {70, -92}, {56, -92}, {42, -92}, {28, -92}, {14, -92}, {0, -92},
      {-14, -92}, {-28, -92}, {-42, -92}, {-56, -92}, {-28, 92}, {-14, 92}, {0, 92}, {14, 92}, {28, 92}, {42, 92},
      {-84, 92}, {-98, 92}, {-70, 92}},
     150, 78, 20, 0, 0},
    {PART_ESP32,
     {{-140, -58}, {-126, -58}, {-112, -58}, {-98, -58}, {-84, -58}, {-70, -58}, {-56, -58}, {-42, -58}, {-28, -58},
      {-14, -58}, {0, -58}, {-140, 58}, {-126, 58}, {-112, 58}, {-98, 58}, {-84, 58}, {-70, 58}, {-56, 58}, {-42, 58},
      {-28, 58}, {-14, 58}, {0, 58}},
     160, 46, 20, 0, 0},
    {PART_ATTINY85, {{-21, 22}, {-7, 22}, {7, 22}, {21, 22}, {21, -22}, {7, -22}, {-7, -22}, {-21, -22}},
     30, 14, 6, 0, 0},
    {PART_LCD_I2C, {{-136, -21}, {-136, -7}, {-136, 7}, {-136, 21}}, 124, 46, 1, 0, 0},
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
    case PART_ARDUINO_UNO:
    case PART_ESP32:
    case PART_ATTINY85:
        snprintf(vector, capacity, "v(mcu%d_in%d)", index, element);
        return 1;
    case PART_LCD_I2C:
        snprintf(vector, capacity, "v(lcd%d_pwr)", index);
        return 1;
    default:
        return 0;
    }
}

int component_states_valid(PartType type, unsigned states)
{
    if (type == PART_LED)
        return states < COMPONENT_LED_COLORS;
    const ComponentDevice *device = component_device(type);
    if (!device)
        return states == 0;
    if (device->switch_count)
        return states < (1u << device->switch_count);
    return states < (unsigned)(device->mode_count > 1 ? device->mode_count : 1);
}

enum
{
    GENERATOR_WAVES = 3,
    METER_MODES = 3,
    MAX_NAMED_ELEMENTS = 3
};

const char *component_mode_label(PartType type, unsigned states)
{
    static const TextId generator[GENERATOR_WAVES] = {TXT_CAT_WAVE_SINE, TXT_CAT_WAVE_SQUARE, TXT_CAT_WAVE_TRIANGLE};
    static const TextId meter[METER_MODES] = {TXT_CAT_METER_VOLTAGE, TXT_CAT_METER_CURRENT, TXT_CAT_METER_RESISTANCE};
    if (type == PART_FUNCTION_GENERATOR && states < GENERATOR_WAVES)
        return tr(generator[states]);
    if (type == PART_MULTIMETER && states < METER_MODES)
        return tr(meter[states]);
    return NULL;
}

typedef struct
{
    PartType type;
    TextId labels[MAX_NAMED_ELEMENTS];
} ElementTexts;

static const ElementTexts element_texts[] = {
    {PART_RGB_LED, {TXT_CAT_LED_RED, TXT_CAT_LED_GREEN, TXT_CAT_LED_BLUE}},
    {PART_RELAY, {TXT_CAT_ELEMENT_COIL, TXT_CAT_ELEMENT_CONTACT}},
    {PART_DC_MOTOR, {TXT_CAT_ELEMENT_SPEED, TXT_QUANTITY_CURRENT}},
    {PART_VIBRATION_MOTOR, {TXT_CAT_ELEMENT_SPEED, TXT_QUANTITY_CURRENT}},
    {PART_ENCODER_MOTOR, {TXT_CAT_ELEMENT_SPEED, TXT_QUANTITY_CURRENT}},
    {PART_SERVO, {TXT_CAT_ELEMENT_ANGLE}},
    {PART_PIEZO, {TXT_QUANTITY_CURRENT}},
    {PART_MULTIMETER, {TXT_QUANTITY_VOLTAGE, TXT_QUANTITY_CURRENT}},
    {PART_OSCILLOSCOPE, {TXT_CAT_ELEMENT_CHANNEL}},
    {PART_LCD_I2C, {TXT_CAT_ELEMENT_SUPPLY}},
};

static const ElementTexts *element_texts_of(PartType type)
{
    for (size_t i = 0; i < sizeof(element_texts) / sizeof(element_texts[0]); ++i)
        if (element_texts[i].type == type)
            return &element_texts[i];
    return NULL;
}

const char *component_element_label(PartType type, int element)
{
    /* Segment letters are pin names, the same in every language. */
    static const char *segments[COMPONENT_SEGMENTS] = {"a", "b", "c", "d", "e", "f", "g", "dp"};
    const ComponentDevice *device = component_device(type);
    if (!device || element < 0 || element >= device->element_count)
        return NULL;
    if (type == PART_SEVEN_SEGMENT)
        return segments[element];
    const ElementTexts *texts = element_texts_of(type);
    if (!texts || element >= MAX_NAMED_ELEMENTS)
        return NULL;
    return tr(texts->labels[element]);
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

const char *component_led_model(unsigned color)
{
    static const char *models[COMPONENT_LED_COLORS] = {"LED_RED",  "LED_GREEN",  "LED_BLUE", "LED_YELLOW",
                                                       "LED_BLUE", "LED_YELLOW", "LED_BLUE", "LED_BLUE",
                                                       "LED_BLUE", "LED_GREEN"};
    return color < COMPONENT_LED_COLORS ? models[color] : models[0];
}

const char *component_color_name(unsigned color)
{
    static const TextId names[BENCH_COLOR_COUNT] = {TXT_COLOR_RED,    TXT_COLOR_GREEN,  TXT_COLOR_BLUE, TXT_COLOR_YELLOW,
                                                    TXT_COLOR_WHITE,  TXT_COLOR_ORANGE, TXT_COLOR_VIOLET,
                                                    TXT_COLOR_PINK,   TXT_COLOR_CYAN,   TXT_COLOR_LIME,
                                                    TXT_COLOR_BLACK};
    return tr(color < BENCH_COLOR_COUNT ? names[color] : names[0]);
}

int component_ohmmeter_reading_reliable(const Workbench *bench)
{
    if (!bench)
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
    {
        const Part *part = &bench->parts[i];
        if (part->on && (component_electrical_type(part->type) == PART_SOURCE ||
                         part->type == PART_CLOCK || part->type == PART_FUNCTION_GENERATOR))
            return 0;
    }
    return 1;
}
