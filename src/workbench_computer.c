#include "workbench_computer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Layout: chips in rows; to the left, one vertical line per shared net (bus bits, control lines, supply and
 * clocks) made of junctions. Each pin reaches its line along its own horizontal lane in the gap between rows. */
#define ROW_PITCH 520.0f
#define FIRST_ROW_Y 200.0f
#define COLUMN_PITCH 260.0f
#define FIRST_COLUMN_X 640.0f
#define CHANNEL_X 60.0f
#define CHANNEL_PITCH 16.0f
#define LANE_START 14.0f
#define LANE_STEP 7.0f
#define ROW_COUNT 7
#define NET_JUNCTIONS 256
#define LED_PITCH 60.0f
#define LED_OHMS 330.0
#define MICROCODE_TEXT_CAPACITY 2048

/* Wire colors, as indices of the shared palette. */
enum
{
    COLOR_SIGNAL = BENCH_COLOR_BLUE,
    COLOR_SUPPLY = BENCH_COLOR_RED,
    COLOR_RETURN = BENCH_COLOR_BLACK,
    COLOR_CONTROL = BENCH_COLOR_GREEN,
    COLOR_BUS = BENCH_COLOR_YELLOW,
    COLOR_CLOCK = BENCH_COLOR_VIOLET
};

/* Shared nets, one vertical line each. Control lines named with N are active low. */
enum
{
    NET_BUS0,
    NET_BUS7 = NET_BUS0 + 7,
    NET_HLT_N,
    NET_MI_N,
    NET_RO_N,
    NET_IO_N,
    NET_II_N,
    NET_AI_N,
    NET_AO_N,
    NET_EO_N,
    NET_SU,
    NET_BI_N,
    NET_OI_N,
    NET_CE,
    NET_CO_N,
    NET_SR_N,
    NET_CLOCK,
    NET_CLOCK_INVERTED,
    NET_VCC,
    NET_GND,
    NET_COUNT
};

/* Control word: one bit per signal, set when the signal is active. */
enum
{
    HLT = 1 << 0,
    MI = 1 << 1,
    RO = 1 << 2,
    IO = 1 << 3,
    II = 1 << 4,
    AI = 1 << 5,
    AO = 1 << 6,
    EO = 1 << 7,
    SU = 1 << 8,
    BI = 1 << 9,
    OI = 1 << 10,
    CE = 1 << 11,
    CO = 1 << 12,
    SR = 1 << 13,
    ACTIVE_HIGH = SU | CE,
    UNUSED_BITS = 3 << 14
};

enum
{
    OP_LDA = 0x1,
    OP_ADD = 0x2,
    OP_SUB = 0x3,
    OP_OUT = 0xE,
    OP_HLT = 0xF,
    STEPS = 8,
    OPCODES = 16
};

/* 0-based pins (datasheet pin - 1). */
enum
{
    P161_CLR = 0, P161_CLK = 1, P161_A = 2, P161_ENP = 6, P161_GND = 7, P161_LOAD = 8, P161_ENT = 9,
    P161_QA = 13, P161_VCC = 15,
    P173_M = 0, P173_N = 1, P173_Q1 = 2, P173_CLK = 6, P173_GND = 7, P173_G1 = 8, P173_G2 = 9, P173_CLR = 14,
    P173_VCC = 15,
    P245_DIR = 0, P245_A1 = 1, P245_GND = 9, P245_OE = 18, P245_VCC = 19,
    P283_C0 = 6, P283_GND = 7, P283_C4 = 8, P283_VCC = 15,
    P28_GND = 11, P28_CE = 17, P28_OE = 19, P28_WE = 20, P28_VCC = 23,
    PQUAD_GND = 6, PQUAD_VCC = 13
};

static const int d173[4] = {13, 12, 11, 10};
static const int a283[4] = {4, 2, 13, 11}, b283[4] = {5, 1, 14, 10}, s283[4] = {3, 0, 12, 9};
static const int address28[11] = {7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18};
static const int data28[8] = {8, 9, 10, 12, 13, 14, 15, 16};
static const int quad[4][3] = {{0, 1, 2}, {3, 4, 5}, {8, 9, 7}, {11, 12, 10}};

