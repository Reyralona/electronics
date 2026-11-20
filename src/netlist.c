#include "netlist.h"
#include "netlist_devices.h"
#include "netlist_logic.h"
#include "mcu_board.h"
#include "mcu_lang.h"
#include "component_devices.h"
#include "component_registry.h"
#include "ui_text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PIN_TOTAL (WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS)
static int find_root(int *parents, int x)
{
    while (parents[x] != x)
        x = parents[x];
    return x;
}
static void unite(int *parents, int a, int b)
{
    parents[find_root(parents, a)] = find_root(parents, b);
}
static int pin_index(Pin p)
{
    return p.component * WORKBENCH_MAX_PINS + p.pin;
}
static int valid_pin(const Workbench *b, Pin p)
{
    return p.component >= 0 && p.component < b->part_count && p.pin >= 0 &&
           p.pin < bench_pin_count(b->parts[p.component].type);
}

/* Joins pins that are electrically the same node. Union order is kept stable so node numbers do not change. */
static void build_nets(const Workbench *b, int *parents, int unite_wires, int short_inductors, int switches)
{
    int i, j;
    for (i = 0; i < PIN_TOTAL; ++i)
        parents[i] = i;
    if (unite_wires)
    {
        for (i = 0; i < b->wire_count; ++i)
        {
            Wire w = b->wires[i];
            if (valid_pin(b, w.a) && valid_pin(b, w.b))
                unite(parents, pin_index(w.a), pin_index(w.b));
        }
    }
    for (i = 0; i < b->part_count; ++i)
    {
        int base = i * WORKBENCH_MAX_PINS;
        PartType type = b->parts[i].type;
        if (switches && (component_electrical_type(type) == PART_SWITCH || type == PART_BUTTON) && component_effective_on(&b->parts[i]))
            unite(parents, base, base + 1);
        if (type == PART_INDUCTOR && short_inductors)
            unite(parents, base, base + 1);
        if (bench_is_board(type))
        {
            for (j = 0; j < bench_pin_count(type); ++j)
                unite(parents, base + bench_board_group_first(j), base + j);
        }
    }
    for (i = 0; i < b->part_count; ++i)
    {
        int pin;
        for (pin = 0; pin < bench_pin_count(b->parts[i].type); ++pin)
        {
            Pin contact;
            int after = -1;
            while (bench_find_contact_after(b, i, pin, after, &contact))
            {
                after = pin_index(contact);
                unite(parents, i * WORKBENCH_MAX_PINS + pin, after);
            }
        }
    }
}

int netlist_shorted_sources(const Workbench *b, unsigned char shorted[WORKBENCH_MAX_COMPONENTS])
{
    int parents[PIN_TOTAL], i, count = 0;
    if (!b || !shorted || b->part_count < 0 || b->part_count > WORKBENCH_MAX_COMPONENTS)
        return 0;
    memset(shorted, 0, WORKBENCH_MAX_COMPONENTS);
    build_nets(b, parents, 1, 1, 1);
    for (i = 0; i < b->part_count; ++i)
    {
        PartType type = b->parts[i].type;
        int base = i * WORKBENCH_MAX_PINS;
        if (component_electrical_type(type) != PART_SOURCE && type != PART_CLOCK)
            continue;
        if (find_root(parents, base) != find_root(parents, base + 1))
            continue;
        shorted[i] = 1;
        ++count;
    }
    return count;
}

/* CV/CC: drive<i> follows src<i> until the delivered current passes V(ilim<i>), then drops to hold it.
 * V(ilim<i>) holds the limit so a live `alter` turns the knob without rebuilding the circuit. */
static int write_current_limit(FILE *out, const char *source, int index, double limit)
{
    return fprintf(out,
                   "Vilim%d ilim%d 0 %.17g\n"
                   "Bcc%d drive%d src%d V=-%g*0.5*((-I(%s)-V(ilim%d))+sqrt((-I(%s)-V(ilim%d))^2+%g))\n",
                   index, index, limit, index, index, index, NETLIST_SOURCE_LIMIT_GAIN, source, index, source,
                   index, NETLIST_SOURCE_LIMIT_SMOOTH * NETLIST_SOURCE_LIMIT_SMOOTH) >= 0;
}

static int write_live_contacts(FILE *out, const Workbench *bench, int *parents, int ground)
{
    int primary = -1;
    for (int i = 0; i < bench->part_count; ++i)
        if (component_electrical_type(bench->parts[i].type) == PART_SOURCE)
        {
            primary = i;
            break;
        }
    if (primary < 0)
        for (int i = 0; i < bench->part_count; ++i)
            if (bench->parts[i].type == PART_CLOCK)
            {
                primary = i;
                break;
            }
    for (int i = 0; i < bench->part_count; ++i)
    {
        const Part *part = &bench->parts[i];
        if (component_electrical_type(part->type) != PART_SOURCE && part->type != PART_CLOCK &&
            component_electrical_type(part->type) != PART_SWITCH && part->type != PART_BUTTON)
            continue;
        int a = find_root(parents, i * WORKBENCH_MAX_PINS);
        int b = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
        a = a == ground ? 0 : a + 1;
        b = b == ground ? 0 : b + 1;
        double resistance = component_effective_on(part) ? NETLIST_LIVE_CLOSED_OHMS : NETLIST_LIVE_OPEN_OHMS;
        if (component_electrical_type(part->type) == PART_SWITCH || part->type == PART_BUTTON)
        {
            if (fprintf(out, "Rcontact%d %d %d %.17g\n", i, a, b, resistance) < 0)
                return 0;
            continue;
        }
        char name[32];
        if (i == primary)
            snprintf(name, sizeof(name), "Vsource");
        else
            snprintf(name, sizeof(name), "Vsource%d", i);
        if (part->type == PART_CLOCK)
        {
            double period = 1 / part->value;
            if (fprintf(out, "%s drive%d %d PULSE(0 5 0 %.17g %.17g %.17g %.17g)\n", name, i, b,
                        period * .001, period * .001, period * .499, period) < 0)
                return 0;
        }
        else if (part->type == PART_SOURCE)
        {
            if (fprintf(out, "%s src%d %d %.17g\n", name, i, b, part->value) < 0 ||
                !write_current_limit(out, name, i, component_source_limit(part)))
                return 0;
        }
        else if (fprintf(out, "%s drive%d %d %.17g\n", name, i, b, part->value) < 0)
            return 0;
        resistance = part->on ? component_source_resistance(part->type) : NETLIST_LIVE_OPEN_OHMS;
        if (fprintf(out, "Rpower%d drive%d %d %.17g\n", i, i, a, resistance) < 0)
            return 0;
    }
    return 1;
}

