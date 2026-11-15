#ifndef WORKBENCH_H
#define WORKBENCH_H
#include <stdint.h>

#define WORKBENCH_MAX_COMPONENTS 64
#define WORKBENCH_MAX_WIRES 128
#define WORKBENCH_MAX_PINS 420
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

void bench_replace(Workbench *bench, const Workbench *replacement);
/* Snapshots preserve revision; publishing an edit uses bench_replace instead.
 * Destinations must be zero-initialized or previously initialized snapshots. */
int bench_snapshot_clone(Workbench *destination, const Workbench *source);
void bench_snapshot_clear(Workbench *snapshot);
void bench_snapshot_move(Workbench *destination, Workbench *source);
float bench_part_scale(const Part *part);
int bench_set_scale(Workbench *bench, int component, float scale);
int bench_move_part(Workbench *bench, int component, float x, float y);
int bench_set_on(Workbench *bench, int component, int on);
int bench_set_wire_color(Workbench *bench, int wire, int color);
int bench_set_wire_route(Workbench *bench, int wire, const WirePoint *points, int count);
int bench_insert_knee(Workbench *bench, int wire, int slot, WirePoint point);
int wire_pick_segment(const Wire *wire, WirePoint start, WirePoint end, float radius,
                      WirePoint point, float tolerance, int *slot, WirePoint *projected);
int bench_move_knee(Workbench *bench, int wire, int knee, WirePoint point);
int bench_remove_knee(Workbench *bench, int wire, int knee);
int bench_remove_wire(Workbench *bench, int wire);

int bench_add(Workbench *bench, PartType type, float x, float y);
int bench_pin_count(PartType type);
int bench_board_group_first(int pin);
int bench_set_value(Workbench *bench, int component, double value);
int bench_rotate_part(Workbench *bench, int component);
int bench_parse_value(const char *text, double *value);
WirePoint bench_part_to_world(const Part *part, WirePoint local);
WirePoint bench_part_to_local(const Part *part, WirePoint world);
void bench_pin_position(const Part *part, int pin, float *x, float *y);
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