typedef struct
{
    int junctions[NET_JUNCTIONS];
    int count;
} Net;

typedef struct
{
    Workbench *b;
    Net nets[NET_COUNT];
    int lanes[ROW_COUNT][2];
    unsigned char row[WORKBENCH_MAX_COMPONENTS];
    int ok;
} Builder;

static int net_color(int net)
{
    if (net <= NET_BUS7)
        return COLOR_BUS;
    if (net == NET_VCC)
        return COLOR_SUPPLY;
    if (net == NET_GND)
        return COLOR_RETURN;
    if (net == NET_CLOCK || net == NET_CLOCK_INVERTED)
        return COLOR_CLOCK;
    return COLOR_CONTROL;
}

static float net_x(int net)
{
    return CHANNEL_X + (float)net * CHANNEL_PITCH;
}

static int add_wire(Builder *k, Pin a, Pin c, int color, const WirePoint *knees, int count)
{
    if (!k->ok || bench_toggle_wire(k->b, a, c) != 1)
        return k->ok = 0;
    Wire *wire = &k->b->wires[k->b->wire_count - 1];
    wire->color = color;
    wire->knee_count = count;
    if (count)
        memcpy(wire->knees, knees, (size_t)count * sizeof(*knees));
    return 1;
}

static int place(Builder *k, PartType type, int row, int column)
{
    int index = k->ok ? bench_add(k->b, type, FIRST_COLUMN_X + (float)column * COLUMN_PITCH,
                                  FIRST_ROW_Y + (float)row * ROW_PITCH)
                      : -1;
    if (index < 0)
        return k->ok = 0, -1;
    k->row[index] = (unsigned char)row;
    return index;
}

/* Lane of a pin: outside its chip, in the gap of its row, unique among the pins on that side of the row. */
static float lane_y(Builder *k, int part, float pin_y)
{
    int side = pin_y > k->b->parts[part].y;
    float offset = LANE_START + (float)k->lanes[k->row[part]][side]++ * LANE_STEP;
    return side ? pin_y + offset : pin_y - offset;
}

static void attach(Builder *k, int net, int part, int pin)
{
    if (!k->ok || part < 0)
        return;
    Net *n = &k->nets[net];
    float px, py;
    bench_pin_position(&k->b->parts[part], pin, &px, &py);
    float y = lane_y(k, part, py);
    int junction = bench_add(k->b, PART_JUNCTION, net_x(net), y);
    if (junction < 0 || n->count == NET_JUNCTIONS)
    {
        k->ok = 0;
        return;
    }
    WirePoint knee = {px, y};
    add_wire(k, (Pin){part, pin}, (Pin){junction, 0}, net_color(net), &knee, 1);
    n->junctions[n->count++] = junction;
}

static void attach_all(Builder *k, int net, int part, const int *pins, int count)
{
    for (int i = 0; i < count; ++i)
        attach(k, net, part, pins[i]);
}

/* Short connection between two pins, routed through the first pin's lane. */
static void link(Builder *k, int part, int pin, int other, int other_pin, int color)
{
    if (!k->ok || part < 0 || other < 0)
        return;
    float ax, ay, bx, by;
    bench_pin_position(&k->b->parts[part], pin, &ax, &ay);
    bench_pin_position(&k->b->parts[other], other_pin, &bx, &by);
    float y = lane_y(k, part, ay);
    WirePoint knees[2] = {{ax, y}, {bx, y}};
    add_wire(k, (Pin){part, pin}, (Pin){other, other_pin}, color, knees, 2);
}

static Builder *sort_builder;

static int by_y(const void *a, const void *c)
{
    float ya = sort_builder->b->parts[*(const int *)a].y, yc = sort_builder->b->parts[*(const int *)c].y;
    return (ya > yc) - (ya < yc);
}

/* Joins the junctions of each net from top to bottom into one vertical line. */
static void draw_channels(Builder *k)
{
    sort_builder = k;
    for (int net = 0; net < NET_COUNT && k->ok; ++net)
    {
        Net *n = &k->nets[net];
        qsort(n->junctions, (size_t)n->count, sizeof(n->junctions[0]), by_y);
        for (int i = 1; i < n->count; ++i)
            add_wire(k, (Pin){n->junctions[i - 1], 0}, (Pin){n->junctions[i], 0}, net_color(net), NULL, 0);
    }
}

