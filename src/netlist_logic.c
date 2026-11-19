#include "netlist_logic.h"
#include "netlist.h"
#include "rom_image.h"
#include <stdarg.h>
#include <string.h>

/* Output stage: one switch to VCC and one to GND per pin, ~50 ohm when on, like the 74HC00 model.
 * Pin numbers below are 0-based (datasheet pin - 1); see docs/COMPARACAO_COMPUTADOR_8_BITS.md. */
#define LOGIC_IC_PREFIX_CAPACITY 16
#define LOGIC_IC_TOKEN_CAPACITY 24
#define LOGIC_IC_PORTS_CAPACITY 160
#define EEPROM_ADDRESS_BITS 11
#define EEPROM_DATA_BITS 8

typedef struct
{
    FILE *out;
    const int *nodes;
    int vcc, gnd; /* SPICE nodes of the chip's supply pins */
    int ok;
    char prefix[LOGIC_IC_PREFIX_CAPACITY]; /* unique per chip: "l<index>_" */
} LogicWriter;

static void emit(LogicWriter *w, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (w->ok && vfprintf(w->out, format, args) < 0)
        w->ok = 0;
    va_end(args);
}

/* Writes a port list, prefixing every digital node with the chip's prefix. "~node" stays inverted,
 * "null" stays unconnected and brackets pass through. */
static void ports(LogicWriter *w, const char *list)
{
    const char *c = list;
    while (*c)
    {
        if (*c == ' ' || *c == '[' || *c == ']')
        {
            emit(w, "%c", *c++);
            continue;
        }
        size_t length = strcspn(c, " []");
        if (*c == '~')
        {
            emit(w, "~");
            ++c;
            --length;
        }
        if (length == 4 && !strncmp(c, "null", 4))
            emit(w, "null");
        else
            emit(w, "%s%.*s", w->prefix, (int)length, c);
        c += length;
    }
}

/* One XSPICE digital instance, named after its output: the last connected port of the list. */
static void primitive(LogicWriter *w, const char *model, const char *format, ...)
{
    char list[LOGIC_IC_PORTS_CAPACITY];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(list, sizeof(list), format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof(list))
    {
        w->ok = 0;
        return;
    }
    const char *name = list + length;
    while (name > list && name[-1] != ' ')
        --name;
    if (!strcmp(name, "null"))
        while (--name > list && name[-1] != ' ')
            ;
    emit(w, "A%s%.*s ", w->prefix, (int)strcspn(name, " "), name);
    ports(w, list);
    emit(w, " lic_%s\n", model);
}

/* Logic input: 1e8 ohm load, rescaled from the chip supply to the 5 V bridge domain, digital node d<pin>. */
static void input(LogicWriter *w, int pin)
{
    int node = w->nodes[pin];
    const char *p = w->prefix;
    if (node != w->gnd)
        emit(w, "R%sr%d %d %d %.17g\n", p, pin, node, w->gnd, NETLIST_GATE_INPUT_OHMS);
    emit(w, "B%svin%d %svin%d 0 V=V(%d,%d)*%g/max(V(%ssup),%g)\n", p, pin, p, pin, node, w->gnd, LOGIC_BRIDGE_VOLTS,
         p, LOGIC_SUPPLY_FLOOR);
    emit(w, "A%si%d [%svin%d] [%sd%d] lic_adc\n", p, pin, p, pin, p, pin);
}

static void inputs(LogicWriter *w, const int *pins, int count)
{
    for (int i = 0; i < count; ++i)
        input(w, pins[i]);
}

