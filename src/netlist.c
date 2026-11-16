#include "netlist.h"
#include "netlist_devices.h"
#include "mcu_board.h"
#include "component_devices.h"
#include "component_registry.h"
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
        if (type == PART_BREADBOARD)
        {
            for (j = 0; j < BREADBOARD_PINS; ++j)
                unite(parents, base + bench_board_group_first(j), base + j);
        }
    }
    for (i = 0; i < b->part_count; ++i)
    {
        int pin;
        if (b->parts[i].type == PART_BREADBOARD)
            continue;
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

/* 74HC quad gates (5 V): input low <= 1.5 V, high >= 3.5 V, supply window 4.5..5.5 V.
 * Drives are continuous (linear between thresholds, logic by products) instead of ideal steps:
 * a step made the transient timestep collapse when a clock edge crossed a threshold. */
#define QUAD_INPUT_LOW 1.5
#define QUAD_INPUT_HIGH 3.5
#define QUAD_SUPPLY_MIN 4.5
#define QUAD_SUPPLY_MAX 5.5
#define QUAD_EDGE_VOLTS .1

static void quad_gate_drive(PartType type, int chip, int gate, char *high, char *low, size_t capacity)
{
    char a[96], b[96];
    const double span = QUAD_INPUT_HIGH - QUAD_INPUT_LOW;
    snprintf(a, sizeof(a), "min(max((V(chip%d_a%d)-%g)/%g,0),1)", chip, gate, QUAD_INPUT_LOW, span);
    snprintf(b, sizeof(b), "min(max((V(chip%d_b%d)-%g)/%g,0),1)", chip, gate, QUAD_INPUT_LOW, span);
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
                                   double step, double stop, int session)
{
    int parents[PIN_TOTAL], i, source = -1, ground, positive, gate_count = 0, chip_count = 0;
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
            break;
        default:
            if (!netlist_device_supported(component_electrical_type(b->parts[i].type)))
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
    for (i = 0; source < 0 && session && i < b->part_count; ++i)
        if (b->parts[i].type == PART_FUNCTION_GENERATOR || b->parts[i].type == PART_MULTIMETER)
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
    else if (!write_supply(out, &b->parts[source], source, 1, positive + 1, 0))
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
            if (type == PART_NOT)
            {
                if (fprintf(out, "Aadc%d [%d] [din%d_0] gate_adc\nAgate%d din%d_0 dout%d %s\n", i,
                            a == ground ? 0 : a + 1, i, i, i, i, model) < 0)
                    return NETLIST_IO_ERROR;
            }
            else if (type == PART_DFF)
            {
                if (fprintf(out,
                            "Aadc%d [%d %d] [din%d_0 din%d_1] gate_adc\nAgate%d din%d_0 din%d_1 null null dout%d null "
                            "gate_dff\n",
                            i, a == ground ? 0 : a + 1, c == ground ? 0 : c + 1, i, i, i, i, i, i) < 0)
                    return NETLIST_IO_ERROR;
            }
            else
            {
                if (fprintf(out, "Aadc%d [%d %d] [din%d_0 din%d_1] gate_adc\nAgate%d [din%d_0 din%d_1] dout%d %s\n", i,
                            a == ground ? 0 : a + 1, c == ground ? 0 : c + 1, i, i, i, i, i, i, model) < 0)
                    return NETLIST_IO_ERROR;
            }
            if (fprintf(out, "Adac%d [dout%d] [%d] gate_dac\n", i, i, output_node == ground ? 0 : output_node + 1) < 0)
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
            if (fprintf(out, "Bchip%d_sup chip%d_sup 0 V=V(%d,%d)\n", i, i, rail, ref) < 0 ||
                fprintf(out,
                        "Bchip%d_ok chip%d_ok 0 V=min(max((V(chip%d_sup)-%g)/%g,0),1)*min(max((%g-V(chip%d_sup))/%g,0),1)\n",
                        i, i, i, QUAD_SUPPLY_MIN - QUAD_EDGE_VOLTS, QUAD_EDGE_VOLTS, QUAD_SUPPLY_MAX + QUAD_EDGE_VOLTS, i,
                        QUAD_EDGE_VOLTS) < 0)
                return NETLIST_IO_ERROR;
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
                char high[320], low[320];
                quad_gate_drive(b->parts[i].type, i, g, high, low, sizeof(high));
                if (fprintf(out, "Bchip%d_hi%d chip%d_hi%d 0 V=V(chip%d_ok)*%s\n", i, g, i, g, i, high) < 0 ||
                    fprintf(out, "Bchip%d_lo%d chip%d_lo%d 0 V=V(chip%d_ok)*%s\n", i, g, i, g, i, low) < 0)
                    return NETLIST_IO_ERROR;
                if (fprintf(
                        out,
                        "Schip%d_hi%d %d %d chip%d_hi%d 0 chip_switch\nSchip%d_lo%d %d %d chip%d_lo%d 0 chip_switch\n",
                        i, g, rail, y, i, g, i, g, y, ref, i, g) < 0)
                    return NETLIST_IO_ERROR;
            }
        }
    if (chip_count && fprintf(out, ".model chip_switch SW(Ron=50 Roff=1e12 Vt=.5 Vh=.1)\n") < 0)
        return NETLIST_IO_ERROR;
    if (gate_count &&
        fprintf(
            out,
            ".model gate_adc adc_bridge(in_low=1.5 in_high=3.5)\n.model gate_and d_and(rise_delay=1n "
            "fall_delay=1n)\n.model gate_nand d_nand(rise_delay=1n fall_delay=1n)\n.model gate_or d_or(rise_delay=1n "
            "fall_delay=1n)\n.model gate_nor d_nor(rise_delay=1n fall_delay=1n)\n"
            ".model gate_xnor d_xnor(rise_delay=1n fall_delay=1n)\n.model gate_xor d_xor(rise_delay=1n fall_delay=1n)\n.model gate_not "
            "d_inverter(rise_delay=1n fall_delay=1n)\n.model gate_dff d_dff(ic=0 clk_delay=1n rise_delay=1n "
            "fall_delay=1n)\n.model gate_dac dac_bridge(out_low=0 out_high=5 out_undef=2.5 t_rise=1n t_fall=1n)\n") < 0)
        return NETLIST_IO_ERROR;
    static const char semiconductor_models[] =
        ".model SILICON_GENERIC D(IS=1e-14 N=1.5 RS=1 BV=100 IBV=1e-6)\n"
        ".model NPN_GENERIC NPN(IS=1e-14 BF=100 VAF=100 CJE=10p CJC=4p TF=.3n)\n"
        ".model PNP_GENERIC PNP(IS=1e-14 BF=100 VAF=100 CJE=10p CJC=4p TF=.3n)\n";
    if (fputs(semiconductor_models, out) == EOF)
        return NETLIST_IO_ERROR;
    /* RS: series resistance so an LED wired straight to a source gives a finite current in this didactic model.
       rshunt: 1 TOhm from every node to ground so unfinished circuits (floating nodes) still solve. */
    if (!netlist_device_models(out))
        return NETLIST_IO_ERROR;
    if (fprintf(out, ".model LED_GENERIC D(IS=1e-18 N=2 RS=%.17g)\n.options rshunt=1e12\n", NETLIST_LED_SERIES_OHMS) <
        0)
        return NETLIST_IO_ERROR;
    if (session)
    {
        if (fprintf(out, ".save all") < 0)
            return NETLIST_IO_ERROR;
        for (i = 0; i < b->part_count; ++i)
        {
            char vector[48];
            if (component_current_vector(b->parts[i].type, i, vector, sizeof(vector)) &&
                fprintf(out, " %s", vector) < 0)
                return NETLIST_IO_ERROR;
            for (int element = 0; element < COMPONENT_MAX_ELEMENTS; ++element)
                if (component_element_vector(b->parts[i].type, i, element, vector, sizeof(vector)) && vector[0] == '@' &&
                    fprintf(out, " %s", vector) < 0)
                    return NETLIST_IO_ERROR;
        }
        if (fprintf(out, "\n.end\n") < 0)
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

NetlistResult netlist_write_dc(FILE *out, const Workbench *b)
{
    return write_netlist(out, b, 0, 0, (Pin){0, 0}, 0, 0, 0);
}
NetlistResult netlist_write_dc_instrumented(FILE *out, const Workbench *b)
{
    return write_netlist(out, b, 0, 1, (Pin){0, 0}, 0, 0, 0);
}
NetlistResult netlist_write_transient(FILE *out, const Workbench *b, Pin probe, double step, double stop)
{
    return write_netlist(out, b, 1, 0, probe, step, stop, 0);
}

NetlistResult netlist_write_transient_current(FILE *out, const Workbench *b, int component, double step, double stop)
{
    return write_netlist(out, b, 2, 0, (Pin){component, 0}, step, stop, 0);
}

NetlistResult netlist_write_transient_wire(FILE *out, const Workbench *b, int wire, double step, double stop)
{
    return write_netlist(out, b, 3, 1, (Pin){wire, 0}, step, stop, 0);
}

NetlistResult netlist_write_transient_instrumented(FILE *out, const Workbench *b, int mode, Pin probe, double step,
                                                   double stop)
{
    if (mode < 1 || mode > 3)
        return NETLIST_INVALID;
    return write_netlist(out, b, mode, 1, probe, step, stop, 0);
}

NetlistResult netlist_write_live(FILE *out, const Workbench *bench, int measure_wires)
{
    return write_netlist(out, bench, 0, measure_wires, (Pin){0, 0}, 0, 0, 1);
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
