#include "spice_live.h"
#include "netlist.h"
#include "component_registry.h"
#include "rom_image.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Each medium-scale chip runs in the continuous ngspice session the app uses. Outputs are read through a
 * 1 kOhm load to ground: about 5 mA when the pin drives high, nothing when it drives low or is 3-state. */
#define HIGH_AMPS 3e-3
#define LOW_AMPS 1e-4
#define CLOCK_HZ 1000.0

static const char *executable;

typedef struct
{
    Workbench bench;
    int source;
    float next_x;
} Rig;

static void rig_start(Rig *rig)
{
    memset(rig, 0, sizeof(*rig));
    rig->source = bench_add(&rig->bench, PART_SOURCE, 100, 100);
    rig->next_x = 100;
    assert(rig->source == 0);
}

static void wire(Rig *rig, int part, int pin, int other, int other_pin)
{
    assert(bench_toggle_wire(&rig->bench, (Pin){part, pin}, (Pin){other, other_pin}) == 1);
}

/* Ties a pin to the supply: 1 = VCC, 0 = GND. */
static void tie(Rig *rig, int part, int pin, int level)
{
    wire(rig, part, pin, rig->source, level ? 0 : 1);
}

static int chip(Rig *rig, PartType type)
{
    int index = bench_add(&rig->bench, type, 600, 600);
    const ComponentPinInfo *info;
    assert(index >= 0);
    for (int pin = 0; (info = component_pin_info(type, pin)) != NULL && pin < bench_pin_count(type); ++pin)
        if (info->role == PIN_SUPPLY_POSITIVE)
            tie(rig, index, pin, 1);
        else if (info->role == PIN_GROUND)
            tie(rig, index, pin, 0);
    return index;
}

/* A 1 kOhm load from an output pin to GND, placed apart so it touches nothing. */
static int probe(Rig *rig, int part, int pin)
{
    rig->next_x += 150;
    int load = bench_add(&rig->bench, PART_RESISTOR, rig->next_x, 1200);
    assert(load >= 0 && bench_set_value(&rig->bench, load, 1000));
    wire(rig, part, pin, load, 0);
    wire(rig, load, 1, rig->source, 1);
    return load;
}