static int write_supply(FILE *out, const Part *part, int index, int primary, int a, int b)
{
    char name[32];
    if (primary)
        snprintf(name, sizeof(name), "Vsource");
    else
        snprintf(name, sizeof(name), "Vsource%d", index);
    if (component_source_resistance(part->type) > NETLIST_LIVE_CLOSED_OHMS)
        return fprintf(out, "%s battery%d %d %.17g\nRpower%d battery%d %d %.17g\n", name,
                       index, b, part->value, index, index, a,
                       part->on ? component_source_resistance(part->type) : NETLIST_LIVE_OPEN_OHMS) >= 0;
    if (part->type == PART_SOURCE)
        return fprintf(out, "%s src%d %d %.17g\nRpower%d drive%d %d %.17g\n", name, index, b,
                       part->on ? part->value : 0.0, index, index, a, NETLIST_LIVE_CLOSED_OHMS) >= 0 &&
               write_current_limit(out, name, index, component_source_limit(part));
    return fprintf(out, "%s %d %d %.17g\n", name, a, b, part->on ? part->value : 0.0) >= 0;
}

static int write_temperature(FILE *out, const Part *part, int index, int supply, int output, int ground)
{
    const ComponentTemperatureModel *model = component_temperature_model();
    char powered[160], sense[48];
    snprintf(powered, sizeof(powered), "v(%d,%d)>=%.17g && v(%d,%d)<=%.17g", supply, ground,
             model->supply_minimum, supply, ground, model->supply_maximum);
    snprintf(sense, sizeof(sense), "Vtmp_sense%d", index);
    if (fprintf(out, "Venv%d env%d 0 %.17g\n", index, index, component_environment_value(part)) < 0)
        return 0;
    /* Voltage feedback keeps an MNA slope at startup and under overload. */
    if (fprintf(out, "Btmp%d tmp_raw%d %d V={%s ? (%.17g+%.17g*v(env%d))-%.17g*"
                    "(max(0,i(%s)-%.17g)+min(0,i(%s)+%.17g)) : -%.17g*i(%s)}\n",
                index, index, ground, powered, model->offset, model->volts_per_degree, index,
                model->limiting_resistance, sense, model->current_limit, sense, model->current_limit,
                model->off_resistance, sense) < 0)
        return 0;
    if (fprintf(out, "Rtmp_out%d tmp_raw%d tmp_sense%d %.17g\n%s tmp_sense%d %d 0\n",
                index, index, index, model->output_resistance, sense, index, output) < 0)
        return 0;
    return fprintf(out, "Btmp_power%d %d %d I={%s ? %.17g+max(0,i(%s)) : 0}\n",
                   index, supply, ground, powered, model->quiescent_current, sense) >= 0;
}

static int write_semiconductor(FILE *out, const Part *part, int index, int a, int b, int c)
{
    if (part->type == PART_SOIL)
    {
        double probe_resistance = component_effective_value(part);
        return fprintf(out, "Rsoil%d %d %d %.17g\nRsoil_bias%d %d %d 10000\n", index, a, b,
                       probe_resistance, index, b, c) >= 0;
    }
    if (part->type == PART_TEMPERATURE)
        return write_temperature(out, part, index, a, b, c);
    if (part->type == PART_DIODE || part->type == PART_PHOTODIODE)
    {
        if (fprintf(out, "D%d %d %d SILICON_GENERIC\n", index, a, b) < 0)
            return 0;
        if (part->type == PART_PHOTODIODE)
            return fprintf(out, "Iphoto%d %d %d %.17g\n", index, b, a, component_photocurrent(part)) >= 0;
    }
    if (part->type == PART_NPN || part->type == PART_PNP)
        return fprintf(out, "Q%d %d %d %d %s\n", index, c, b, a,
                       part->type == PART_NPN ? "NPN_GENERIC" : "PNP_GENERIC") >= 0;
    if (part->type == PART_AMBIENT_LIGHT)
        return fprintf(out, "Q%d %d photo_base%d %d NPN_GENERIC\nIphoto%d %d photo_base%d %.17g\n"
                       "Rphoto%d photo_base%d %d 1e9\n", index, a, index, b, index, b, index,
                       component_photocurrent(part), index, index, b) >= 0;
    return 1;
}

/* Logic input resistance to its reference; see NETLIST_GATE_INPUT_OHMS. Node numbers are SPICE nodes. */
static int write_gate_input(FILE *out, const char *owner, int index, int input, int node, int reference)
{
    if (node == reference)
        return 1;
    return fprintf(out, "R%sin%d_%d %d %d %.17g\n", owner, index, input, node, reference,
                   NETLIST_GATE_INPUT_OHMS) >= 0;
}

#define QUAD_INPUT_TEXT_CAPACITY 96
#define QUAD_DRIVE_TEXT_CAPACITY (4 * QUAD_INPUT_TEXT_CAPACITY + 16)

/* Logic supply of the loose gate symbols ("hidden power pins"): logic_rail is the positive terminal of the
 * reference source; logic_vcc/logic_ok mirror the quad chip supply window. */
static int write_logic_supply(FILE *out, int rail_node, int clock_reference)
{
    if (clock_reference)
    {
        /* A pulse generator alone is a 5 V logic module: it powers the gates with its own 5 V. */
        if (fprintf(out, "Vlogic logic_rail 0 %g\n", LOGIC_BRIDGE_VOLTS) < 0)
            return 0;
    }
    else if (fprintf(out, "Rlogic_rail logic_rail %d %.17g\n", rail_node, NETLIST_LIVE_CLOSED_OHMS) < 0)
        return 0;
    return fprintf(out,
                   "Rlogic_supply logic_rail 0 %.17g\n"
                   "Blogic_vcc logic_vcc 0 V=V(logic_rail)\n"
                   "Blogic_ok logic_ok 0 V=min(max((V(logic_vcc)-%g)/%g,0),1)*min(max((%g-V(logic_vcc))/%g,0),1)\n",
                   NETLIST_LOGIC_SUPPLY_OHMS, LOGIC_SUPPLY_MIN - QUAD_EDGE_VOLTS, QUAD_EDGE_VOLTS, LOGIC_SUPPLY_MAX + QUAD_EDGE_VOLTS,
                   QUAD_EDGE_VOLTS) >= 0;
}