static void power(Builder *k, int part, int vcc, int gnd)
{
    attach(k, NET_VCC, part, vcc);
    attach(k, NET_GND, part, gnd);
}

/* 74HC173 with its own clock, data from bus bits [first, first+3], load enable `load` (active low). */
static int register_173(Builder *k, int row, int column, int first_bit, int load, int output_net)
{
    int reg = place(k, PART_74HC173, row, column);
    power(k, reg, P173_VCC, P173_GND);
    attach(k, NET_CLOCK, reg, P173_CLK);
    attach(k, NET_GND, reg, P173_CLR);
    attach(k, load, reg, P173_G1);
    attach(k, load, reg, P173_G2);
    attach(k, output_net, reg, P173_M);
    attach(k, output_net, reg, P173_N);
    for (int bit = 0; bit < 4; ++bit)
        attach(k, NET_BUS0 + first_bit + bit, reg, d173[bit]);
    return reg;
}

static int transceiver_245(Builder *k, int row, int column, int enable, int bits)
{
    int bus = place(k, PART_74HC245, row, column);
    power(k, bus, P245_VCC, P245_GND);
    attach(k, NET_VCC, bus, P245_DIR);
    attach(k, enable, bus, P245_OE);
    for (int bit = 0; bit < bits; ++bit)
        attach(k, NET_BUS0 + bit, bus, 17 - bit);
    return bus;
}

static int eeprom(Builder *k, int row, int column, const char *contents, int fixed_high)
{
    int rom = place(k, PART_AT28C16, row, column);
    if (k->ok && !bench_set_code(k->b, rom, contents))
        k->ok = 0;
    power(k, rom, P28_VCC, P28_GND);
    attach(k, NET_GND, rom, P28_CE);
    attach(k, NET_VCC, rom, P28_WE);
    for (int bit = fixed_high; bit < 11; ++bit)
        attach(k, NET_GND, rom, address28[bit]);
    return rom;
}

static unsigned microcode_step(int opcode, int step)
{
    static const unsigned fetch[2] = {CO | MI, RO | II | CE};
    if (step < 2)
        return fetch[step];
    static const unsigned lda[] = {IO | MI, RO | AI | SR};
    static const unsigned add[] = {IO | MI, RO | BI, EO | AI | SR};
    static const unsigned sub[] = {IO | MI, RO | BI, EO | AI | SU | SR};
    int i = step - 2;
    if (opcode == OP_LDA && i < 2)
        return lda[i];
    if ((opcode == OP_ADD || opcode == OP_SUB) && i < 3)
        return opcode == OP_ADD ? add[i] : sub[i];
    if (opcode == OP_OUT && i == 0)
        return AO | OI | SR;
    if (opcode == OP_HLT && i == 0)
        return HLT;
    return SR;
}

/* Microcode for one EEPROM: byte `half` (0 low, 1 high) of the stored word at address step + 8 * opcode. */
static int microcode_text(int half, char *text, size_t capacity)
{
    size_t used = 0;
    for (int address = 0; address < OPCODES * STEPS; ++address)
    {
        unsigned active = microcode_step(address / STEPS, address % STEPS);
        unsigned stored = ((active & ACTIVE_HIGH) | (~active & ~ACTIVE_HIGH & 0x3FFF) | UNUSED_BITS) >> (8 * half);
        int length = snprintf(text + used, capacity - used, "%s%02X", address % 16 ? " " : address ? "\n" : "",
                              stored & 0xFF);
        if (length < 0 || (size_t)length >= capacity - used)
            return 0;
        used += (size_t)length;
    }
    return 1;
}

