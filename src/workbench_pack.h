#ifndef WORKBENCH_PACK_H
#define WORKBENCH_PACK_H
#include "workbench_selection.h"
#include <stddef.h>
#define BENCH_GROUP_PIN_SPACING 14.0f /* same as BENCH_DIP_PITCH: ports land on neighbouring holes */
#define BENCH_BOARD_ROW_PITCH 10.0f
#define BENCH_GROUP_DEFAULT_WIDTH 98.0f
#define BENCH_GROUP_DEFAULT_HEIGHT 44.0f
#define BENCH_GROUP_MIN_WIDTH 20.0f
#define BENCH_GROUP_MIN_HEIGHT 8.0f
#define BENCH_GROUP_MAX_EXTENT 10000.0f

int bench_pin_exposed(const Part *part, int pin);
int bench_group_first(const Workbench *bench, int group);
int bench_group_ports(const Workbench *bench, int group);
int bench_group_port(const Workbench *bench, int group, int index, Pin *pin);
float bench_group_width(const Workbench *bench, int group);
int bench_group_resize(Workbench *bench, int group, float width, float height);
/* Nearest width whose left-to-right port distance matches the breadboard holes in the current rotation. */
float bench_group_board_width(const Workbench *bench, int group, float width);
float bench_group_height(const Workbench *bench, int group);
#define BENCH_PORT_LEFT 0
#define BENCH_PORT_RIGHT 1
/* Where terminal `index` of a chip sits: side, slot on that side (top = 0) and terminals on that side. */
void bench_group_port_place(const Workbench *bench, int group, int index, int *side, int *slot, int *side_count);
/* Move terminal `index` to `side` at `slot` (clamped); the others on both sides keep their order.
 * Returns 0 when the chip has more terminals than BENCH_GROUP_LAYOUT_MAX or the arguments are invalid. */
int bench_group_move_port(Workbench *bench, int group, int index, int side, int slot);
/* Index of the chip terminal that is `pin`, or -1. */
int bench_group_port_index(const Workbench *bench, Pin pin);
WirePoint bench_group_to_world(const BenchGroup *group, WirePoint local);
WirePoint bench_group_to_local(const BenchGroup *group, WirePoint world);
void bench_terminal_position(const Workbench *bench, Pin pin, float *x, float *y);
int bench_pack_selection(Workbench *bench, const BenchSelection *selection, char *message, size_t capacity);
int bench_unpack_selection(Workbench *bench, const BenchSelection *selection);
int bench_group_wire_internal(const Workbench *bench, const Wire *wire);
#endif