/* Input seen by the digital core: the real input voltage rescaled from VCC to the 5 V bridge domain. */
static int write_gate_scaled_input(FILE *out, int node)
{
    return fprintf(out, "Bgin%d gin%d 0 V=V(%d)*%g/max(V(logic_vcc),%g)\n", node, node, node,
                   LOGIC_BRIDGE_VOLTS, LOGIC_SUPPLY_FLOOR) >= 0;
}

/* CMOS output stage: one switch to the supply rail, one to ground, each ~50 ohm when on (chip_switch).
 * The current really comes from the reference source, and an unpowered gate leaves its output open.
 * Rgout<gate> joins the stage to the pin; a burnt gate opens it. */
static int write_gate_output(FILE *out, int gate, int node, NetlistProtection *protection)
{
    if (protection)
    {
        ProtectionCurrent term = {.part = gate, .pin = -1, .limit_amps = HC_OUTPUT_ABSOLUTE_MAX_AMPS};
        snprintf(term.vcc, sizeof(term.vcc), "logic_rail");
        snprintf(term.gnd, sizeof(term.gnd), "0");
        snprintf(term.node, sizeof(term.node), "gj%d", gate);
        snprintf(term.high, sizeof(term.high), "ghi%d", gate);
        snprintf(term.low_plus, sizeof(term.low_plus), "logic_ok");
        snprintf(term.low_minus, sizeof(term.low_minus), "ghi%d", gate);
        netlist_protection_add_current(protection, term);
    }
    return fprintf(out,
                   "Adac%d [dout%d] [gdo%d] gate_dac\n"
                   "Bghi%d ghi%d 0 V=V(logic_ok)*V(gdo%d)\n"
                   /* low = logic_ok - high; no extra behavioral voltage source is needed. */
                   "Sghi%d logic_rail gj%d ghi%d 0 chip_switch\nSglo%d gj%d 0 logic_ok ghi%d chip_switch\n"
                   "Rgout%d gj%d %d %.17g\n",
                   gate, gate, gate, gate, gate, gate, gate, gate, gate, gate, gate, gate, gate, gate, node,
                   NETLIST_LIVE_CLOSED_OHMS) >= 0;
}

static void quad_gate_drive(PartType type, int chip, int gate, char *high, char *low, size_t capacity)
{
    char a[QUAD_INPUT_TEXT_CAPACITY], b[QUAD_INPUT_TEXT_CAPACITY];
    const double span = LOGIC_INPUT_HIGH_RATIO - LOGIC_INPUT_LOW_RATIO;
    snprintf(a, sizeof(a), "min(max((V(chip%d_a%d)/max(V(chip%d_sup),%g)-%g)/%g,0),1)", chip, gate, chip,
             LOGIC_SUPPLY_FLOOR, LOGIC_INPUT_LOW_RATIO, span);
    snprintf(b, sizeof(b), "min(max((V(chip%d_b%d)/max(V(chip%d_sup),%g)-%g)/%g,0),1)", chip, gate, chip,
             LOGIC_SUPPLY_FLOOR, LOGIC_INPUT_LOW_RATIO, span);
    if (type == PART_74HC86)
    {
        snprintf(high, capacity, "(%s*(1-%s)+(1-%s)*%s)", a, b, a, b);
        snprintf(low, capacity, "(%s*%s+(1-%s)*(1-%s))", a, b, a, b);
    }
    else if (type == PART_74HC32)
    {
        snprintf(low, capacity, "((1-%s)*(1-%s))", a, b);
        snprintf(high, capacity, "(1-(1-%s)*(1-%s))", a, b);
    }
    else
    {
        int inverted = type == PART_74HC00;
        snprintf(inverted ? low : high, capacity, "(%s*%s)", a, b);
        snprintf(inverted ? high : low, capacity, "(1-%s*%s)", a, b);
    }
}