static SpiceLive *run(Rig *rig, double seconds, SimAutoResult *result, SpiceLive *live)
{
    SimAutoOptions options = {1, {0, 0}, 1e-5, .005, burn_limits_default(), {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
    if (!live)
        live = spice_live_create();
    assert(live);
    int success = spice_live_step(live, &rig->bench, executable, &options, seconds, result);
    if (!success)
        fprintf(stderr, "logic chips: %s\n", result->message);
    assert(success && result->burnt_count == 0);
    return live;
}

/* Reads loads as a binary number, least significant first; a load between the levels is an error. */
static int read_bits(const SimAutoResult *result, const int *loads, int count)
{
    int value = 0;
    for (int bit = 0; bit < count; ++bit)
    {
        double amps = result->currents[loads[bit]];
        assert(amps > HIGH_AMPS || amps < LOW_AMPS);
        value |= (amps > HIGH_AMPS) << bit;
    }
    return value;
}

/* 5 + 3 with carry in 1 gives 9 and no carry out; inputs tied, outputs read through loads. */
static void adder_adds(void)
{
    static Rig rig;
    SimAutoResult result;
    rig_start(&rig);
    int adder = chip(&rig, PART_74HC283);
    const int a_pins[] = {4, 2, 13, 11}, b_pins[] = {5, 1, 14, 10}, sum_pins[] = {3, 0, 12, 9};
    int loads[5];
    for (int bit = 0; bit < 4; ++bit)
    {
        tie(&rig, adder, a_pins[bit], 5 >> bit & 1);
        tie(&rig, adder, b_pins[bit], 3 >> bit & 1);
        loads[bit] = probe(&rig, adder, sum_pins[bit]);
    }
    tie(&rig, adder, 6, 1);
    loads[4] = probe(&rig, adder, 8);
    SpiceLive *live = run(&rig, 1e-4, &result, NULL);
    assert(read_bits(&result, loads, 5) == 9);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* The counter advances once per clock period and wraps from 15 to 0; RCO is high only at 15. */
static void counter_counts(void)
{
    static Rig rig;
    SimAutoResult result;
    rig_start(&rig);
    int counter = chip(&rig, PART_74HC161);
    int clock = bench_add(&rig.bench, PART_CLOCK, 300, 900);
    assert(clock >= 0 && bench_set_value(&rig.bench, clock, CLOCK_HZ));
    wire(&rig, clock, 0, counter, 1);
    wire(&rig, clock, 1, rig.source, 1);
    const int high_inputs[] = {0, 6, 8, 9}, data[] = {2, 3, 4, 5}, q_pins[] = {13, 12, 11, 10};
    for (int i = 0; i < 4; ++i)
    {
        tie(&rig, counter, high_inputs[i], 1);
        tie(&rig, counter, data[i], 0);
    }
    int loads[4], carry = probe(&rig, counter, 14);
    for (int bit = 0; bit < 4; ++bit)
        loads[bit] = probe(&rig, counter, q_pins[bit]);
    /* Read in the middle of the low half of each period, far from the rising edges. */
    SpiceLive *live = run(&rig, .75 / CLOCK_HZ, &result, NULL);
    int first = read_bits(&result, loads, 4), seen_carry = 0;
    for (int period = 1; period <= 17; ++period)
    {
        live = run(&rig, 1 / CLOCK_HZ, &result, live);
        int value = read_bits(&result, loads, 4);
        assert(value == (first + period) % 16);
        int rco = result.currents[carry] > HIGH_AMPS;
        assert(rco == (value == 15));
        seen_carry |= rco;
    }
    assert(seen_carry);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* With the data enables low the register takes D on a clock edge; M high releases the outputs. */
static void register_loads_and_releases(void)
{
    static Rig rig;
    SimAutoResult result;
    rig_start(&rig);
    int reg = chip(&rig, PART_74HC173);
    int clock = bench_add(&rig.bench, PART_CLOCK, 300, 900);
    assert(clock >= 0 && bench_set_value(&rig.bench, clock, CLOCK_HZ));
    wire(&rig, clock, 0, reg, 6);
    wire(&rig, clock, 1, rig.source, 1);
    const int data[] = {13, 12, 11, 10}, q_pins[] = {2, 3, 4, 5};
    int loads[4];
    for (int bit = 0; bit < 4; ++bit)
    {
        tie(&rig, reg, data[bit], 0xA >> bit & 1);
        loads[bit] = probe(&rig, reg, q_pins[bit]);
    }
    const int low_controls[] = {0, 1, 8, 9, 14};
    for (int i = 0; i < 5; ++i)
        tie(&rig, reg, low_controls[i], 0);
    SpiceLive *live = run(&rig, 1.75 / CLOCK_HZ, &result, NULL);
    assert(read_bits(&result, loads, 4) == 0xA);
    spice_live_destroy(live);
    /* Output control M high: every output is 3-state, so no load sees current. */
    assert(bench_toggle_wire(&rig.bench, (Pin){reg, 0}, (Pin){rig.source, 1}) == 2);
    tie(&rig, reg, 0, 1);
    live = run(&rig, 1.75 / CLOCK_HZ, &result, NULL);
    assert(read_bits(&result, loads, 4) == 0);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* DIR high and OE low copy A to B; OE high leaves B released. */
static void transceiver_passes_a_to_b(void)
{
    static Rig rig;
    SimAutoResult result;
    rig_start(&rig);
    int bus = chip(&rig, PART_74HC245);
    int loads[8];
    for (int bit = 0; bit < 8; ++bit)
    {
        tie(&rig, bus, 1 + bit, 0x96 >> bit & 1);
        loads[bit] = probe(&rig, bus, 17 - bit);
    }
    tie(&rig, bus, 0, 1);
    tie(&rig, bus, 18, 0);
    SpiceLive *live = run(&rig, 1e-4, &result, NULL);
    assert(read_bits(&result, loads, 8) == 0x96);
    spice_live_destroy(live);
    assert(bench_toggle_wire(&rig.bench, (Pin){bus, 18}, (Pin){rig.source, 1}) == 2);
    tie(&rig, bus, 18, 1);
    live = run(&rig, 1e-4, &result, NULL);
    assert(read_bits(&result, loads, 8) == 0);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* The EEPROM reads its text: address 2 holds 5A; an address never written reads FF; bad text is refused. */
static void eeprom_reads_contents(void)
{
    static Rig rig;
    SimAutoResult result;
    rig_start(&rig);
    int rom = chip(&rig, PART_AT28C16);
    assert(bench_set_code(&rig.bench, rom, "00: 11 22 5A ; third byte\n"));
    const int address_pins[] = {7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18};
    const int data_pins[] = {8, 9, 10, 12, 13, 14, 15, 16};
    int loads[8];
    for (int bit = 0; bit < 11; ++bit)
        tie(&rig, rom, address_pins[bit], 2 >> bit & 1);
    for (int bit = 0; bit < 8; ++bit)
        loads[bit] = probe(&rig, rom, data_pins[bit]);
    tie(&rig, rom, 17, 0);
    tie(&rig, rom, 19, 0);
    tie(&rig, rom, 20, 1);
    SpiceLive *live = run(&rig, 1e-4, &result, NULL);
    assert(read_bits(&result, loads, 8) == 0x5A);
    spice_live_destroy(live);
    assert(bench_set_code(&rig.bench, rom, "7F0: 00"));
    live = run(&rig, 1e-4, &result, NULL);
    assert(read_bits(&result, loads, 8) == 0xFF);
    spice_live_destroy(live);
    assert(bench_set_code(&rig.bench, rom, "00: 1G"));
    FILE *sink = tmpfile();
    assert(sink && netlist_write_live(sink, &rig.bench, 0) == NETLIST_INVALID);
    fclose(sink);
    bench_snapshot_clear(&rig.bench);
}

static int control_source(Rig *rig, int part, int pin, int high, float x)
{
    int source = bench_add(&rig->bench, PART_SOURCE, x, 1500);
    assert(source >= 0 && bench_set_on(&rig->bench, source, high));
    wire(rig, source, 1, rig->source, 1);
    wire(rig, source, 0, part, pin);
    return source;
}

static void eeprom_writes_and_retains(void)
{
    static Rig rig;
    static SimAutoResult result;
    rig_start(&rig);
    int rom = chip(&rig, PART_AT28C16);
    const int address_pins[] = {7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18};
    const int data_pins[] = {8, 9, 10, 12, 13, 14, 15, 16};
    for (int bit = 1; bit < 11; ++bit)
        tie(&rig, rom, address_pins[bit], 2 >> bit & 1);
    int address = control_source(&rig, rom, 7, 0, 100);
    int oe = control_source(&rig, rom, 19, 1, 300);
    int we = control_source(&rig, rom, 20, 1, 500);
    tie(&rig, rom, 17, 0);
    int loads[8], switches[8];
    for (int bit = 0; bit < 8; ++bit)
    {
        loads[bit] = probe(&rig, rom, data_pins[bit]);
        switches[bit] = bench_add(&rig.bench, PART_SWITCH, 100 + bit * 150, 1800);
        assert(switches[bit] >= 0 && bench_set_on(&rig.bench, switches[bit], 1));
        control_source(&rig, switches[bit], 0, 0x5A >> bit & 1, 700 + bit * 150);
        wire(&rig, switches[bit], 1, rom, data_pins[bit]);
    }
    SpiceLive *live = run(&rig, .006, &result, NULL);
    assert(bench_set_on(&rig.bench, we, 0));
    run(&rig, .0000005, &result, live);
    assert(bench_set_on(&rig.bench, we, 1));
    run(&rig, .0000005, &result, live);
    for (int bit = 0; bit < 8; ++bit)
        assert(bench_set_on(&rig.bench, switches[bit], 0));
    assert(bench_set_on(&rig.bench, oe, 0));
    run(&rig, .000001, &result, live);
    assert(read_bits(&result, loads, 8) & 128);
    run(&rig, .002, &result, live);
    assert(read_bits(&result, loads, 8) == 0x5A);
    char message[256];
    assert(spice_live_sync_memory(live, &rig.bench, message, sizeof(message)));
    RomImage image;
    assert(rom_image_parse(rig.bench.parts[rom].code, &image, NULL));
    assert(image.data[2] == 0x5A && image.data[3] == 0xFF);
    assert(bench_set_on(&rig.bench, address, 1));
    run(&rig, .0001, &result, live);
    assert(!spice_live_restarted(live));
    assert(read_bits(&result, loads, 8) == 0xFF);
    assert(bench_set_on(&rig.bench, address, 0));
    run(&rig, .0001, &result, live);
    assert(read_bits(&result, loads, 8) == 0x5A);
    assert(bench_set_on(&rig.bench, rig.source, 0));
    run(&rig, .001, &result, live);
    assert(bench_set_on(&rig.bench, rig.source, 1));
    run(&rig, .001, &result, live);
    assert(read_bits(&result, loads, 8) == 0x5A);
    spice_live_destroy(live);
    live = run(&rig, .001, &result, NULL);
    assert(read_bits(&result, loads, 8) == 0x5A);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
    puts("EEPROM bus: writes 5A at 2, polls busy, reads FF at 3, retains across power and solver restart.");
}

/* Inputs 1, 0, 1, 0, 1, 0 on the six inverters read back as 0, 1, 0, 1, 0, 1. */
static void hex_inverter_inverts(void)
{
    static Rig rig;
    SimAutoResult result;
    rig_start(&rig);
    int inverter = chip(&rig, PART_74HC04);
    const int a_pins[] = {0, 2, 4, 8, 10, 12}, y_pins[] = {1, 3, 5, 7, 9, 11};
    int loads[6];
    for (int gate = 0; gate < 6; ++gate)
    {
        tie(&rig, inverter, a_pins[gate], gate % 2 == 0);
        loads[gate] = probe(&rig, inverter, y_pins[gate]);
    }
    SpiceLive *live = run(&rig, 1e-4, &result, NULL);
    assert(read_bits(&result, loads, 6) == 0x2A);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* Flip-flop 1 captures D=1 on the clock edge (Q=1, ~Q=0). Flip-flop 2 has ~CLR low: Q=0 whatever D and the
 * clock do. Then ~PRE and ~CLR low together drive both outputs of flip-flop 2 high (datasheet table). */
static void dual_dff_captures_and_overrides(void)
{
    static Rig rig;
    SimAutoResult result;
    rig_start(&rig);
    int ff = chip(&rig, PART_74HC74);
    int clock = bench_add(&rig.bench, PART_CLOCK, 300, 900);
    assert(clock >= 0 && bench_set_value(&rig.bench, clock, CLOCK_HZ));
    wire(&rig, clock, 1, rig.source, 1);
    wire(&rig, clock, 0, ff, 2);
    wire(&rig, clock, 0, ff, 10);
    tie(&rig, ff, 0, 1);
    tie(&rig, ff, 3, 1);
    tie(&rig, ff, 1, 1);
    tie(&rig, ff, 9, 1);
    tie(&rig, ff, 11, 1);
    tie(&rig, ff, 12, 0);
    const int outputs[] = {4, 5, 8, 7};
    int loads[4];
    for (int i = 0; i < 4; ++i)
        loads[i] = probe(&rig, ff, outputs[i]);
    SpiceLive *live = run(&rig, 1.75 / CLOCK_HZ, &result, NULL);
    assert(read_bits(&result, loads, 4) == 0x9); /* 1Q=1, ~1Q=0, 2Q=0, ~2Q=1 */
    spice_live_destroy(live);
    assert(bench_toggle_wire(&rig.bench, (Pin){ff, 9}, (Pin){rig.source, 0}) == 2);
    tie(&rig, ff, 9, 0);
    live = run(&rig, 1.75 / CLOCK_HZ, &result, NULL);
    assert(read_bits(&result, loads, 4) == 0xD); /* 2Q=1 and ~2Q=1 */
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

static void probes(Rig *rig, int part, const int *pins, int count, int *loads)
{
    for (int i = 0; i < count; ++i)
        loads[i] = probe(rig, part, pins[i]);
}

/* Runs a fresh session for 0.1 ms and reads the loads. */
static int settle(Rig *rig, const int *loads, int count)
{
    static SimAutoResult result;
    SpiceLive *live = run(rig, 1e-4, &result, NULL);
    int value = read_bits(&result, loads, count);
    spice_live_destroy(live);
    return value;
}

/* Ties each listed pin to bit i of `levels`. */
static void tie_bits(Rig *rig, int part, const int *pins, int count, unsigned levels)
{
    for (int i = 0; i < count; ++i)
        tie(rig, part, pins[i], levels >> i & 1);
}

/* NOR gates with A,B = 00, 01, 10, 11: only the first output is high. */
static void quad_nor_nors(void)
{
    static Rig rig;
    rig_start(&rig);
    int nor = chip(&rig, PART_74HC02);
    const int a_pins[] = {1, 4, 7, 10}, b_pins[] = {2, 5, 8, 11}, y_pins[] = {0, 3, 9, 12};
    int loads[4];
    tie_bits(&rig, nor, a_pins, 4, 0xC);
    tie_bits(&rig, nor, b_pins, 4, 0xA);
    probes(&rig, nor, y_pins, 4, loads);
    assert(settle(&rig, loads, 4) == 0x1);
    bench_snapshot_clear(&rig.bench);
}

/* Gates fed 111, 110 and 011: the 74HC10 answers 0, 1, 1 and the 74HC11 answers 1, 0, 0. */
static void triple_gates_decide(PartType type, int expected)
{
    static Rig rig;
    rig_start(&rig);
    int gates = chip(&rig, type);
    const int in_pins[] = {0, 1, 12, 2, 3, 4, 8, 9, 10}, y_pins[] = {11, 5, 7};
    int loads[3];
    tie_bits(&rig, gates, in_pins, 9, 0x19F); /* gate 1: 111, gate 2: 110 (C low), gate 3: 011 (A low) */
    probes(&rig, gates, y_pins, 3, loads);
    assert(settle(&rig, loads, 3) == expected);
    bench_snapshot_clear(&rig.bench);
}

/* Sets the input source and reads the inverter output in the same session. */
static int schmitt_at(Rig *rig, int source, double volts, int load, SpiceLive **live)
{
    static SimAutoResult result;
    assert(bench_set_value(&rig->bench, source, volts));
    *live = run(rig, 1e-4, &result, *live);
    assert(!spice_live_restarted(*live));
    return read_bits(&result, &load, 1);
}

/* 2.25 V lies between the thresholds: the output keeps what the last crossing set. Rising past 2.75 V
 * drives it low; falling below 1.75 V drives it high. */
static void schmitt_holds_between_thresholds(void)
{
    static Rig rig;
    static SimAutoResult result;
    rig_start(&rig);
    int inverter = chip(&rig, PART_74HC14);
    int source = bench_add(&rig.bench, PART_SOURCE, 300, 1500);
    assert(source >= 0 && bench_set_value(&rig.bench, source, .5));
    wire(&rig, source, 1, rig.source, 1);
    wire(&rig, source, 0, inverter, 0);
    int load = probe(&rig, inverter, 1);
    SpiceLive *live = run(&rig, 1e-4, &result, NULL);
    assert(read_bits(&result, &load, 1) == 1);
    assert(schmitt_at(&rig, source, 2.25, load, &live) == 1);
    assert(schmitt_at(&rig, source, 2.6, load, &live) == 1);
    assert(schmitt_at(&rig, source, 3.0, load, &live) == 0);
    assert(schmitt_at(&rig, source, 2.25, load, &live) == 0);
    assert(schmitt_at(&rig, source, 1.9, load, &live) == 0);
    assert(schmitt_at(&rig, source, 1.5, load, &live) == 1);
    assert(schmitt_at(&rig, source, 2.25, load, &live) == 1);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* Continues the session (starting it when *live is NULL) and reads the loads. */
static int step(Rig *rig, SpiceLive **live, double seconds, const int *loads, int count)
{
    static SimAutoResult result;
    int fresh = *live == NULL;
    *live = run(rig, seconds, &result, *live);
    assert(fresh || !spice_live_restarted(*live));
    return read_bits(&result, loads, count);
}

static int add_clock(Rig *rig, int part, int pin)
{
    int clock = bench_add(&rig->bench, PART_CLOCK, 300, 900);
    assert(clock >= 0 && bench_set_value(&rig->bench, clock, CLOCK_HZ));
    wire(rig, clock, 1, rig->source, 1);
    wire(rig, clock, 0, part, pin);
    return clock;
}

static const int octal_d[] = {2, 3, 6, 7, 12, 13, 16, 17}, octal_q[] = {1, 4, 5, 8, 11, 14, 15, 18};

/* D1-D4 tied to 0101 (bit 0 first); D5-D8 share one switchable source. */
static int octal_rig(Rig *rig, PartType type, int *loads, int *data)
{
    rig_start(rig);
    int part = chip(rig, type);
    tie_bits(rig, part, octal_d, 4, 0x5);
    *data = control_source(rig, part, octal_d[4], 1, 100);
    for (int bit = 5; bit < 8; ++bit)
        wire(rig, *data, 0, part, octal_d[bit]);
    probes(rig, part, octal_q, 8, loads);
    return part;
}

/* With ~CLR high the clock loads F5; ~CLR low clears at once, clock or not; released, the next edge reloads. */
static void octal_dff_273_clears(void)
{
    static Rig rig;
    int loads[8], data;
    int reg = octal_rig(&rig, PART_74HC273, loads, &data);
    add_clock(&rig, reg, 10);
    int clear = control_source(&rig, reg, 0, 1, 300);
    SpiceLive *live = NULL;
    assert(step(&rig, &live, 1.75 / CLOCK_HZ, loads, 8) == 0xF5);
    assert(bench_set_on(&rig.bench, clear, 0));
    assert(step(&rig, &live, .1 / CLOCK_HZ, loads, 8) == 0);
    assert(step(&rig, &live, 1 / CLOCK_HZ, loads, 8) == 0);
    assert(bench_set_on(&rig.bench, clear, 1));
    assert(step(&rig, &live, 1 / CLOCK_HZ, loads, 8) == 0xF5);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* LE high: Q follows D. LE low: Q holds while D changes. ~OE high releases Q; low again shows the held bits. */
static void octal_latch_373_follows_and_holds(void)
{
    static Rig rig;
    int loads[8], data;
    int latch = octal_rig(&rig, PART_74HC373, loads, &data);
    int enable = control_source(&rig, latch, 10, 1, 300);
    int release = control_source(&rig, latch, 0, 0, 500);
    SpiceLive *live = NULL;
    assert(step(&rig, &live, 1e-4, loads, 8) == 0xF5);
    assert(bench_set_on(&rig.bench, data, 0));
    assert(step(&rig, &live, 1e-4, loads, 8) == 0x05);
    assert(bench_set_on(&rig.bench, enable, 0));
    assert(bench_set_on(&rig.bench, data, 1));
    assert(step(&rig, &live, 1e-4, loads, 8) == 0x05);
    assert(bench_set_on(&rig.bench, release, 1));
    assert(step(&rig, &live, 1e-4, loads, 8) == 0);
    assert(bench_set_on(&rig.bench, release, 0));
    assert(step(&rig, &live, 1e-4, loads, 8) == 0x05);
    assert(bench_set_on(&rig.bench, enable, 1));
    assert(step(&rig, &live, 1e-4, loads, 8) == 0xF5);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* The clock loads F5; ~OE high releases Q while the clock goes on loading 05; ~OE low shows 05. */
static void octal_dff_374_loads_while_released(void)
{
    static Rig rig;
    int loads[8], data;
    int reg = octal_rig(&rig, PART_74HC374, loads, &data);
    add_clock(&rig, reg, 10);
    int release = control_source(&rig, reg, 0, 0, 300);
    SpiceLive *live = NULL;
    assert(step(&rig, &live, 1.75 / CLOCK_HZ, loads, 8) == 0xF5);
    assert(bench_set_on(&rig.bench, release, 1));
    assert(bench_set_on(&rig.bench, data, 0));
    assert(step(&rig, &live, 1 / CLOCK_HZ, loads, 8) == 0);
    assert(bench_set_on(&rig.bench, release, 0));
    assert(step(&rig, &live, .1 / CLOCK_HZ, loads, 8) == 0x05);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* Counting, then ~LOAD low: nothing until the next edge, which loads 9. ~CLR low with ~LOAD still low:
 * nothing until the next edge, which clears (clear beats load). Released, it counts again. */
static void counter_163_waits_for_the_edge(void)
{
    static Rig rig;
    rig_start(&rig);
    int counter = chip(&rig, PART_74HC163);
    add_clock(&rig, counter, 1);
    const int enables[] = {6, 9}, data[] = {2, 3, 4, 5}, q_pins[] = {13, 12, 11, 10};
    tie_bits(&rig, counter, enables, 2, 0x3);
    tie_bits(&rig, counter, data, 4, 0x9);
    int clear = control_source(&rig, counter, 0, 1, 100);
    int load = control_source(&rig, counter, 8, 1, 300);
    int loads[4];
    probes(&rig, counter, q_pins, 4, loads);
    SpiceLive *live = NULL;
    int first = step(&rig, &live, .75 / CLOCK_HZ, loads, 4);
    int counted = step(&rig, &live, 1 / CLOCK_HZ, loads, 4);
    assert(counted == (first + 1) % 16);
    assert(bench_set_on(&rig.bench, load, 0));
    assert(step(&rig, &live, .1 / CLOCK_HZ, loads, 4) == counted);
    assert(step(&rig, &live, 1 / CLOCK_HZ, loads, 4) == 9);
    assert(bench_set_on(&rig.bench, clear, 0));
    assert(step(&rig, &live, .1 / CLOCK_HZ, loads, 4) == 9);
    assert(step(&rig, &live, 1 / CLOCK_HZ, loads, 4) == 0);
    assert(bench_set_on(&rig.bench, clear, 1));
    assert(bench_set_on(&rig.bench, load, 1));
    assert(step(&rig, &live, 1 / CLOCK_HZ, loads, 4) == 1);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* A 74HC193 with its clock on UP (count up) or on DOWN (count down); the other clock pin tied high. Data A-D
 * is 12. Loads: QA-QD, then ~CO (up) or ~BO (down). */
static int up_down_rig(Rig *rig, int up, int *loads)
{
    rig_start(rig);
    int counter = chip(rig, PART_74HC193);
    const int data[] = {14, 0, 9, 8}, outputs[] = {2, 1, 5, 6, up ? 11 : 12};
    add_clock(rig, counter, up ? 4 : 3);
    tie(rig, counter, up ? 3 : 4, 1);
    tie_bits(rig, counter, data, 4, 0xC);
    probes(rig, counter, outputs, 5, loads);
    return counter;
}

/* Up: one step per UP edge, wrapping 15 to 0; ~CO is low only at 15 (read while UP is low). ~LOAD low loads 12
 * at once, between edges. CLR high clears at once and beats ~LOAD. */
static void up_down_193_counts_up_loads_and_clears(void)
{
    static Rig rig;
    int loads[5];
    int counter = up_down_rig(&rig, 1, loads);
    int clear = control_source(&rig, counter, 13, 0, 100);
    int load = control_source(&rig, counter, 10, 1, 300);
    SpiceLive *live = NULL;
    int first = step(&rig, &live, .75 / CLOCK_HZ, loads, 5) & 15, seen_carry = 0;
    for (int period = 1; period <= 17; ++period)
    {
        int bits = step(&rig, &live, 1 / CLOCK_HZ, loads, 5), value = bits & 15;
        assert(value == (first + period) % 16);
        assert(!(bits & 16) == (value == 15));
        seen_carry |= value == 15;
    }
    assert(seen_carry);
    assert(bench_set_on(&rig.bench, load, 0));
    assert((step(&rig, &live, .1 / CLOCK_HZ, loads, 5) & 15) == 12);
    assert(bench_set_on(&rig.bench, clear, 1));
    assert((step(&rig, &live, .1 / CLOCK_HZ, loads, 5) & 15) == 0);
    assert(bench_set_on(&rig.bench, clear, 0));
    assert((step(&rig, &live, .1 / CLOCK_HZ, loads, 5) & 15) == 12);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* Down: one step per DOWN edge, wrapping 0 to 15; ~BO is low only at 0 (read while DOWN is low). */
static void up_down_193_counts_down(void)
{
    static Rig rig;
    int loads[5];
    int counter = up_down_rig(&rig, 0, loads);
    tie(&rig, counter, 13, 0);
    tie(&rig, counter, 10, 1);
    SpiceLive *live = NULL;
    int first = step(&rig, &live, .75 / CLOCK_HZ, loads, 5) & 15, seen_borrow = 0;
    for (int period = 1; period <= 17; ++period)
    {
        int bits = step(&rig, &live, 1 / CLOCK_HZ, loads, 5), value = bits & 15;
        assert(value == (first + 16 * 2 - period) % 16);
        assert(!(bits & 16) == (value == 0));
        seen_borrow |= value == 0;
    }
    assert(seen_borrow);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* One switchable source per pin, all starting low. */
static void control_sources(Rig *rig, int part, const int *pins, int count, int *sources)
{
    for (int i = 0; i < count; ++i)
        sources[i] = control_source(rig, part, pins[i], 0, 100 + 150 * (float)i);
}

/* Sets the sources to the bits of `value`, bit 0 first. */
static void set_code(Rig *rig, const int *sources, int count, unsigned value)
{
    for (int i = 0; i < count; ++i)
        assert(bench_set_on(&rig->bench, sources[i], value >> i & 1));
}

/* Select 5 with G1 high and ~G2A, ~G2B low: only ~Y5 goes low. G1 low, or ~G2A high, keeps all high. */
static void decoder_138_selects_one(void)
{
    static Rig rig;
    rig_start(&rig);
    int dec = chip(&rig, PART_74HC138);
    const int select[] = {0, 1, 2}, enables[] = {5, 3, 4}, y_pins[] = {14, 13, 12, 11, 10, 9, 8, 6};
    int sources[3], loads[8];
    tie_bits(&rig, dec, select, 3, 5);
    control_sources(&rig, dec, enables, 3, sources);
    probes(&rig, dec, y_pins, 8, loads);
    SpiceLive *live = NULL;
    assert(step(&rig, &live, 1e-4, loads, 8) == 0xFF);
    set_code(&rig, sources, 3, 0x1);
    assert(step(&rig, &live, 1e-4, loads, 8) == 0xDF);
    set_code(&rig, sources, 3, 0x3);
    assert(step(&rig, &live, 1e-4, loads, 8) == 0xFF);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* Decoder 1 enabled with BA = 2: only ~1Y2 low. Decoder 2 disabled: all high; enabled with BA = 1: ~2Y1 low. */
static void dual_decoder_139_selects_per_half(void)
{
    static Rig rig;
    rig_start(&rig);
    int dec = chip(&rig, PART_74HC139);
    const int select1[] = {1, 2}, select2[] = {13, 12}, y_pins[] = {3, 4, 5, 6, 11, 10, 9, 8};
    int loads[8];
    tie(&rig, dec, 0, 0);
    tie_bits(&rig, dec, select1, 2, 2);
    tie_bits(&rig, dec, select2, 2, 1);
    int enable2 = control_source(&rig, dec, 14, 1, 100);
    probes(&rig, dec, y_pins, 8, loads);
    SpiceLive *live = NULL;
    assert(step(&rig, &live, 1e-4, loads, 8) == 0xFB);
    assert(bench_set_on(&rig.bench, enable2, 0));
    assert(step(&rig, &live, 1e-4, loads, 8) == 0xDB);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* Data 10010110 on D7-D0; each select code CBA routes its bit to Y, and ~W is the opposite. ~G high: Y low. */
static void multiplexer_151_routes(void)
{
    static Rig rig;
    rig_start(&rig);
    int mux = chip(&rig, PART_74HC151);
    const int data[] = {3, 2, 1, 0, 14, 13, 12, 11}, select[] = {10, 9, 8}, outputs[] = {4, 5};
    int sources[3], loads[2];
    tie_bits(&rig, mux, data, 8, 0x96);
    control_sources(&rig, mux, select, 3, sources);
    int strobe = control_source(&rig, mux, 6, 0, 600);
    probes(&rig, mux, outputs, 2, loads);
    SpiceLive *live = NULL;
    for (int code = 0; code < 8; ++code)
    {
        set_code(&rig, sources, 3, (unsigned)code);
        int bit = 0x96 >> code & 1;
        assert(step(&rig, &live, 1e-4, loads, 2) == (bit ? 1 : 2));
    }
    assert(bench_set_on(&rig.bench, strobe, 1));
    assert(step(&rig, &live, 1e-4, loads, 2) == 2);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* A inputs 0101 and B inputs 1010: A/~B low shows A, high shows B; ~G high drives every Y low. */
static void quad_multiplexer_157_switches(void)
{
    static Rig rig;
    rig_start(&rig);
    int mux = chip(&rig, PART_74HC157);
    const int a_pins[] = {1, 4, 10, 13}, b_pins[] = {2, 5, 9, 12}, y_pins[] = {3, 6, 8, 11};
    int loads[4];
    tie_bits(&rig, mux, a_pins, 4, 0x5);
    tie_bits(&rig, mux, b_pins, 4, 0xA);
    int select = control_source(&rig, mux, 0, 0, 100);
    int strobe = control_source(&rig, mux, 14, 0, 300);
    probes(&rig, mux, y_pins, 4, loads);
    SpiceLive *live = NULL;
    assert(step(&rig, &live, 1e-4, loads, 4) == 0x5);
    assert(bench_set_on(&rig.bench, select, 1));
    assert(step(&rig, &live, 1e-4, loads, 4) == 0xA);
    assert(bench_set_on(&rig.bench, strobe, 1));
    assert(step(&rig, &live, 1e-4, loads, 4) == 0);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* Segments lit for each BCD code, as a person reads the display; 10-15 are dark. */
static int segments(const char *lit)
{
    int mask = 0;
    for (const char *c = lit; *c; ++c)
        mask |= 1 << (*c - 'a');
    return mask;
}

/* Every code shows its digit (6 and 9 without tails, 10-15 dark). LE high holds 3 while the inputs read 8.
 * ~BL low darkens it; ~LT low lights all segments even then. Released, the held 3 returns; LE low shows 8. */
static void bcd_decoder_4511_shows_digits(void)
{
    static const char *const digits[16] = {"abcdef", "bc", "abdeg", "abcdg", "bcfg", "acdfg", "cdefg", "abc",
                                           "abcdefg", "abcfg", "", "", "", "", "", ""};
    static Rig rig;
    rig_start(&rig);
    int dec = chip(&rig, PART_74HC4511);
    const int bcd[] = {6, 0, 1, 5}, segment_pins[] = {12, 11, 10, 9, 8, 14, 13}; /* a-g */
    int sources[4], loads[7];
    control_sources(&rig, dec, bcd, 4, sources);
    int latch = control_source(&rig, dec, 4, 0, 700);
    int blank = control_source(&rig, dec, 3, 1, 850);
    int test = control_source(&rig, dec, 2, 1, 1000);
    probes(&rig, dec, segment_pins, 7, loads);
    SpiceLive *live = NULL;
    for (int code = 0; code < 16; ++code)
    {
        set_code(&rig, sources, 4, (unsigned)code);
        assert(step(&rig, &live, 1e-4, loads, 7) == segments(digits[code]));
    }
    set_code(&rig, sources, 4, 3);
    step(&rig, &live, 1e-4, loads, 7);
    assert(bench_set_on(&rig.bench, latch, 1));
    assert(step(&rig, &live, 1e-4, loads, 7) == segments(digits[3]));
    set_code(&rig, sources, 4, 8);
    assert(step(&rig, &live, 1e-4, loads, 7) == segments(digits[3]));
    assert(bench_set_on(&rig.bench, blank, 0));
    assert(step(&rig, &live, 1e-4, loads, 7) == 0);
    assert(bench_set_on(&rig.bench, test, 0));
    assert(step(&rig, &live, 1e-4, loads, 7) == 0x7F);
    assert(bench_set_on(&rig.bench, test, 1));
    assert(bench_set_on(&rig.bench, blank, 1));
    assert(step(&rig, &live, 1e-4, loads, 7) == segments(digits[3]));
    assert(bench_set_on(&rig.bench, latch, 0));
    assert(step(&rig, &live, 1e-4, loads, 7) == segments(digits[8]));
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    executable = argv[1];
    adder_adds();
    transceiver_passes_a_to_b();
    eeprom_reads_contents();
    eeprom_writes_and_retains();
    register_loads_and_releases();
    counter_counts();
    hex_inverter_inverts();
    dual_dff_captures_and_overrides();
    puts("Logic ICs: 74HC04 inverts, 74HC74 captures, presets and clears.");
    quad_nor_nors();
    triple_gates_decide(PART_74HC10, 0x6);
    triple_gates_decide(PART_74HC11, 0x1);
    schmitt_holds_between_thresholds();
    puts("Logic ICs: 74HC02 nors, 74HC10 nands, 74HC11 ands, 74HC14 holds between its thresholds.");
    octal_dff_273_clears();
    octal_latch_373_follows_and_holds();
    octal_dff_374_loads_while_released();
    puts("Logic ICs: 74HC273 loads and clears, 74HC373 follows, holds and releases, 74HC374 loads while released.");
    counter_163_waits_for_the_edge();
    up_down_193_counts_up_loads_and_clears();
    up_down_193_counts_down();
    puts("Logic ICs: 74HC163 clears and loads on the edge, 74HC193 counts both ways, loads and clears at once.");
    decoder_138_selects_one();
    dual_decoder_139_selects_per_half();
    multiplexer_151_routes();
    quad_multiplexer_157_switches();
    bcd_decoder_4511_shows_digits();
    puts("Logic ICs: 74HC138 and 74HC139 decode, 74HC151 and 74HC157 route, 74HC4511 shows, holds, blanks and "
         "tests digits.");
    printf("Logic ICs: 74HC283 adds, 74HC245 passes and releases, AT28C16 reads, 74HC173 loads and releases, "
           "74HC161 counts and carries.\n");
    return 0;
}