static void clock_section(Builder *k)
{
    int clock = place(k, PART_CLOCK, 0, 0);
    if (k->ok && !bench_set_value(k->b, clock, COMPUTER_CLOCK_HZ))
        k->ok = 0;
    int gate = place(k, PART_74HC08, 0, 1), inverter = place(k, PART_74HC00, 0, 2);
    power(k, gate, PQUAD_VCC, PQUAD_GND);
    power(k, inverter, PQUAD_VCC, PQUAD_GND);
    attach(k, NET_GND, clock, 1);
    /* HLT stops the clock: the AND passes it only while HLT (active low) stays high. */
    link(k, clock, 0, gate, quad[0][0], COLOR_CLOCK);
    attach(k, NET_HLT_N, gate, quad[0][1]);
    attach(k, NET_CLOCK, gate, quad[0][2]);
    attach(k, NET_CLOCK, inverter, quad[0][0]);
    attach(k, NET_CLOCK, inverter, quad[0][1]);
    attach(k, NET_CLOCK_INVERTED, inverter, quad[0][2]);
    for (int g = 1; g < 4; ++g)
    {
        attach_all(k, NET_GND, gate, quad[g], 2);
        attach_all(k, NET_GND, inverter, quad[g], 2);
    }
}

static void program_counter(Builder *k)
{
    int pc = place(k, PART_74HC161, 0, 3), out = transceiver_245(k, 0, 4, NET_CO_N, 4);
    power(k, pc, P161_VCC, P161_GND);
    attach(k, NET_CLOCK, pc, P161_CLK);
    attach(k, NET_VCC, pc, P161_CLR);
    attach(k, NET_VCC, pc, P161_LOAD);
    attach(k, NET_CE, pc, P161_ENP);
    attach(k, NET_CE, pc, P161_ENT);
    for (int bit = 0; bit < 4; ++bit)
    {
        attach(k, NET_GND, pc, P161_A + bit);
        link(k, pc, P161_QA - bit, out, P245_A1 + bit, COLOR_SIGNAL);
        attach(k, NET_GND, out, P245_A1 + 4 + bit);
    }
}

/* Memory address register, program EEPROM and instruction register; returns the instruction register high
 * half, whose outputs address the microcode. */
static int memory_and_instruction(Builder *k)
{
    static const char program[] = "00: 1E 2F 3D E0 F0 ; LDA 14, ADD 15, SUB 13, OUT, HLT\n0D: 07 1C 0E ; 7, 28, 14\n";
    int mar = register_173(k, 1, 0, 0, NET_MI_N, NET_GND);
    int rom = eeprom(k, 1, 1, program, 4);
    attach(k, NET_RO_N, rom, P28_OE);
    for (int bit = 0; bit < 4; ++bit)
        link(k, mar, P173_Q1 + bit, rom, address28[bit], COLOR_SIGNAL);
    for (int bit = 0; bit < 8; ++bit)
        attach(k, NET_BUS0 + bit, rom, data28[bit]);
    int low = register_173(k, 1, 2, 0, NET_II_N, NET_IO_N);
    for (int bit = 0; bit < 4; ++bit)
        attach(k, NET_BUS0 + bit, low, P173_Q1 + bit);
    return register_173(k, 1, 3, 4, NET_II_N, NET_GND);
}

static void control(Builder *k, int instruction)
{
    static const int lines[2][8] = {{NET_HLT_N, NET_MI_N, NET_RO_N, NET_IO_N, NET_II_N, NET_AI_N, NET_AO_N, NET_EO_N},
                                    {NET_SU, NET_BI_N, NET_OI_N, NET_CE, NET_CO_N, NET_SR_N, -1, -1}};
    int step = place(k, PART_74HC161, 2, 0);
    power(k, step, P161_VCC, P161_GND);
    attach(k, NET_CLOCK_INVERTED, step, P161_CLK);
    attach(k, NET_VCC, step, P161_CLR);
    attach(k, NET_VCC, step, P161_ENP);
    attach(k, NET_VCC, step, P161_ENT);
    attach(k, NET_SR_N, step, P161_LOAD);
    for (int bit = 0; bit < 4; ++bit)
        attach(k, NET_GND, step, P161_A + bit);
    for (int half = 0; half < 2; ++half)
    {
        char text[MICROCODE_TEXT_CAPACITY];
        if (!microcode_text(half, text, sizeof(text)))
            k->ok = 0;
        int rom = eeprom(k, 2, 1 + half, text, 7);
        attach(k, NET_GND, rom, P28_OE);
        for (int bit = 0; bit < 3; ++bit)
            link(k, step, P161_QA - bit, rom, address28[bit], COLOR_SIGNAL);
        for (int bit = 0; bit < 4; ++bit)
            link(k, instruction, P173_Q1 + bit, rom, address28[3 + bit], COLOR_SIGNAL);
        for (int bit = 0; bit < 8; ++bit)
            if (lines[half][bit] >= 0)
                attach(k, lines[half][bit], rom, data28[bit]);
    }
}