static NetlistResult write_netlist(FILE *out, const Workbench *b, int transient, int measure_wires, Pin probe,
                                   double step, double stop, int session, NetlistProtection *protection)
{
    int parents[PIN_TOTAL], i, source = -1, ground, positive, gate_count = 0, chip_count = 0, logic_count = 0;
    if (!out || !b || b->part_count < 0 || b->part_count > WORKBENCH_MAX_COMPONENTS || b->wire_count < 0 ||
        b->wire_count > WORKBENCH_MAX_WIRES)
        return NETLIST_INVALID;
    if (transient &&
        ((transient == 3 ? (probe.component < 0 || probe.component >= b->wire_count) : !valid_pin(b, probe)) ||
         !isfinite(step) || !isfinite(stop) || step <= 0 || stop <= 0 || step > stop))
        return NETLIST_INVALID;
    for (i = 0; i < b->wire_count; ++i)
    {
        Wire w = b->wires[i];
        if (!valid_pin(b, w.a) || !valid_pin(b, w.b))
            return NETLIST_INVALID;
    }
    for (i = 0; i < b->part_count; ++i)
    {
        PartType type = b->parts[i].type;
        const ComponentInfo *info = component_info(type);
        if (!info || (info->primary && !component_value_valid(type, b->parts[i].value)))
            return NETLIST_INVALID;
        if (component_environment(type) && (!isfinite(b->parts[i].control) ||
            b->parts[i].control < 0 || b->parts[i].control > 1))
            return NETLIST_INVALID;
        switch (component_electrical_type(b->parts[i].type))
        {
        case PART_SOURCE:
            if (source < 0)
                source = i;
            break;
        case PART_NOR:
        case PART_XNOR:
        case PART_AND:
        case PART_NAND:
        case PART_OR:
        case PART_XOR:
        case PART_NOT:
        case PART_DFF:
            ++gate_count;
            break;
        case PART_74HC08:
        case PART_74HC32:
        case PART_74HC86:
        case PART_74HC00:
            ++chip_count;
            break;
        case PART_POTENTIOMETER:
        case PART_BUTTON:
        case PART_SWITCH:
        case PART_BREADBOARD:
        case PART_BREADBOARD_WIDE:
        case PART_RESISTOR:
        case PART_TEMPERATURE:
        case PART_SOIL:
        case PART_DIODE:
        case PART_PHOTODIODE:
        case PART_NPN:
        case PART_PNP:
        case PART_AMBIENT_LIGHT:
        case PART_LED:
        case PART_CAPACITOR:
        case PART_INDUCTOR:
        case PART_CLOCK:
        case PART_JUNCTION:
        case PART_NOTE:
            break;
        default:
            if (netlist_logic_supported(b->parts[i].type))
            {
                if (!netlist_logic_valid(&b->parts[i]))
                    return NETLIST_INVALID;
                ++logic_count;
            }
            else if (!netlist_device_supported(component_electrical_type(b->parts[i].type)))
                return NETLIST_UNSUPPORTED;
            break;
        }
    }
    build_nets(b, parents, !measure_wires, 0, !session);
    if (source < 0)
        for (i = 0; i < b->part_count; ++i)
            if (b->parts[i].type == PART_CLOCK)
            {
                source = i;
                break;
            }
    /* Instruments can be the only reference: generator COM (pin 1) or multimeter COM (pin 0). */
    int reference_pin = 1, supply_pin = -1;
    for (i = 0; source < 0 && i < b->part_count; ++i)
        if (b->parts[i].type == PART_FUNCTION_GENERATOR ||
            (session && b->parts[i].type == PART_MULTIMETER))
        {
            source = i;
            reference_pin = b->parts[i].type == PART_MULTIMETER ? 0 : 1;
        }
        else if (mcu_board(b->parts[i].type) && mcu_board(b->parts[i].type)->supply_volts > 0)
        {
            source = i;
            reference_pin = mcu_board(b->parts[i].type)->ground_terminal;
            supply_pin = mcu_board(b->parts[i].type)->supply_terminal;
        }
    if (source < 0)
        return NETLIST_INVALID;
    ground = find_root(parents, source * WORKBENCH_MAX_PINS + reference_pin);
    positive = find_root(parents, source * WORKBENCH_MAX_PINS + (supply_pin >= 0 ? supply_pin : 1 - reference_pin));
    if (!session && ground == positive)
        return NETLIST_INVALID;
    for (i = 0; i < b->part_count; ++i)
        if (!session && (component_electrical_type(b->parts[i].type) == PART_SOURCE || b->parts[i].type == PART_CLOCK) &&
            find_root(parents, i * WORKBENCH_MAX_PINS) == find_root(parents, i * WORKBENCH_MAX_PINS + 1))
            return NETLIST_INVALID;
    if (measure_wires)
    {
        int paths[PIN_TOTAL];
        for (i = 0; i < PIN_TOTAL; ++i)
            paths[i] = find_root(parents, i);
        for (i = 0; i < b->wire_count; ++i)
        {
            int a = pin_index(b->wires[i].a), c = pin_index(b->wires[i].b);
            if (find_root(paths, a) == find_root(paths, c))
                return NETLIST_AMBIGUOUS_WIRES;
            unite(paths, a, c);
        }
        for (i = 0; i < b->part_count; ++i)
            if (!session && (component_electrical_type(b->parts[i].type) == PART_SOURCE || b->parts[i].type == PART_CLOCK) &&
                find_root(paths, i * WORKBENCH_MAX_PINS) == find_root(paths, i * WORKBENCH_MAX_PINS + 1))
                return NETLIST_INVALID;
    }
    if (fprintf(out, "Electronics experimental %s\n", transient ? "transient" : "DC") < 0)
        return NETLIST_IO_ERROR;
    if (session)
    {
        if (!write_live_contacts(out, b, parents, ground))
            return NETLIST_IO_ERROR;
    }
    else
    {
    if (b->parts[source].type == PART_CLOCK && b->parts[source].on)
    {
        double period = 1.0 / b->parts[source].value;
        if (fprintf(out, "Vsource %d 0 PULSE(0 5 0 %.17g %.17g %.17g %.17g)\n", positive + 1, period * .001,
                    period * .001, period * .499, period) < 0)
            return NETLIST_IO_ERROR;
    }
    else if (b->parts[source].type != PART_FUNCTION_GENERATOR &&
             !write_supply(out, &b->parts[source], source, 1, positive + 1, 0))
        return NETLIST_IO_ERROR;
    for (i = 0; i < b->part_count; ++i)
        if (component_electrical_type(b->parts[i].type) == PART_SOURCE && i != source)
        {
            int a = find_root(parents, i * WORKBENCH_MAX_PINS), c = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            if (!write_supply(out, &b->parts[i], i, 0, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1))
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (b->parts[i].type == PART_CLOCK && i != source)
        {
            int a = find_root(parents, i * WORKBENCH_MAX_PINS), c = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            double period = 1.0 / b->parts[i].value, edge = period * .001, width = period * .499;
            if (b->parts[i].on)
            {
                if (fprintf(out, "Vclock%d %d %d PULSE(0 5 0 %.17g %.17g %.17g %.17g)\n", i, a == ground ? 0 : a + 1,
                            c == ground ? 0 : c + 1, edge, edge, width, period) < 0)
                    return NETLIST_IO_ERROR;
            }
            else if (fprintf(out, "Vclock%d %d %d 0\n", i, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1) < 0)
                return NETLIST_IO_ERROR;
        }
    }
    if (measure_wires)
        for (i = 0; i < b->wire_count; ++i)
        {
            int a = find_root(parents, pin_index(b->wires[i].a)), c = find_root(parents, pin_index(b->wires[i].b));
            if (fprintf(out, "Vwire%d %d %d 0\n", i, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1) < 0)
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (component_electrical_type(b->parts[i].type) == PART_RESISTOR)
        {
            int a = find_root(parents, i * WORKBENCH_MAX_PINS), c = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            if (fprintf(out, "R%d %d %d %.17g\n", i, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1,
                        component_effective_value(&b->parts[i])) < 0)
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (b->parts[i].type == PART_POTENTIOMETER)
        {
            double position = b->parts[i].control;
            if (!isfinite(position) || position < 0 || position > 1)
                return NETLIST_INVALID;
            int a = find_root(parents, i * WORKBENCH_MAX_PINS);
            int wiper = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            int c = find_root(parents, i * WORKBENCH_MAX_PINS + 2);
            const double contact_ohms = NETLIST_POT_MIN_SEGMENT_OHMS; /* Minimum segment resistance at end stops. */
            double first = fmax(contact_ohms, b->parts[i].value * position);
            double second = fmax(contact_ohms, b->parts[i].value * (1 - position));
            if (fprintf(out, "Rpot%d_a %d %d %.17g\nRpot%d_b %d %d %.17g\n", i,
                        a == ground ? 0 : a + 1, wiper == ground ? 0 : wiper + 1, first,
                        i, wiper == ground ? 0 : wiper + 1, c == ground ? 0 : c + 1, second) < 0)
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
    {
        int base = i * WORKBENCH_MAX_PINS;
        int a = find_root(parents, base), c = find_root(parents, base + 1);
        int third = find_root(parents, base + 2);
        if (!write_semiconductor(out, &b->parts[i], i, a == ground ? 0 : a + 1,
                                 c == ground ? 0 : c + 1, third == ground ? 0 : third + 1))
            return NETLIST_IO_ERROR;
    }
    for (i = 0; i < b->part_count; ++i)
        if (netlist_device_supported(b->parts[i].type))
        {
            int nodes[COMPONENT_MAX_DEVICE_PINS] = {0};
            for (int pin = 0; pin < bench_pin_count(b->parts[i].type); ++pin)
            {
                int root = find_root(parents, i * WORKBENCH_MAX_PINS + pin);
                nodes[pin] = root == ground ? 0 : root + 1;
            }
            if (!netlist_device_write(out, &b->parts[i], i, nodes))
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (netlist_logic_supported(b->parts[i].type))
        {
            int nodes[COMPONENT_MAX_DEVICE_PINS] = {0};
            for (int pin = 0; pin < bench_pin_count(b->parts[i].type); ++pin)
            {
                int root = find_root(parents, i * WORKBENCH_MAX_PINS + pin);
                nodes[pin] = root == ground ? 0 : root + 1;
            }
            if (!netlist_logic_write(out, &b->parts[i], i, nodes, protection))
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (b->parts[i].type == PART_LED)
        {
            int a = find_root(parents, i * WORKBENCH_MAX_PINS), c = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            if (session)
            {
                if (fprintf(out, "D%d led%d %d %s\nRguard%d %d led%d %.17g\n", i, i, c == ground ? 0 : c + 1,
                            component_led_model(b->parts[i].states), i, a == ground ? 0 : a + 1, i,
                            NETLIST_LIVE_CLOSED_OHMS) < 0)
                    return NETLIST_IO_ERROR;
            }
            else if (fprintf(out, "D%d %d %d %s\n", i, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1,
                             component_led_model(b->parts[i].states)) < 0)
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (b->parts[i].type == PART_CAPACITOR)
        {
            int a = find_root(parents, i * WORKBENCH_MAX_PINS), c = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            if (fprintf(out, "C%d %d %d %.17g IC=0\n", i, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1,
                        b->parts[i].value) < 0)
                return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (b->parts[i].type == PART_INDUCTOR)
        {
            int a = find_root(parents, i * WORKBENCH_MAX_PINS), c = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            if (fprintf(out, "L%d %d %d %.17g IC=0\n", i, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1,
                        b->parts[i].value) < 0)
                return NETLIST_IO_ERROR;
        }
    if (gate_count &&
        !write_logic_supply(out, positive == ground ? 0 : positive + 1, b->parts[source].type == PART_CLOCK))
        return NETLIST_IO_ERROR;
    if (gate_count)
        netlist_protection_supply(protection, -1, "logic_vcc", HC_SUPPLY_ABSOLUTE_MAX, HC_SUPPLY_RECOMMENDED_MAX);
    unsigned char scaled_inputs[PIN_TOTAL + 1] = {0};
    for (i = 0; i < b->part_count; ++i)
        if (component_has_flag(b->parts[i].type, COMPONENT_GATE_SYMBOL))
        {
            PartType type = b->parts[i].type;
            int a = find_root(parents, i * WORKBENCH_MAX_PINS), c = find_root(parents, i * WORKBENCH_MAX_PINS + 1);
            int output_node = type == PART_NOT ? c : find_root(parents, i * WORKBENCH_MAX_PINS + 2);
            const char *model = type == PART_NOR ? "gate_nor"
                                : type == PART_XNOR ? "gate_xnor"
                                : type == PART_AND    ? "gate_and"
                                : type == PART_NAND ? "gate_nand"
                                : type == PART_OR   ? "gate_or"
                                : type == PART_XOR  ? "gate_xor"
                                                    : "gate_not";
            const int inputs[2] = {a == ground ? 0 : a + 1, c == ground ? 0 : c + 1};
            const int input_count = type == PART_NOT ? 1 : 2;
            for (int input = 0; input < input_count; ++input)
                if (!scaled_inputs[inputs[input]])
                {
                    if (!write_gate_scaled_input(out, inputs[input]))
                        return NETLIST_IO_ERROR;
                    scaled_inputs[inputs[input]] = 1;
                }
            if (type == PART_NOT)
            {
                if (fprintf(out, "Aadc%d [gin%d] [din%d_0] gate_adc\nAgate%d din%d_0 dout%d %s\n", i, inputs[0], i, i, i,
                            i, model) < 0)
                    return NETLIST_IO_ERROR;
            }
            else if (type == PART_DFF)
            {
                if (fprintf(out,
                            "Aadc%d [gin%d gin%d] [din%d_0 din%d_1] gate_adc\nAgate%d din%d_0 din%d_1 null null dout%d "
                            "null gate_dff\n",
                            i, inputs[0], inputs[1], i, i, i, i, i, i) < 0)
                    return NETLIST_IO_ERROR;
            }
            else
            {
                if (fprintf(out, "Aadc%d [gin%d gin%d] [din%d_0 din%d_1] gate_adc\nAgate%d [din%d_0 din%d_1] dout%d %s\n",
                            i, inputs[0], inputs[1], i, i, i, i, i, i, model) < 0)
                    return NETLIST_IO_ERROR;
            }
            if (!write_gate_output(out, i, output_node == ground ? 0 : output_node + 1, protection))
                return NETLIST_IO_ERROR;
            for (int input = 0; input < input_count; ++input)
                if (!write_gate_input(out, "g", i, input, inputs[input], 0))
                    return NETLIST_IO_ERROR;
        }
    for (i = 0; i < b->part_count; ++i)
        if (component_has_flag(b->parts[i].type, COMPONENT_QUAD_GATE))
        {
            static const int pins[4][3] = {{0, 1, 2}, {3, 4, 5}, {8, 9, 7}, {11, 12, 10}};
            int rail = find_root(parents, i * WORKBENCH_MAX_PINS + 13),
                ref = find_root(parents, i * WORKBENCH_MAX_PINS + 6), g;
            rail = rail == ground ? 0 : rail + 1;
            ref = ref == ground ? 0 : ref + 1;
            if (fprintf(out, "Rchip%d_supply %d %d %.17g\nBchip%d_sup chip%d_sup 0 V=V(%d,%d)\n",
                        i, rail, ref, NETLIST_LOGIC_SUPPLY_OHMS, i, i, rail, ref) < 0 ||
                fprintf(out,
                        "Vchip%d_alive chip%d_alive 0 1\n"
                        "Bchip%d_ok chip%d_ok 0 V=V(chip%d_alive)*min(max((V(chip%d_sup)-%g)/%g,0),1)*"
                        "min(max((%g-V(chip%d_sup))/%g,0),1)\n",
                        i, i, i, i, i, i, QUAD_SUPPLY_MIN - QUAD_EDGE_VOLTS, QUAD_EDGE_VOLTS,
                        QUAD_SUPPLY_MAX + QUAD_EDGE_VOLTS, i, QUAD_EDGE_VOLTS) < 0)
                return NETLIST_IO_ERROR;
            char supply[PROTECTION_NAME_CAPACITY];
            snprintf(supply, sizeof(supply), "chip%d_sup", i);
            netlist_protection_supply(protection, i, supply, HC_SUPPLY_ABSOLUTE_MAX, HC_SUPPLY_RECOMMENDED_MAX);
            for (g = 0; g < 4; ++g)
            {
                int a = find_root(parents, i * WORKBENCH_MAX_PINS + pins[g][0]),
                    c = find_root(parents, i * WORKBENCH_MAX_PINS + pins[g][1]),
                    y = find_root(parents, i * WORKBENCH_MAX_PINS + pins[g][2]);
                a = a == ground ? 0 : a + 1;
                c = c == ground ? 0 : c + 1;
                y = y == ground ? 0 : y + 1;
                if (fprintf(out, "Bchip%d_a%d chip%d_a%d 0 V=V(%d,%d)\nBchip%d_b%d chip%d_b%d 0 V=V(%d,%d)\n", i, g, i,
                            g, a, ref, i, g, i, g, c, ref) < 0)
                    return NETLIST_IO_ERROR;
                if (!write_gate_input(out, "chip", i, g * 2, a, ref) ||
                    !write_gate_input(out, "chip", i, g * 2 + 1, c, ref))
                    return NETLIST_IO_ERROR;
                char high[QUAD_DRIVE_TEXT_CAPACITY], low[QUAD_DRIVE_TEXT_CAPACITY];
                quad_gate_drive(b->parts[i].type, i, g, high, low, sizeof(high));
                if (fprintf(out, "Bchip%d_hi%d chip%d_hi%d 0 V=V(chip%d_ok)*%s\n", i, g, i, g, i, high) < 0 ||
                    fprintf(out, "Bchip%d_lo%d chip%d_lo%d 0 V=V(chip%d_ok)*%s\n", i, g, i, g, i, low) < 0)
                    return NETLIST_IO_ERROR;
                if (fprintf(
                        out,
                        "Schip%d_hi%d %d %d chip%d_hi%d 0 chip_switch\nSchip%d_lo%d %d %d chip%d_lo%d 0 chip_switch\n",
                        i, g, rail, y, i, g, i, g, y, ref, i, g) < 0)
                    return NETLIST_IO_ERROR;
                char high_control[PROTECTION_NAME_CAPACITY], low_control[PROTECTION_NAME_CAPACITY];
                snprintf(high_control, sizeof(high_control), "chip%d_hi%d", i, g);
                snprintf(low_control, sizeof(low_control), "chip%d_lo%d", i, g);
                netlist_protection_output(protection, i, pins[g][2], rail, ref, y, high_control, low_control, "0",
                                          HC_OUTPUT_ABSOLUTE_MAX_AMPS);
            }
        }
    if ((chip_count || gate_count) && fprintf(out, ".model chip_switch SW(Ron=50 Roff=1e12 Vt=.5 Vh=.1)\n") < 0)
        return NETLIST_IO_ERROR;
    if (gate_count &&
        fprintf(
            out,
            ".model gate_adc adc_bridge(in_low=1.5 in_high=3.5)\n.model gate_and d_and(rise_delay=1n "
            "fall_delay=1n)\n.model gate_nand d_nand(rise_delay=1n fall_delay=1n)\n.model gate_or d_or(rise_delay=1n "
            "fall_delay=1n)\n.model gate_nor d_nor(rise_delay=1n fall_delay=1n)\n"
            ".model gate_xnor d_xnor(rise_delay=1n fall_delay=1n)\n.model gate_xor d_xor(rise_delay=1n fall_delay=1n)\n.model gate_not "
            "d_inverter(rise_delay=1n fall_delay=1n)\n.model gate_dff d_dff(ic=0 clk_delay=1n rise_delay=1n "
            "fall_delay=1n)\n.model gate_dac dac_bridge(out_low=0 out_high=1 out_undef=.5 t_rise=1n t_fall=1n)\n") < 0)
        return NETLIST_IO_ERROR;
    static const char semiconductor_models[] =
        ".model SILICON_GENERIC D(IS=1e-14 N=1.5 RS=1 BV=100 IBV=1e-6)\n"
        ".model NPN_GENERIC NPN(IS=1e-14 BF=100 VAF=100 CJE=10p CJC=4p TF=.3n)\n"
        ".model PNP_GENERIC PNP(IS=1e-14 BF=100 VAF=100 CJE=10p CJC=4p TF=.3n)\n";
    if (fputs(semiconductor_models, out) == EOF)
        return NETLIST_IO_ERROR;
    /* RS: series resistance so an LED wired straight to a source gives a finite current in this didactic model.
       rshunt: 1 TOhm from every node to ground so unfinished circuits (floating nodes) still solve. */
    if (logic_count && (!netlist_logic_models(out) || !netlist_eeprom_core(out, b)))
        return NETLIST_IO_ERROR;
    if (!netlist_device_models(out))
        return NETLIST_IO_ERROR;
    if (fprintf(out, ".model LED_GENERIC D(IS=1e-18 N=2 RS=%.17g)\n.options rshunt=1e12\n", NETLIST_LED_SERIES_OHMS) <
        0)
        return NETLIST_IO_ERROR;
    if (session)
    {
        if (fprintf(out, ".save none\n.end\n") < 0)
            return NETLIST_IO_ERROR;
        return NETLIST_OK;
    }
    if (transient)
    {
        int node = transient == 3 ? 0 : find_root(parents, pin_index(probe));
        char vector[40] = {0};
        if (transient == 2)
        {
            PartType type = b->parts[probe.component].type;
            int component = probe.component;
            if (component_electrical_type(type) == PART_SOURCE)
            {
                if (component == source)
                    snprintf(vector, sizeof(vector), "@vsource[i]");
                else
                    snprintf(vector, sizeof(vector), "@vsource%d[i]", component);
            }
            else if (type == PART_CLOCK)
            {
                if (component == source)
                    snprintf(vector, sizeof(vector), "@vsource[i]");
                else
                    snprintf(vector, sizeof(vector), "@vclock%d[i]", component);
            }
            else if (component_electrical_type(type) == PART_RESISTOR)
                snprintf(vector, sizeof(vector), "@r%d[i]", component);
            else if (type == PART_CAPACITOR)
                snprintf(vector, sizeof(vector), "@c%d[i]", component);
            else if (type == PART_INDUCTOR)
                snprintf(vector, sizeof(vector), "@l%d[i]", component);
            else if (component_current_vector(type, component, vector, sizeof(vector)))
            {
                /* Native device current: diode A->K or transistor C->E. */
            }
            else
                return NETLIST_UNSUPPORTED;
        }
        if (transient == 3)
            snprintf(vector, sizeof(vector), "@vwire%d[i]", probe.component);
        if (fprintf(out, ".control\nsave all") < 0)
            return NETLIST_IO_ERROR;
        if (transient >= 2 && fprintf(out, " %s", vector) < 0)
            return NETLIST_IO_ERROR;
        for (i = 0; i < b->part_count; ++i)
            if (b->parts[i].type == PART_LED && fprintf(out, " @d%d[id]", i) < 0)
                return NETLIST_IO_ERROR;
        if (measure_wires)
            for (i = 0; i < b->wire_count; ++i)
                if (fprintf(out, " @vwire%d[i]", i) < 0)
                    return NETLIST_IO_ERROR;
        if (fprintf(out, "\ntran %.17g %.17g uic\n", step, stop) < 0)
            return NETLIST_IO_ERROR;
        if (transient >= 2)
        {
            if (fprintf(out, "print %s\n", vector) < 0)
                return NETLIST_IO_ERROR;
        }
        else if (node == ground)
        {
            if (fprintf(out, "print time*0\n") < 0)
                return NETLIST_IO_ERROR;
        }
        else if (fprintf(out, "print v(%d)\n", node + 1) < 0)
            return NETLIST_IO_ERROR;
        for (i = 0; i < b->part_count; ++i)
            if (b->parts[i].type == PART_LED && fprintf(out, "print @d%d[id]\n", i) < 0)
                return NETLIST_IO_ERROR;
        if (measure_wires)
            for (i = 0; i < b->wire_count; ++i)
                if (fprintf(out, "print @vwire%d[i]\n", i) < 0)
                    return NETLIST_IO_ERROR;
        if (fprintf(out, "quit\n.endc\n.end\n") < 0)
            return NETLIST_IO_ERROR;
    }
    else
    {
        if (fprintf(out, ".control\nop\nprint v(%d) @vsource[i]\n", positive == ground ? 0 : positive + 1) < 0)
            return NETLIST_IO_ERROR;
        for (i = 0; i < b->part_count; ++i)
        {
            char vector[48];
            if (component_current_vector(b->parts[i].type, i, vector, sizeof(vector)) &&
                fprintf(out, "print %s\n", vector) < 0)
                return NETLIST_IO_ERROR;
        }
        if (measure_wires)
            for (i = 0; i < b->wire_count; ++i)
                if (fprintf(out, "print @vwire%d[i]\n", i) < 0)
                    return NETLIST_IO_ERROR;
        if (fprintf(out, "quit\n.endc\n.end\n") < 0)
            return NETLIST_IO_ERROR;
    }
    if (ferror(out))
        return NETLIST_IO_ERROR;
    return NETLIST_OK;
}

const char *netlist_result_message(NetlistResult result)
{
    switch (result)
    {
    case NETLIST_OK:
        return "";
    case NETLIST_AMBIGUOUS_WIRES:
        return tr(TXT_ENG_AMBIGUOUS_WIRES);
    case NETLIST_IO_ERROR:
        return tr(TXT_ENG_TEMP_FILE_FAILED);
    default:
        return tr(TXT_ENG_CIRCUIT_UNSUPPORTED);
    }
}

NetlistResult netlist_write_dc(FILE *out, const Workbench *b)
{
    return write_netlist(out, b, 0, 0, (Pin){0, 0}, 0, 0, 0, NULL);
}
NetlistResult netlist_write_dc_instrumented(FILE *out, const Workbench *b)
{
    return write_netlist(out, b, 0, 1, (Pin){0, 0}, 0, 0, 0, NULL);
}
NetlistResult netlist_write_transient(FILE *out, const Workbench *b, Pin probe, double step, double stop)
{
    return write_netlist(out, b, 1, 0, probe, step, stop, 0, NULL);
}

NetlistResult netlist_write_transient_current(FILE *out, const Workbench *b, int component, double step, double stop)
{
    return write_netlist(out, b, 2, 0, (Pin){component, 0}, step, stop, 0, NULL);
}

NetlistResult netlist_write_transient_wire(FILE *out, const Workbench *b, int wire, double step, double stop)
{
    return write_netlist(out, b, 3, 1, (Pin){wire, 0}, step, stop, 0, NULL);
}

NetlistResult netlist_write_transient_instrumented(FILE *out, const Workbench *b, int mode, Pin probe, double step,
                                                   double stop)
{
    if (mode < 1 || mode > 3)
        return NETLIST_INVALID;
    return write_netlist(out, b, mode, 1, probe, step, stop, 0, NULL);
}

NetlistResult netlist_write_live(FILE *out, const Workbench *bench, int measure_wires)
{
    return write_netlist(out, bench, 0, measure_wires, (Pin){0, 0}, 0, 0, 1, NULL);
}

NetlistResult netlist_write_live_protected(FILE *out, const Workbench *bench, int measure_wires,
                                           NetlistProtection *protection)
{
    netlist_protection_reset(protection);
    return write_netlist(out, bench, 0, measure_wires, (Pin){0, 0}, 0, 0, 1, protection);
}

int netlist_live_node(const Workbench *bench, int measure_wires, Pin pin)
{
    if (!bench || !valid_pin(bench, pin))
        return -1;
    int parents[PIN_TOTAL], source = -1;
    build_nets(bench, parents, !measure_wires, 0, 0);
    for (int i = 0; i < bench->part_count; ++i)
        if (component_electrical_type(bench->parts[i].type) == PART_SOURCE)
        {
            source = i;
            break;
        }
    if (source < 0)
        for (int i = 0; i < bench->part_count; ++i)
            if (bench->parts[i].type == PART_CLOCK)
            {
                source = i;
                break;
            }
    if (source < 0)
        for (int i = 0; i < bench->part_count; ++i)
            if (bench->parts[i].type == PART_FUNCTION_GENERATOR)
            {
                source = i;
                break;
            }
    if (source < 0)
        return -1;
    int ground = find_root(parents, source * WORKBENCH_MAX_PINS + 1);
    int node = find_root(parents, pin_index(pin));
    return node == ground ? 0 : node + 1;
}

int netlist_same_net(const Workbench *bench, Pin a, Pin b)
{
    if (!bench || a.component < 0 || b.component < 0 || a.component >= bench->part_count ||
        b.component >= bench->part_count)
        return 0;
    int *parents = malloc(sizeof(int) * PIN_TOTAL);
    if (!parents)
        return 0;
    build_nets(bench, parents, 1, 0, 0);
    int same = find_root(parents, pin_index(a)) == find_root(parents, pin_index(b));
    free(parents);
    return same;
}

/* ---- Floating logic inputs (static check, no solver work) ---- */

/* DIP and DPST contacts conduct when closed; the net builder only knows plain switches. */
static void unite_closed_contacts(const Workbench *b, int *parents)
{
    for (int i = 0; i < b->part_count; ++i)
    {
        const Part *part = &b->parts[i];
        int base = i * WORKBENCH_MAX_PINS;
        if (part->type == PART_DIP_SWITCH)
            for (int contact = 0; contact < 4; ++contact)
                if (part->states >> contact & 1u)
                    unite(parents, base + contact, base + contact + 4);
        if (part->type == PART_DPST && part->on)
        {
            unite(parents, base, base + 1);
            unite(parents, base + 2, base + 3);
        }
    }
}

static int is_contact_part(PartType type)
{
    return component_electrical_type(type) == PART_SWITCH || type == PART_BUTTON || type == PART_DIP_SWITCH ||
           type == PART_DPST;
}

/* A terminal that cannot set a level by itself: logic inputs, open contacts, strips, junctions and
 * microcontroller pins configured as plain inputs. Everything else (sources, resistors, outputs...) can. */
static int passive_terminal(const Part *part, int pin)
{
    PartType type = part->type;
    if (bench_is_board(type) || type == PART_JUNCTION || is_contact_part(type))
        return 1;
    const McuBoard *board = mcu_board(type);
    if (board)
    {
        for (int k = 0; k < board->io_count; ++k)
            if (board->io_terminal[k] == pin)
                return part->io_mode[k] == MCU_INPUT;
        return 0;
    }
    const ComponentPinInfo *info = component_pin_info(type, pin);
    int logic = component_has_flag(type, COMPONENT_GATE_SYMBOL) || component_has_flag(type, COMPONENT_QUAD_GATE) ||
                netlist_logic_supported(type);
    return logic && info && (info->role == PIN_INPUT || info->role == PIN_CLOCK_INPUT);
}

typedef struct
{
    int anchored, contacts, used;
} NetFacts;

/* Facts of every net: something sets its level; it reaches a switch-like contact; it reaches a part
 * other than strips and junctions (a used output). */
static void collect_net_facts(const Workbench *b, const int *parents, NetFacts *facts)
{
    for (int i = 0; i < b->part_count; ++i)
        for (int pin = 0; pin < bench_pin_count(b->parts[i].type); ++pin)
        {
            NetFacts *net = &facts[find_root((int *)parents, i * WORKBENCH_MAX_PINS + pin)];
            PartType type = b->parts[i].type;
            net->anchored |= !passive_terminal(&b->parts[i], pin);
            net->contacts |= is_contact_part(type);
            net->used += !bench_is_board(type) && type != PART_JUNCTION;
        }
}

/* Whether input `pin` of part i is in use: loose gates always; quad gates when their output reaches a part;
 * medium-scale chips when powered; board pins when they reach a switch-like contact. */
static int input_in_use(const Workbench *b, const int *parents, const NetFacts *facts, int i, int pin)
{
    static const int quad[4][3] = {{0, 1, 2}, {3, 4, 5}, {8, 9, 7}, {11, 12, 10}};
    PartType type = b->parts[i].type;
    int base = i * WORKBENCH_MAX_PINS;
    if (component_has_flag(type, COMPONENT_GATE_SYMBOL))
        return 1;
    if (component_has_flag(type, COMPONENT_QUAD_GATE))
    {
        for (int g = 0; g < 4; ++g)
            if (pin == quad[g][0] || pin == quad[g][1])
                return facts[find_root((int *)parents, base + quad[g][2])].used > 1;
        return 0;
    }
    if (mcu_board(type))
        return facts[find_root((int *)parents, base + pin)].contacts;
    const int vcc_pin = bench_pin_count(type) - 1; /* DIP chips: VCC is the last pin */
    return facts[find_root((int *)parents, base + vcc_pin)].anchored;
}

int netlist_floating_inputs(const Workbench *b, signed char first_pin[WORKBENCH_MAX_COMPONENTS])
{
    int *parents = malloc(sizeof(int) * PIN_TOTAL);
    NetFacts *facts = calloc(PIN_TOTAL, sizeof(NetFacts));
    int count = 0;
    if (!parents || !facts)
    {
        free(parents);
        free(facts);
        return -1;
    }
    build_nets(b, parents, 1, 0, 1);
    unite_closed_contacts(b, parents);
    collect_net_facts(b, parents, facts);
    for (int i = 0; i < b->part_count; ++i)
    {
        first_pin[i] = -1;
        for (int pin = 0; pin < bench_pin_count(b->parts[i].type) && first_pin[i] < 0; ++pin)
        {
            const NetFacts *net = &facts[find_root(parents, i * WORKBENCH_MAX_PINS + pin)];
            if (!mcu_board(b->parts[i].type) && bench_is_board(b->parts[i].type))
                break;
            if (passive_terminal(&b->parts[i], pin) && !is_contact_part(b->parts[i].type) &&
                b->parts[i].type != PART_JUNCTION && !net->anchored && input_in_use(b, parents, facts, i, pin))
            {
                first_pin[i] = (signed char)pin;
                ++count;
            }
        }
    }
    free(parents);
    free(facts);
    return count;
}
