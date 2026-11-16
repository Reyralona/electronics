#ifndef WORKBENCH_H
#define WORKBENCH_H
#include <stdint.h>
#include <stddef.h>

#define WORKBENCH_MAX_COMPONENTS 64
#define WORKBENCH_MAX_WIRES 128
#define WORKBENCH_MAX_PINS 420
#define WORKBENCH_MAX_IO 24
#define BREADBOARD_PINS 420
#define PART_SCALE_MIN 0.01f
#define PART_SCALE_MAX 100.0f
#define BENCH_GROUP_NAME_CAPACITY 128

typedef enum
{
    PART_SOURCE,
    PART_SWITCH,
    PART_LED,
    PART_BREADBOARD,
    PART_RESISTOR,
    PART_CAPACITOR,
    PART_INDUCTOR,
    PART_AND,
    PART_NAND,
    PART_OR,
    PART_XOR,
    PART_NOT,
    PART_CLOCK,
    PART_DFF,
    PART_74HC00,
    PART_NOR,
    PART_XNOR,
    PART_POTENTIOMETER,
    PART_BUTTON,
    PART_74HC08,
    PART_74HC32,
    PART_74HC86,
    PART_DIODE,
    PART_LDR,
    PART_FLEX,
    PART_FORCE,
    PART_TILT,
    PART_NPN,
    PART_PNP,
    PART_BATTERY_9V,
    PART_BATTERY_AA,
    PART_BATTERY_COIN,
    PART_PHOTODIODE,
    PART_AMBIENT_LIGHT,
    PART_TEMPERATURE,
    PART_SOIL,
    PART_LAMP,
    PART_RGB_LED,
    PART_SEVEN_SEGMENT,
    PART_DIP_SWITCH,
    PART_DPST,
    PART_RELAY,
    PART_DC_MOTOR,
    PART_VIBRATION_MOTOR,
    PART_ENCODER_MOTOR,
    PART_SERVO,
    PART_PIEZO,
    PART_FUNCTION_GENERATOR,
    PART_MULTIMETER,
    PART_OSCILLOSCOPE,
    PART_PIR,
    PART_GAS,
    PART_IR_OBSTACLE,
    PART_DISTANCE,
    PART_ARDUINO_UNO,
    PART_ESP32,
    PART_ATTINY85,
    PART_LCD_I2C
} PartType;
typedef struct
{
    PartType type;
    struct BenchTerminals *terminals; /* Owned; use part clone/clear, never owning assignment. */
    char *code; /* Owned microcontroller program text (NULL = default); same ownership rules as terminals. */
    float x, y;
    int on;
    double value;
    double control; /* Potentiometer wiper position, 0..1. */
    unsigned states; /* Per-element switches (DIP bits) or instrument mode; see component_devices.h. */
    /* Microcontroller pin drive written by the running program; runtime only, not saved or versioned. */
    float io_level[WORKBENCH_MAX_IO];
    unsigned char io_mode[WORKBENCH_MAX_IO];
    int rotation;
    int board_layout;
    int group; /* 0 = ordinary part; otherwise index in groups + 1. */
    uint64_t exposed[(WORKBENCH_MAX_PINS + 63) / 64];
    float scale; /* 0 is the legacy/default identity scale. */
    int edit_origin; /* Transaction provenance; cleared by paste/import, never serialized. */
} Part;
typedef struct
{
    int component, pin;
} Pin;
typedef struct
{
    float x, y;
} WirePoint;

typedef void (*BenchLeadVertex)(void *context, WirePoint point, size_t slot);
void bench_lead_walk(const Part *part, int pin, float radius, BenchLeadVertex vertex, void *context);
int bench_lead_pick(const Part *part, int pin, float radius, WirePoint point, float tolerance,
                    size_t *slot, WirePoint *projected);
size_t bench_lead_count(const Part *part, int pin);
WirePoint bench_lead_point(const Part *part, int pin, size_t point);
WirePoint bench_lead_anchor(const Part *part, int pin);
#define WIRE_MAX_KNEES 16
#define WIRE_PATH_CAPACITY (WIRE_MAX_KNEES * 9 + 2)
#define WIRE_COLOR_COUNT 6
typedef struct
{
    Pin a, b;
    WirePoint knees[WIRE_MAX_KNEES];
    int knee_count;
    int color;
} Wire;

