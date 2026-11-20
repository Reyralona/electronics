#include "netlist_logic.h"
#include "netlist.h"
#include "rom_image.h"
#include "mcu_board.h"
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
    NetlistProtection *protection;
    int part;
    double output_limit_amps; /* 0: the datasheet gives no output rating, no current term */
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

/* Input load of 1e8 ohm and the pin voltage rescaled from the chip supply to the 5 V bridge domain: vin<pin>. */
static void input_scaled(LogicWriter *w, int pin)
{
    int node = w->nodes[pin];
    const char *p = w->prefix;
    if (node != w->gnd)
        emit(w, "R%sr%d %d %d %.17g\n", p, pin, node, w->gnd, NETLIST_GATE_INPUT_OHMS);
    emit(w, "B%svin%d %svin%d 0 V=V(%d,%d)*%g/max(V(%ssup),%g)\n", p, pin, p, pin, node, w->gnd, LOGIC_BRIDGE_VOLTS,
         p, LOGIC_SUPPLY_FLOOR);
}

/* Logic input: digital node d<pin>. */
static void input(LogicWriter *w, int pin)
{
    input_scaled(w, pin);
    emit(w, "A%si%d [%svin%d] [%sd%d] lic_adc\n", w->prefix, pin, w->prefix, pin, w->prefix, pin);
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
    if (!w->output_limit_amps)
        return;
    char high[PROTECTION_NAME_CAPACITY], low[PROTECTION_NAME_CAPACITY];
    snprintf(high, sizeof(high), "%shi%d", p, pin);
    snprintf(low, sizeof(low), "%slo%d", p, pin);
    netlist_protection_output(w->protection, w->part, pin, w->vcc, w->gnd, node, high, low, "0",
                              w->output_limit_amps);
}

static void supply(LogicWriter *w)
{
    const char *p = w->prefix;
    emit(w, "R%ssupply %d %d %.17g\n", p, w->vcc, w->gnd, NETLIST_LOGIC_SUPPLY_OHMS);
    emit(w, "B%ssup %ssup 0 V=V(%d,%d)\n", p, p, w->vcc, w->gnd);
    /* V<prefix>alive drops to 0 when the chip burns: every output opens. */
    emit(w, "V%salive %salive 0 1\n", p, p);
    emit(w, "B%sok %sok 0 V=V(%salive)*min(max((V(%ssup)-%g)/%g,0),1)*min(max((%g-V(%ssup))/%g,0),1)\n", p, p, p, p,
         LOGIC_SUPPLY_MIN - QUAD_EDGE_VOLTS, QUAD_EDGE_VOLTS, LOGIC_SUPPLY_MAX + QUAD_EDGE_VOLTS, p, QUAD_EDGE_VOLTS);
}

/* SN74HC161 and SN74HC163 share everything but the clear: asynchronous on the 161, on the clock edge on the 163
 * (where it beats LOAD). */
static void binary_counter(LogicWriter *w, int clear_on_edge)
{
    static const int pins[] = {0, 1, 2, 3, 4, 5, 6, 8, 9};
    static const int data_pins[] = {2, 3, 4, 5}, q_pins[] = {13, 12, 11, 10};
    inputs(w, pins, (int)(sizeof(pins) / sizeof(pins[0])));
    if (!clear_on_edge)
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
        primitive(w, "or", "[ld%d kp%d] %s%d", bit, bit, clear_on_edge ? "nx" : "n", bit);
        if (clear_on_edge)
            primitive(w, "and", "[d0 nx%d] n%d", bit, bit);
        primitive(w, "dff", "n%d d1 null %s q%d null", bit, clear_on_edge ? "null" : "clr", bit);
        char q[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(q, sizeof(q), "q%d", bit);
        output(w, q_pins[bit], q, NULL);
    }
    primitive(w, "and", "[d9 q0 q1 q2 q3] rco");
    output(w, 14, "rco", NULL);
}

/* SN74HC161 (TI SCLS297D): synchronous load and count on the rising CLK edge, asynchronous clear. */
static void counter_161(LogicWriter *w)
{
    binary_counter(w, 0);
}

/* SN74HC163 (TI SCLS298D): clear, load and count all on the rising CLK edge. */
static void counter_163(LogicWriter *w)
{
    binary_counter(w, 1);
}

