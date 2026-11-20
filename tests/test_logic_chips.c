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
    printf("Logic ICs: 74HC283 adds, 74HC245 passes and releases, AT28C16 reads, 74HC173 loads and releases, "
           "74HC161 counts and carries.\n");
    return 0;
}
