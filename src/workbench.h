#ifndef WORKBENCH_H
#define WORKBENCH_H
#include <stdint.h>
#include <stddef.h>

#define WORKBENCH_MAX_COMPONENTS 1024
#define WORKBENCH_MAX_WIRES 2048
#define WORKBENCH_MAX_PINS 840
#define WORKBENCH_MAX_IO 24
#define BENCH_DIP_PITCH 14 /* distance between neighboring IC pins */
#define BREADBOARD_PINS 420
#define BREADBOARD_WIDE_PINS 840
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
    PART_LCD_I2C,
    PART_JUNCTION, /* wire-to-wire connection point: one pin, no body; saved from format 31 */
    /* Medium-scale logic ICs, saved from format 35 (see netlist_logic.h). */
    PART_74HC161,
    PART_74HC173,
    PART_74HC245,
    PART_74HC283,
    PART_AT28C16,
    PART_BREADBOARD_WIDE,
    PART_NOTE,
    /* Logic IC collection, saved from format 38. Append new types at the end: the index is the file code. */
    PART_74HC04,
    PART_74HC74,
    /* Linear ICs (netlist_linear.h), saved from format 38. */
    PART_NE555,
    PART_LM358,
    PART_LM393,
    PART_LM7805,
    PART_ULN2003,
    PART_L293D,
    PART_74HC02,
    PART_74HC10,
    PART_74HC11,
    PART_74HC14,
    PART_74HC273,
    PART_74HC373,
    PART_74HC374,
    PART_74HC163,
    PART_74HC193,
    PART_74HC138,
    PART_74HC139,
    PART_74HC151,
    PART_74HC157,
    PART_74HC4511,
    PART_TYPE_COUNT
} PartType;
static inline int bench_is_board(PartType type)
{
    return type == PART_BREADBOARD || type == PART_BREADBOARD_WIDE;
}
typedef struct BenchPartLayer
{
    int group;
    uint64_t exposed[(WORKBENCH_MAX_PINS + 63) / 64];
    char *port_names;
    struct BenchPartLayer *next;
} BenchPartLayer;

typedef struct
{
    PartType type;
    BenchPartLayer *layers; /* Owned inner package states, outermost first. */
    struct BenchTerminals *terminals; /* Owned; use part clone/clear, never owning assignment. */
    char *code; /* Owned microcontroller program text (NULL = default); same ownership rules as terminals. */
    /* Owned names of this part's chip terminals, one "<pin>\t<name>\n" line each (NULL = none); same
     * ownership rules as terminals. Shown only while the pin is an exposed terminal of a chip. */
    char *port_names;
    float x, y;
    int on;
    double value;
    double control; /* Potentiometer wiper position, 0..1. */
    unsigned states; /* Per-element switches (DIP bits) or instrument mode; see component_devices.h. */
    /* Microcontroller pin drive written by the running program; runtime only, not saved or versioned. */
    float io_level[WORKBENCH_MAX_IO];
    unsigned char io_mode[WORKBENCH_MAX_IO];
    int rotation;
    int mirrored; /* Left-right mirror in the package frame, applied before rotation; saved from format 34. */
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
/* One palette for wires and LEDs (Wire.color, LED Part.states), saved by index from format 33.
 * Black is the last entry and exists only for wires: there is no black LED. */
typedef enum
{
    BENCH_COLOR_RED,
    BENCH_COLOR_GREEN,
    BENCH_COLOR_BLUE,
    BENCH_COLOR_YELLOW,
    BENCH_COLOR_WHITE,
    BENCH_COLOR_ORANGE,
    BENCH_COLOR_VIOLET,
    BENCH_COLOR_PINK,
    BENCH_COLOR_CYAN,
    BENCH_COLOR_LIME,
    BENCH_COLOR_BLACK,
    BENCH_COLOR_COUNT
} BenchColor;
#define BENCH_LED_COLOR_COUNT BENCH_COLOR_BLACK
#define WIRE_COLOR_COUNT BENCH_COLOR_COUNT
#define BENCH_DEFAULT_WIRE_COLOR BENCH_COLOR_BLUE
/* Wire colors up to format 32 were blue, red, black, green, yellow, white. */
#define BENCH_LEGACY_WIRE_COLORS 6
int bench_color_from_legacy_wire(int legacy);
typedef struct
{
    Pin a, b;
    WirePoint knees[WIRE_MAX_KNEES];
    int knee_count;
    int color;
} Wire;

/* Custom chip terminal arrangement: up to this many terminals can be placed by hand. */
#define BENCH_GROUP_LAYOUT_MAX 64
typedef struct
{
    unsigned char side; /* 0 = left, 1 = right (package frame, before rotation). */
    unsigned char row;  /* Order on that side, top to bottom. */
} BenchPortPlace;

typedef struct
{
    int active;
    int parent; /* Hidden inside this group; zero means visible in this context. */
    float x, y;
    char name[BENCH_GROUP_NAME_CAPACITY];
    float width, height; /* Zero selects the automatic package dimensions. */
    int rotation; /* Package orientation, independent of internal parts; saved from format 29. */
    int mirrored; /* Terminal sides swapped (left <-> right) before rotation; saved from format 34. */
    uint64_t model; /* Library model identity shared by its copies (0 = none); saved from format 28. */
    /* Number of terminals the custom arrangement covers; 0 or a different terminal count = automatic
     * (first half left, rest right). Saved from format 30. */
    int port_layout;
    BenchPortPlace port_places[BENCH_GROUP_LAYOUT_MAX];
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
int bench_part_push_layer(Part *part);
void bench_part_pop_layer(Part *part);
void bench_groups_prune(Workbench *bench);
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
/* Chip terminal names: up to BENCH_PORT_NAME_CAPACITY - 1 bytes of UTF-8 without control characters. */
#define BENCH_PORT_NAME_CAPACITY 16
int bench_port_name_valid(const char *name);
/* Copies the name of `pin` into `name` (empty when unnamed); returns 1 when the pin has a name. */
int bench_port_name(const Part *part, int pin, char *name, size_t capacity);
/* Sets or, with an empty name, removes the name of one terminal. Returns 0 for invalid arguments. */
int bench_set_port_name(Workbench *bench, Pin pin, const char *name);
int bench_set_wire_color(Workbench *bench, int wire, int color);
/* Mirrors a rotatable part (or a chip) left-right: its terminals swap sides. Displays stay readable. */
int bench_mirror_part(Workbench *bench, int component);
int bench_set_wire_route(Workbench *bench, int wire, const WirePoint *points, int count);
int bench_insert_knee(Workbench *bench, int wire, int slot, WirePoint point);
int wire_pick_segment(const Wire *wire, WirePoint start, WirePoint end, float radius,
                      WirePoint point, float tolerance, int *slot, WirePoint *projected);
int bench_move_knee(Workbench *bench, int wire, int knee, WirePoint point);
int bench_remove_knee(Workbench *bench, int wire, int knee);
int bench_remove_wire(Workbench *bench, int wire);
/* Splits `wire` at `point` (knees before `slot` stay on the first half) with a new junction part there.
 * Both halves keep the color. Returns the junction index, or -1 when the arguments or capacity fail. */
int bench_split_wire(Workbench *bench, int wire, int slot, WirePoint point);
/* Removes junctions that no wire reaches any more. Returns how many were removed. */
int bench_prune_junctions(Workbench *bench);

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