/* SN74HC193 (TI SCLS122D): counts up on the rising edge of UP with DOWN high, down on that of DOWN with UP
 * high. CLR (high) and ~LOAD (low) act at once through the flip-flop set and reset; CLR wins. A flip-flop
 * set by UP low and reset by DOWN low remembers which clock went low last, so the edge that brings both
 * high knows its direction. */
static void up_down_counter_193(LogicWriter *w)
{
    static const int pins[] = {0, 3, 4, 8, 9, 10, 13, 14};
    static const int data_pins[] = {14, 0, 9, 8}, q_pins[] = {2, 1, 5, 6};
    inputs(w, pins, (int)(sizeof(pins) / sizeof(pins[0])));
    primitive(w, "one", "t0");
    primitive(w, "and", "[d4 d3] ck");
    primitive(w, "dff", "t0 t0 ~d4 ~d3 up dn");
    primitive(w, "and", "[~d10 ~d13] ld");
    for (int bit = 0; bit < 4; ++bit)
    {
        /* Up: a bit toggles when every lower bit is 1. Down: when every lower bit is 0. */
        if (bit)
            primitive(w, "xor", "[q%d dn] x%d", bit - 1, bit - 1);
        if (bit)
            primitive(w, "and", "[t%d x%d] t%d", bit - 1, bit - 1, bit);
        primitive(w, "xor", "[q%d t%d] n%d", bit, bit, bit);
        primitive(w, "and", "[ld ~d%d] lr%d", data_pins[bit], bit);
        primitive(w, "or", "[d13 lr%d] r%d", bit, bit);
        /* Set waits for reset to drop: both active and released together would leave the flip-flop unknown. */
        primitive(w, "and", "[ld d%d ~r%d] s%d", data_pins[bit], bit, bit);
        primitive(w, "dff", "n%d ck s%d r%d q%d null", bit, bit, bit, bit);
        char q[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(q, sizeof(q), "q%d", bit);
        output(w, q_pins[bit], q, NULL);
    }
    primitive(w, "nand", "[~d4 q0 q1 q2 q3] co");
    primitive(w, "nand", "[~d3 ~q0 ~q1 ~q2 ~q3] bo");
    output(w, 11, "co", NULL);
    output(w, 12, "bo", NULL);
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

/* SN74HC04: six inverters, Y = NOT A. */
static void hex_inverter_04(LogicWriter *w)
{
    static const int a_pins[] = {0, 2, 4, 8, 10, 12}, y_pins[] = {1, 3, 5, 7, 9, 11};
    inputs(w, a_pins, 6);
    for (int gate = 0; gate < 6; ++gate)
    {
        char y[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(y, sizeof(y), "y%d", gate);
        primitive(w, "inv", "d%d y%d", a_pins[gate], gate);
        output(w, y_pins[gate], y, NULL);
    }
}

/* SN74HC74: rising-edge D flip-flops; ~PRE/~CLR asynchronous and active low. Both low drive Q and ~Q
 * high (the datasheet's unstable state); releasing them returns to the stored bit. */
static void dual_dff_74(LogicWriter *w)
{
    static const int pins[] = {0, 1, 2, 3, 9, 10, 11, 12};
    static const int clr[] = {0, 12}, d[] = {1, 11}, clk[] = {2, 10}, pre[] = {3, 9}, q[] = {4, 8}, nq[] = {5, 7};
    inputs(w, pins, 8);
    for (int ff = 0; ff < 2; ++ff)
    {
        primitive(w, "and", "[~d%d d%d] set%d", pre[ff], clr[ff], ff);
        primitive(w, "and", "[d%d ~d%d] rst%d", pre[ff], clr[ff], ff);
        primitive(w, "nor", "[d%d d%d] both%d", pre[ff], clr[ff], ff);
        primitive(w, "dff", "d%d d%d set%d rst%d s%d ns%d", d[ff], clk[ff], ff, ff, ff, ff);
        primitive(w, "or", "[s%d both%d] q%d", ff, ff, ff);
        primitive(w, "or", "[ns%d both%d] nq%d", ff, ff, ff);
        char out[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(out, sizeof(out), "q%d", ff);
        output(w, q[ff], out, NULL);
        snprintf(out, sizeof(out), "nq%d", ff);
        output(w, nq[ff], out, NULL);
    }
}

/* `count` gates of `model` with `width` inputs each: gate g reads in_pins[g * width ...] and drives y_pins[g]. */
static void gate_array(LogicWriter *w, const char *model, const int *in_pins, int width, const int *y_pins, int count)
{
    inputs(w, in_pins, width * count);
    for (int gate = 0; gate < count; ++gate)
    {
        char list[LOGIC_IC_PORTS_CAPACITY] = "", y[LOGIC_IC_TOKEN_CAPACITY];
        for (int i = 0; i < width; ++i)
        {
            size_t used = strlen(list);
            snprintf(list + used, sizeof(list) - used, " d%d", in_pins[gate * width + i]);
        }
        snprintf(y, sizeof(y), "y%d", gate);
        primitive(w, model, "[%s] %s", list + 1, y);
        output(w, y_pins[gate], y, NULL);
    }
}

/* SN74HC02: four 2-input NOR gates; outputs 1Y and 2Y come before their inputs. */
static void quad_nor_02(LogicWriter *w)
{
    static const int in_pins[] = {1, 2, 4, 5, 7, 8, 10, 11}, y_pins[] = {0, 3, 9, 12};
    gate_array(w, "nor", in_pins, 2, y_pins, 4);
}

/* SN74HC10 and SN74HC11 share a pinout: gate 1 takes pins 1, 2 and 13 and drives pin 12. */
static const int triple_in_pins[] = {0, 1, 12, 2, 3, 4, 8, 9, 10}, triple_y_pins[] = {11, 5, 7};

static void triple_nand_10(LogicWriter *w)
{
    gate_array(w, "nand", triple_in_pins, 3, triple_y_pins, 3);
}

static void triple_and_11(LogicWriter *w)
{
    gate_array(w, "and", triple_in_pins, 3, triple_y_pins, 3);
}

/* SN74HC14: Y = NOT A with hysteresis. Two bridges at VT+ and VT- (lic_vtp, lic_vtn) set and reset a
 * latch; between them the latch holds, so Y keeps its level. */
static void hex_schmitt_14(LogicWriter *w)
{
    static const int a_pins[] = {0, 2, 4, 8, 10, 12}, y_pins[] = {1, 3, 5, 7, 9, 11};
    const char *p = w->prefix;
    primitive(w, "one", "one");
    for (int gate = 0; gate < 6; ++gate)
    {
        int pin = a_pins[gate];
        input_scaled(w, pin);
        emit(w, "A%sup%d [%svin%d] [%sup%d] lic_vtp\n", p, pin, p, pin, p, pin);
        emit(w, "A%sdn%d [%svin%d] [%sdn%d] lic_vtn\n", p, pin, p, pin, p, pin);
        primitive(w, "srlatch", "up%d ~dn%d one null null s%d y%d", pin, pin, gate, gate);
        char y[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(y, sizeof(y), "y%d", gate);
        output(w, y_pins[gate], y, NULL);
    }
}

/* 74HC273/373/374 pinout: pin 1 (~CLR or ~OE), pin 11 (CLK or LE), nD to nQ. */
static const int octal_d_pins[] = {2, 3, 6, 7, 12, 13, 16, 17}, octal_q_pins[] = {1, 4, 5, 8, 11, 14, 15, 18};

/* Eight cells of `model` (dff or dlatch) taking nD under pin 11; `reset` and `enable` may be NULL. */
static void octal_cells(LogicWriter *w, const char *model, const char *reset, const char *enable)
{
    static const int controls[] = {0, 10};
    inputs(w, controls, 2);
    inputs(w, octal_d_pins, 8);
    for (int bit = 0; bit < 8; ++bit)
    {
        char q[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(q, sizeof(q), "q%d", bit);
        primitive(w, model, "d%d d10 null %s %s null", octal_d_pins[bit], reset ? reset : "null", q);
        output(w, octal_q_pins[bit], q, enable);
    }
}

/* SN74HC273: rising-edge D flip-flops with an asynchronous, active-low clear. */
static void octal_dff_273(LogicWriter *w)
{
    primitive(w, "inv", "d0 clr");
    octal_cells(w, "dff", "clr", NULL);
}

/* SN74HC373: LE high makes Q follow D, LE low holds; ~OE high releases the outputs and keeps the latches. */
static void octal_latch_373(LogicWriter *w)
{
    primitive(w, "inv", "d0 en");
    octal_cells(w, "dlatch", NULL, "en");
}

/* SN74HC374: rising-edge D flip-flops; ~OE high releases the outputs and keeps the stored bits. */
static void octal_dff_374(LogicWriter *w)
{
    primitive(w, "inv", "d0 en");
    octal_cells(w, "dff", NULL, "en");
}

/* EEPROM pins cross the same analog bridges as other CIs; its state runs in d_cosim. */
static void eeprom_28c16(LogicWriter *w)
{
    static const int address_pins[EEPROM_ADDRESS_BITS] = {7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18};
    static const int data_pins[EEPROM_DATA_BITS] = {8, 9, 10, 12, 13, 14, 15, 16};
    static const int controls[] = {17, 19, 20};
    inputs(w, address_pins, EEPROM_ADDRESS_BITS);
    inputs(w, data_pins, EEPROM_DATA_BITS);
    inputs(w, controls, 3);
    emit(w, "B%spower %spower 0 V=(V(%ssup)>=4.5 && V(%ssup)<=5.5)?5:0\n",
         w->prefix, w->prefix, w->prefix, w->prefix);
    emit(w, "A%spower [%spower] [%spowered] lic_adc\n", w->prefix, w->prefix, w->prefix);
    for (int bit = 0; bit < EEPROM_DATA_BITS; ++bit)
    {
        char value[LOGIC_IC_TOKEN_CAPACITY];
        snprintf(value, sizeof(value), "v%d", bit);
        output(w, data_pins[bit], value, "en");
    }
}

int netlist_eeprom_core(FILE *out, const Workbench *bench)
{
    int count = 0;
    for (int i = 0; i < bench->part_count; ++i)
        count += bench->parts[i].type == PART_AT28C16;
    if (!count)
        return 1;
    static const int pins[] = {7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18, 8, 9, 10, 12, 13, 14, 15, 16, 17, 19, 20};
    fprintf(out, "Aee_core [");
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == PART_AT28C16)
        {
            for (size_t p = 0; p < sizeof(pins) / sizeof(pins[0]); ++p)
                fprintf(out, " l%d_d%d", i, pins[p]);
            fprintf(out, " l%d_powered", i);
        }
    fprintf(out, "] [");
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == PART_AT28C16)
        {
            for (int bit = 0; bit < EEPROM_DATA_BITS; ++bit)
                fprintf(out, " l%d_v%d", i, bit);
            fprintf(out, " l%d_en", i);
        }
    fprintf(out, "] null ee_core\n.model ee_core d_cosim(simulation=\"bancada-eeprom.dll\" delay=1n lib_args=[");
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == PART_AT28C16)
        {
            RomImage image;
            if (!rom_image_parse(bench->parts[i].code, &image, NULL))
                return 0;
            fprintf(out, " \"%d:", i);
            for (int address = 0; address < ROM_IMAGE_BYTES; ++address)
                fprintf(out, "%02X", image.data[address]);
            fprintf(out, "\"");
        }
    fprintf(out, "])\n");
    return !ferror(out);
}

typedef struct
{
    PartType type;
    int vcc, gnd;
    double supply_absolute_max, supply_recommended_max, output_limit_amps;
    void (*write)(LogicWriter *w);
} LogicChip;

/* AT28C16 (Microchip, consulted 20/11/2026): pins -0.6..+6.25 V, VCC 5 V +-10%; no output current rating. */
#define AT28C16_ABSOLUTE_MAX 6.25
#define AT28C16_RECOMMENDED_MAX 5.5
#define HC_RATINGS HC_SUPPLY_ABSOLUTE_MAX, HC_SUPPLY_RECOMMENDED_MAX, HC_OUTPUT_ABSOLUTE_MAX_AMPS
#define HC_RATINGS_35 HC_SUPPLY_ABSOLUTE_MAX, HC_SUPPLY_RECOMMENDED_MAX, HC35_OUTPUT_ABSOLUTE_MAX_AMPS
static const LogicChip chips[] = {
    {PART_74HC161, 15, 7, HC_RATINGS, counter_161},
    {PART_74HC173, 15, 7, HC_RATINGS, register_173},
    {PART_74HC283, 15, 7, HC_RATINGS, adder_283},
    {PART_74HC245, 19, 9, HC_RATINGS, transceiver_245},
    {PART_AT28C16, 23, 11, AT28C16_ABSOLUTE_MAX, AT28C16_RECOMMENDED_MAX, 0, eeprom_28c16},
    {PART_74HC04, 13, 6, HC_RATINGS, hex_inverter_04},
    {PART_74HC74, 13, 6, HC_RATINGS, dual_dff_74},
    {PART_74HC02, 13, 6, HC_RATINGS, quad_nor_02},
    {PART_74HC10, 13, 6, HC_RATINGS, triple_nand_10},
    {PART_74HC11, 13, 6, HC_RATINGS, triple_and_11},
    {PART_74HC14, 13, 6, HC_RATINGS, hex_schmitt_14},
    {PART_74HC273, 19, 9, HC_RATINGS, octal_dff_273},
    {PART_74HC373, 19, 9, HC_RATINGS_35, octal_latch_373},
    {PART_74HC374, 19, 9, HC_RATINGS_35, octal_dff_374},
    {PART_74HC163, 15, 7, HC_RATINGS, counter_163},
    {PART_74HC193, 15, 7, HC_RATINGS, up_down_counter_193},
};

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

int netlist_logic_write(FILE *out, const Part *part, int index, const int *nodes, NetlistProtection *protection)
{
    const LogicChip *chip = part ? logic_chip(part->type) : NULL;
    if (!out || !chip || !nodes)
        return 0;
    LogicWriter w = {out, nodes, nodes[chip->vcc], nodes[chip->gnd], 1, {0}, protection, index,
                     chip->output_limit_amps};
    snprintf(w.prefix, sizeof(w.prefix), "l%d_", index);
    char supply_node[PROTECTION_NAME_CAPACITY];
    snprintf(supply_node, sizeof(supply_node), "%ssup", w.prefix);
    netlist_protection_supply(protection, index, supply_node, chip->supply_absolute_max,
                              chip->supply_recommended_max);
    RomImage image;
    if (part->type == PART_AT28C16 && !rom_image_parse(part->code, &image, NULL))
        return 0;
    supply(&w);
    chip->write(&w);
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
                   ".model lic_one d_pullup\n"
                   ".model lic_nand d_nand(rise_delay=1n fall_delay=1n)\n"
                   ".model lic_dlatch d_dlatch(ic=0 data_delay=1n enable_delay=1n set_delay=1n reset_delay=1n "
                   "rise_delay=1n fall_delay=1n)\n"
                   ".model lic_srlatch d_srlatch(ic=0 sr_delay=1n enable_delay=1n set_delay=1n reset_delay=1n "
                   "rise_delay=1n fall_delay=1n)\n"
                   /* 74HC14 thresholds VT+ = .55 VCC and VT- = .35 VCC (TI SCLS085L typical, 4.5 and 6 V). */
                   ".model lic_vtp adc_bridge(in_low=2.75 in_high=2.75)\n"
                   ".model lic_vtn adc_bridge(in_low=1.75 in_high=1.75)\n") >= 0;
}

void netlist_logic_ratings(PartType type, double *supply_absolute_max, double *supply_recommended_max,
                           double *output_limit_amps)
{
    const McuRatings *board = mcu_board_ratings(type);
    if (board)
    {
        *supply_absolute_max = board->supply_absolute_max;
        *supply_recommended_max = board->supply_recommended_max;
        *output_limit_amps = board->pin_amps;
        return;
    }
    const LogicChip *chip = logic_chip(type);
    *supply_absolute_max = chip ? chip->supply_absolute_max : HC_SUPPLY_ABSOLUTE_MAX;
    *supply_recommended_max = chip ? chip->supply_recommended_max : HC_SUPPLY_RECOMMENDED_MAX;
    *output_limit_amps = chip ? chip->output_limit_amps : HC_OUTPUT_ABSOLUTE_MAX_AMPS;
}
