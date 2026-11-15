#ifndef WORKBENCH_H
#define WORKBENCH_H
#include <stdint.h>

#define WORKBENCH_MAX_COMPONENTS 64
#define WORKBENCH_MAX_WIRES 128
#define WORKBENCH_MAX_PINS 420
#define BREADBOARD_PINS 420

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
    PART_74HC00
} PartType;
typedef struct
{
    PartType type;
    float x, y;
    int on;
    double value;
    int rotation;
    int board_layout;
} Part;
typedef struct
{
    int component, pin;
} Pin;
typedef struct
{
    float x, y;
} WirePoint;
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
    Part parts[WORKBENCH_MAX_COMPONENTS];
    Wire wires[WORKBENCH_MAX_WIRES];
    int part_count, wire_count;
    /* Runtime identity of edits; never serialized or restored from history.
     * Edit published assemblies through the mutation functions below. */
    uint64_t revision;
} Workbench;

void bench_replace(Workbench *bench, const Workbench *replacement);
int bench_move_part(Workbench *bench, int component, float x, float y);
int bench_set_on(Workbench *bench, int component, int on);
int bench_set_wire_color(Workbench *bench, int wire, int color);
int bench_set_wire_route(Workbench *bench, int wire, const WirePoint *points, int count);
int bench_move_knee(Workbench *bench, int wire, int knee, WirePoint point);
int bench_remove_knee(Workbench *bench, int wire, int knee);
int bench_remove_wire(Workbench *bench, int wire);

int bench_add(Workbench *bench, PartType type, float x, float y);
int bench_pin_count(PartType type);
int bench_board_group_first(int pin);
int bench_set_value(Workbench *bench, int component, double value);
int bench_rotate_part(Workbench *bench, int component);
int bench_parse_value(const char *text, double *value);
void bench_pin_position(const Part *part, int pin, float *x, float *y);
int bench_find_contact(const Workbench *bench, int component, int pin, Pin *contact);
int bench_snap_to_breadboard(Workbench *bench, int component);
/* Returns 1 for added, 2 for removed, 0 for invalid/capacity. */
int bench_toggle_wire(Workbench *bench, Pin a, Pin b);
void bench_remove(Workbench *bench, int index);
void bench_simulate(const Workbench *bench, int led_on[WORKBENCH_MAX_COMPONENTS]);
int bench_boolean_supported(const Workbench *bench);
int wire_path(const Wire *wire, WirePoint start, WirePoint end, float radius, WirePoint output[WIRE_PATH_CAPACITY]);
#endif