/* Push-pull output driven by digital node `value`; with `enable` (NULL = always on) it is 3-state. */
static void output(LogicWriter *w, int pin, const char *value, const char *enable)
{
    const char *p = w->prefix;
    int node = w->nodes[pin];
    emit(w, "A%so%d [%s%s", p, pin, p, value);
    if (enable)
        emit(w, " %s%s", p, enable);
    emit(w, "] [%sa%d", p, pin);
    if (enable)
        emit(w, " %se%d", p, pin);
    emit(w, "] lic_dac\n");
    char gate[LOGIC_IC_TOKEN_CAPACITY] = "1";
    if (enable)
        snprintf(gate, sizeof(gate), "V(%se%d)", p, pin);
    emit(w, "B%shi%d %shi%d 0 V=V(%sok)*%s*V(%sa%d)\n", p, pin, p, pin, p, gate, p, pin);
    emit(w, "B%slo%d %slo%d 0 V=V(%sok)*%s*(1-V(%sa%d))\n", p, pin, p, pin, p, gate, p, pin);
    emit(w, "S%ssh%d %d %d %shi%d 0 lic_switch\n", p, pin, w->vcc, node, p, pin);
    emit(w, "S%ssl%d %d %d %slo%d 0 lic_switch\n", p, pin, node, w->gnd, p, pin);
}

static void supply(LogicWriter *w)
{
    const char *p = w->prefix;
    emit(w, "B%ssup %ssup 0 V=V(%d,%d)\n", p, p, w->vcc, w->gnd);
    emit(w, "B%sok %sok 0 V=min(max((V(%ssup)-%g)/%g,0),1)*min(max((%g-V(%ssup))/%g,0),1)\n", p, p, p,
         LOGIC_SUPPLY_MIN - QUAD_EDGE_VOLTS, QUAD_EDGE_VOLTS, LOGIC_SUPPLY_MAX + QUAD_EDGE_VOLTS, p, QUAD_EDGE_VOLTS);
}