typedef struct
{
    int active;
    float x, y;
    char name[BENCH_GROUP_NAME_CAPACITY];
    float width, height; /* Zero selects the automatic package dimensions. */
    int rotation; /* Package orientation, independent of internal parts; saved from format 29. */
    uint64_t model; /* Library model identity shared by its copies (0 = none); saved from format 28. */
} BenchGroup;
typedef struct
{
    Part parts[WORKBENCH_MAX_COMPONENTS];
    Wire wires[WORKBENCH_MAX_WIRES];
    BenchGroup groups[WORKBENCH_MAX_COMPONENTS];
    int part_count, wire_count;
    /* Runtime identity of edits; never serialized or restored from history.
     * Edit published assemblies through the mutation functions below. */
    uint64_t revision;
} Workbench;

int bench_lead_move(Workbench *bench, int component, int pin, size_t point, WirePoint world);
int bench_lead_insert(Workbench *bench, int component, int pin, size_t before, WirePoint world);
int bench_lead_remove(Workbench *bench, int component, int pin, size_t point);
int bench_replace(Workbench *bench, const Workbench *replacement);
/* Snapshots preserve revision; publishing an edit uses bench_replace instead.
 * Destinations must be zero-initialized or previously initialized snapshots. */
int bench_part_clone(Part *destination, const Part *source);
void bench_part_clear(Part *part);
void bench_part_move(Part *destination, Part *source);
int bench_snapshot_clone(Workbench *destination, const Workbench *source);
void bench_snapshot_clear(Workbench *snapshot);
void bench_snapshot_move(Workbench *destination, Workbench *source);
float bench_part_scale(const Part *part);
int bench_set_control(Workbench *bench, int component, double position);
int bench_set_scale(Workbench *bench, int component, float scale);
int bench_move_part(Workbench *bench, int component, float x, float y);
int bench_set_on(Workbench *bench, int component, int on);
/* Replaces the per-part state word; rejects values the component does not accept. */
int bench_set_states(Workbench *bench, int component, unsigned states);
/* Replaces the owned program text of a microcontroller part. */
int bench_set_code(Workbench *bench, int component, const char *code);
int bench_set_wire_color(Workbench *bench, int wire, int color);
int bench_set_wire_route(Workbench *bench, int wire, const WirePoint *points, int count);
int bench_insert_knee(Workbench *bench, int wire, int slot, WirePoint point);
int wire_pick_segment(const Wire *wire, WirePoint start, WirePoint end, float radius,
                      WirePoint point, float tolerance, int *slot, WirePoint *projected);
int bench_move_knee(Workbench *bench, int wire, int knee, WirePoint point);
int bench_remove_knee(Workbench *bench, int wire, int knee);
int bench_remove_wire(Workbench *bench, int wire);

int bench_add(Workbench *bench, PartType type, float x, float y);
int bench_swap_type(Workbench *bench, int component, PartType type);
int bench_pin_count(PartType type);
int bench_board_group_first(int pin);
int bench_set_value(Workbench *bench, int component, double value);
int bench_rotate_part(Workbench *bench, int component);
int bench_parse_value(const char *text, double *value);
WirePoint bench_part_to_world(const Part *part, WirePoint local);
WirePoint bench_part_to_local(const Part *part, WirePoint world);
void bench_pin_position(const Part *part, int pin, float *x, float *y);
int bench_find_contact_after(const Workbench *bench, int component, int pin, int after, Pin *contact);
int bench_find_contact(const Workbench *bench, int component, int pin, Pin *contact);
int bench_breadboard_snap_position(const Workbench *bench, const Part *part, float *x, float *y);
int bench_snap_to_breadboard(Workbench *bench, int component);
/* Returns 1 for added, 2 for removed, 0 for invalid/capacity. */
int bench_toggle_wire(Workbench *bench, Pin a, Pin b);
void bench_remove(Workbench *bench, int index);
void bench_simulate(const Workbench *bench, int led_on[WORKBENCH_MAX_COMPONENTS]);
int bench_boolean_supported(const Workbench *bench);
int wire_path(const Wire *wire, WirePoint start, WirePoint end, float radius, WirePoint output[WIRE_PATH_CAPACITY]);
#endif