/* A and B registers, adder with XOR for subtraction, and the drivers of A and of the sum onto the bus. */
static void arithmetic(Builder *k)
{
    int a[2], b[2], adder[2], xors[2];
    int a_out = transceiver_245(k, 3, 2, NET_AO_N, 8), sum_out = transceiver_245(k, 3, 3, NET_EO_N, 8);
    for (int half = 0; half < 2; ++half)
    {
        a[half] = register_173(k, 3, half, 4 * half, NET_AI_N, NET_GND);
        adder[half] = place(k, PART_74HC283, 4, half);
        xors[half] = place(k, PART_74HC86, 4, 2 + half);
        b[half] = register_173(k, 5, half, 4 * half, NET_BI_N, NET_GND);
        power(k, adder[half], P283_VCC, P283_GND);
        power(k, xors[half], PQUAD_VCC, PQUAD_GND);
    }
    attach(k, NET_SU, adder[0], P283_C0);
    link(k, adder[0], P283_C4, adder[1], P283_C0, COLOR_SIGNAL);
    for (int bit = 0; bit < 8; ++bit)
    {
        int half = bit / 4, i = bit % 4;
        link(k, a[half], P173_Q1 + i, adder[half], a283[i], COLOR_SIGNAL);
        link(k, a[half], P173_Q1 + i, a_out, P245_A1 + bit, COLOR_SIGNAL);
        link(k, b[half], P173_Q1 + i, xors[half], quad[i][0], COLOR_SIGNAL);
        attach(k, NET_SU, xors[half], quad[i][1]);
        link(k, xors[half], quad[i][2], adder[half], b283[i], COLOR_SIGNAL);
        link(k, adder[half], s283[i], sum_out, P245_A1 + bit, COLOR_SIGNAL);
    }
}

static void output(Builder *k)
{
    int out[2] = {register_173(k, 5, 2, 0, NET_OI_N, NET_GND), register_173(k, 5, 3, 4, NET_OI_N, NET_GND)};
    for (int bit = 0; bit < COMPUTER_OUTPUT_BITS && k->ok; ++bit)
    {
        float x = FIRST_COLUMN_X + 2 * COLUMN_PITCH + (float)bit * LED_PITCH, y = FIRST_ROW_Y + 6 * ROW_PITCH;
        int led = bench_add(k->b, PART_LED, x, y), load = bench_add(k->b, PART_RESISTOR, x, y + 120);
        if (led < 0 || load < 0 || !bench_set_value(k->b, load, LED_OHMS) || !bench_rotate_part(k->b, led) ||
            !bench_rotate_part(k->b, load))
        {
            k->ok = 0;
            return;
        }
        k->row[led] = k->row[load] = 6;
        link(k, out[bit / 4], P173_Q1 + bit % 4, led, 0, COLOR_SIGNAL);
        add_wire(k, (Pin){led, 1}, (Pin){load, 0}, COLOR_SIGNAL, NULL, 0);
        attach(k, NET_GND, load, 1);
    }
}

int workbench_build_computer(Workbench *bench, int *selected)
{
    static Builder k;
    memset(&k, 0, sizeof(k));
    k.b = bench;
    k.ok = 1;
    int source = bench_add(bench, PART_SOURCE, CHANNEL_X - 160, FIRST_ROW_Y);
    if (source < 0)
        return 0;
    k.row[source] = 0;
    clock_section(&k);
    program_counter(&k);
    control(&k, memory_and_instruction(&k));
    arithmetic(&k);
    output(&k);
    attach(&k, NET_VCC, source, 0);
    attach(&k, NET_GND, source, 1);
    draw_channels(&k);
    *selected = source;
    return k.ok;
}

int workbench_computer_output_led(const Workbench *bench, int bit)
{
    for (int i = 0, seen = 0; bench && i < bench->part_count; ++i)
        if (bench->parts[i].type == PART_LED && seen++ == bit)
            return i;
    return -1;
}