/* SN74HC161 (TI SCLS297D): synchronous load and count on the rising CLK edge, asynchronous clear. */
static void counter_161(LogicWriter *w)
{
    static const int pins[] = {0, 1, 2, 3, 4, 5, 6, 8, 9};
    static const int data_pins[] = {2, 3, 4, 5}, q_pins[] = {13, 12, 11, 10};
    inputs(w, pins, (int)(sizeof(pins) / sizeof(pins[0])));
    primitive(w, "inv", "d0 clr");
    primitive(w, "and", "[d6 d9] t0");
    for (int bit = 0; bit < 4; ++bit)
    {
        if (bit)
            primitive(w, "and", "[t%d q%d] t%d", bit - 1, bit - 1, bit);
        /* Counting toggles a bit when every lower bit is 1; LOAD (pin 9, active low) takes A-D instead. */
        primitive(w, "xor", "[q%d t%d] inc%d", bit, bit, bit);
        primitive(w, "and", "[~d8 d%d] ld%d", data_pins[bit], bit);
        primitive(w, "and", "[d8 inc%d] kp%d", bit, bit);
        primitive(w, "or", "[ld%d kp%d] n%d", bit, bit, bit);
        primitive(w, "dff", "n%d d1 null clr q%d null", bit, bit);
        char q[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(q, sizeof(q), "q%d", bit);
        output(w, q_pins[bit], q, NULL);
    }
    primitive(w, "and", "[d9 q0 q1 q2 q3] rco");
    output(w, 14, "rco", NULL);
}

/* SN74HC173: loads D on the rising CLK edge when G1 and G2 are low, CLR high clears, outputs enabled
 * when M and N are low. */
static void register_173(LogicWriter *w)
{
    static const int pins[] = {0, 1, 6, 8, 9, 10, 11, 12, 13, 14};
    static const int data_pins[] = {13, 12, 11, 10}, q_pins[] = {2, 3, 4, 5};
    inputs(w, pins, (int)(sizeof(pins) / sizeof(pins[0])));
    primitive(w, "and", "[~d8 ~d9] load");
    primitive(w, "inv", "load keep");
    primitive(w, "nor", "[d0 d1] en");
    for (int bit = 0; bit < 4; ++bit)
    {
        char q[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(q, sizeof(q), "q%d", bit);
        primitive(w, "and", "[load d%d] ld%d", data_pins[bit], bit);
        primitive(w, "and", "[keep q%d] kp%d", bit, bit);
        primitive(w, "or", "[ld%d kp%d] n%d", bit, bit, bit);
        primitive(w, "dff", "n%d d6 null d14 q%d null", bit, bit);
        output(w, q_pins[bit], q, "en");
    }
}

/* SN74HC283: 4-bit binary full adder with carry in C0 and carry out C4. */
static void adder_283(LogicWriter *w)
{
    static const int pins[] = {1, 2, 4, 5, 6, 10, 11, 13, 14};
    static const int a_pins[] = {4, 2, 13, 11}, b_pins[] = {5, 1, 14, 10}, sum_pins[] = {3, 0, 12, 9};
    inputs(w, pins, (int)(sizeof(pins) / sizeof(pins[0])));
    for (int bit = 0; bit < 4; ++bit)
    {
        char sum[LOGIC_IC_TOKEN_CAPACITY], carry[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(sum, sizeof(sum), "s%d", bit);
        snprintf(carry, sizeof(carry), bit ? "c%d" : "d6", bit); /* C0 is pin 7 */
        primitive(w, "xor", "[d%d d%d] p%d", a_pins[bit], b_pins[bit], bit);
        primitive(w, "xor", "[p%d %s] s%d", bit, carry, bit);
        primitive(w, "and", "[d%d d%d] g%d", a_pins[bit], b_pins[bit], bit);
        primitive(w, "and", "[p%d %s] h%d", bit, carry, bit);
        primitive(w, "or", "[g%d h%d] c%d", bit, bit, bit + 1);
        output(w, sum_pins[bit], sum, NULL);
    }
    output(w, 8, "c4", NULL);
}

/* SN74HC245: OE low enables; DIR high sends A to B, DIR low sends B to A. */
static void transceiver_245(LogicWriter *w)
{
    static const int pins[] = {0, 18};
    inputs(w, pins, 2);
    primitive(w, "and", "[~d18 d0] eab");
    primitive(w, "and", "[~d18 ~d0] eba");
    for (int bit = 0; bit < 8; ++bit)
    {
        int a = 1 + bit, b = 17 - bit;
        char from_a[LOGIC_IC_TOKEN_CAPACITY], from_b[LOGIC_IC_TOKEN_CAPACITY];
        input(w, a);
        input(w, b);
        snprintf(from_a, sizeof(from_a), "d%d", a);
        snprintf(from_b, sizeof(from_b), "d%d", b);
        output(w, b, from_a, "eab");
        output(w, a, from_b, "eba");
    }
}

/* AT28C16 read mode (CE and OE low, WE high). The stored bytes become an address decoder: each address
 * whose byte is not FF gets an AND of its address bits, and each data bit is the NOR of the addresses
 * where that bit is 0. Writing through WE is not modeled. */
static void eeprom_28c16(LogicWriter *w, const RomImage *image)
{
    static const int address_pins[EEPROM_ADDRESS_BITS] = {7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18};
    static const int data_pins[EEPROM_DATA_BITS] = {8, 9, 10, 12, 13, 14, 15, 16};
    static const int controls[] = {17, 19, 20};
    const char *p = w->prefix;
    inputs(w, address_pins, EEPROM_ADDRESS_BITS);
    inputs(w, controls, 3);
    primitive(w, "and", "[~d17 ~d19 d20] en");
    for (int address = 0; address < ROM_IMAGE_BYTES; ++address)
    {
        if (image->data[address] == ROM_IMAGE_ERASED)
            continue;
        emit(w, "A%sm%d [", p, address);
        for (int bit = 0; bit < EEPROM_ADDRESS_BITS; ++bit)
            emit(w, "%s%s%sd%d", bit ? " " : "", address >> bit & 1 ? "" : "~", p, address_pins[bit]);
        emit(w, "] %sm%d lic_and\n", p, address);
    }
    for (int bit = 0; bit < EEPROM_DATA_BITS; ++bit)
    {
        int zeros = 0;
        for (int address = 0; address < ROM_IMAGE_BYTES; ++address)
            zeros += !(image->data[address] >> bit & 1);
        if (!zeros)
            emit(w, "A%sv%d %sv%d lic_one\n", p, bit, p, bit);
        else
        {
            /* XSPICE gates need two inputs: a single address is listed twice. */
            emit(w, "A%sv%d [", p, bit);
            for (int address = 0, written = 0; address < ROM_IMAGE_BYTES; ++address)
                if (!(image->data[address] >> bit & 1))
                    emit(w, "%s%sm%d", written++ ? " " : "", p, address);
            if (zeros == 1)
                for (int address = 0; address < ROM_IMAGE_BYTES; ++address)
                    if (!(image->data[address] >> bit & 1))
                        emit(w, " %sm%d", p, address);
            emit(w, "] %sv%d lic_nor\n", p, bit);
        }
        char value[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(value, sizeof(value), "v%d", bit);
        output(w, data_pins[bit], value, "en");
    }
}

typedef struct
{
    PartType type;
    int vcc, gnd;
} LogicChip;

static const LogicChip chips[] = {
    {PART_74HC161, 15, 7}, {PART_74HC173, 15, 7}, {PART_74HC283, 15, 7}, {PART_74HC245, 19, 9}, {PART_AT28C16, 23, 11}};

static const LogicChip *logic_chip(PartType type)
{
    for (size_t i = 0; i < sizeof(chips) / sizeof(chips[0]); ++i)
        if (chips[i].type == type)
            return &chips[i];
    return NULL;
}

int netlist_logic_supported(PartType type)
{
    return logic_chip(type) != NULL;
}

int netlist_logic_valid(const Part *part)
{
    return part && (part->type != PART_AT28C16 || rom_image_parse(part->code, NULL, NULL));
}

int netlist_logic_write(FILE *out, const Part *part, int index, const int *nodes)
{
    const LogicChip *chip = part ? logic_chip(part->type) : NULL;
    if (!out || !chip || !nodes)
        return 0;
    LogicWriter w = {out, nodes, nodes[chip->vcc], nodes[chip->gnd], 1, {0}};
    snprintf(w.prefix, sizeof(w.prefix), "l%d_", index);
    supply(&w);
    if (part->type == PART_74HC161)
        counter_161(&w);
    else if (part->type == PART_74HC173)
        register_173(&w);
    else if (part->type == PART_74HC283)
        adder_283(&w);
    else if (part->type == PART_74HC245)
        transceiver_245(&w);
    else
    {
        RomImage image;
        if (!rom_image_parse(part->code, &image, NULL))
            return 0;
        eeprom_28c16(&w, &image);
    }
    return w.ok;
}

int netlist_logic_models(FILE *out)
{
    return fprintf(out,
                   ".model lic_adc adc_bridge(in_low=1.5 in_high=3.5)\n"
                   ".model lic_dac dac_bridge(out_low=0 out_high=1 out_undef=.5 t_rise=1n t_fall=1n)\n"
                   ".model lic_switch SW(Ron=50 Roff=1e12 Vt=.5 Vh=.1)\n"
                   ".model lic_and d_and(rise_delay=1n fall_delay=1n)\n"
                   ".model lic_or d_or(rise_delay=1n fall_delay=1n)\n"
                   ".model lic_nor d_nor(rise_delay=1n fall_delay=1n)\n"
                   ".model lic_xor d_xor(rise_delay=1n fall_delay=1n)\n"
                   ".model lic_inv d_inverter(rise_delay=1n fall_delay=1n)\n"
                   ".model lic_dff d_dff(ic=0 clk_delay=1n set_delay=1n reset_delay=1n rise_delay=1n fall_delay=1n)\n"
                   ".model lic_one d_pullup\n") >= 0;
}
