#include "spice_live.h"
#include "spice_session.h"
#include "spice_session_protocol.h"
#include "netlist.h"
#include "netlist_devices.h"
#include "mcu_board.h"
#include "component_devices.h"
#include "component_registry.h"
#include "ui_text.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SpiceLive
{
    SpiceSession *session;
    Workbench previous;
    char *topology;
    int measures_wires, started, restarted, discarded;
    unsigned timeout_ms;
    double time, requested_time;
    char probe_vector[48];
    double probe_interval;
    int probe_armed, probe_valid;
    unsigned char burnt[WORKBENCH_MAX_COMPONENTS], reason[WORKBENCH_MAX_COMPONENTS];
    unsigned char shorted[WORKBENCH_MAX_COMPONENTS];
    uint64_t shorted_revision;
    int shorted_valid;
    double stress[WORKBENCH_MAX_COMPONENTS];
    NetlistProtection protection;
    int protection_cursor; /* next output term to read */
    signed char floating[WORKBENCH_MAX_COMPONENTS]; /* first floating input per part, -1 none */
    unsigned char alert[WORKBENCH_MAX_COMPONENTS], alert_pin[WORKBENCH_MAX_COMPONENTS];
    double alert_value[WORKBENCH_MAX_COMPONENTS];
};

SpiceLive *spice_live_create(void)
{
    SpiceLive *live = calloc(1, sizeof(SpiceLive));
    if (live)
        memset(live->floating, -1, sizeof(live->floating));
    return live;
}

void spice_live_destroy(SpiceLive *live)
{
    if (!live)
        return;
    spice_session_close(live->session);
    bench_snapshot_clear(&live->previous);
    free(live->topology);
    netlist_protection_free(&live->protection);
    free(live);
}

int spice_live_restarted(const SpiceLive *live)
{
    return live && live->restarted;
}

void spice_live_set_timeout_ms(SpiceLive *live, unsigned milliseconds)
{
    if (!live)
        return;
    live->timeout_ms = milliseconds;
    spice_session_set_timeout(live->session, milliseconds);
}

/* After a failed request the child may still be computing, and its late reply would answer the
 * next request. Closing the child discards that reply; the next batch rebuilds the session. */
static int discard_session(SpiceLive *live)
{
    spice_session_close(live->session);
    live->session = NULL;
    free(live->topology);
    live->topology = NULL;
    live->discarded = 1;
    return 0;
}

double spice_live_time(const SpiceLive *live)
{
    return live ? live->time : 0;
}

static char *topology_key(const Workbench *bench, int measure_wires, int *instrumented);

int spice_live_sync_memory(SpiceLive *live, Workbench *bench, char *message, size_t capacity)
{
    if (!live || !live->session || !bench)
        return 0;
    int changed = 0;
    for (;;)
    {
        RomImage image;
        int part;
        if (!spice_session_eeprom(live->session, &part, &image, message, capacity))
            return 0;
        if (part == -1)
            break;
        if (part < 0 || part >= bench->part_count || bench->parts[part].type != PART_AT28C16)
        {
            snprintf(message, capacity, "%s", tr(TXT_ENG_REPLY_INVALID));
            return 0;
        }
        char text[ROM_IMAGE_BYTES * 4 + 1];
        size_t used = 0;
        for (int address = 0; address < ROM_IMAGE_BYTES; ++address)
        {
            if (address % 16 == 0)
                used += (size_t)snprintf(text + used, sizeof(text) - used, "%03X:", address);
            used += (size_t)snprintf(text + used, sizeof(text) - used, " %02X%s", image.data[address],
                                     address % 16 == 15 ? "\n" : "");
        }
        if (!bench_set_code(bench, part, text))
        {
            snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY));
            return 0;
        }
        changed = 1;
    }
    if (changed)
    {
        int instrumented;
        char *key = topology_key(bench, live->measures_wires, &instrumented);
        if (!key)
        {
            snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY));
            return 0;
        }
        free(live->topology);
        live->topology = key;
    }
    return 1;
}

static char *topology_key(const Workbench *bench, int measure_wires, int *instrumented)
{
    Workbench normalized = {0};
    if (!bench_snapshot_clone(&normalized, bench))
        return NULL;
    for (int i = 0; i < normalized.part_count; ++i)
    {
        Part *part = &normalized.parts[i];
        const ComponentInfo *info = component_info(part->type);
        if (!info)
        {
            bench_snapshot_clear(&normalized);
            return NULL;
        }
        part->on = 1;
        part->control = .5;
        if (!netlist_device_rebuilds_on_change(part->type))
            part->states = 0;
        memset(part->io_level, 0, sizeof(part->io_level));
        memset(part->io_mode, 0, sizeof(part->io_mode));
        if (info->primary && part->type != PART_CLOCK && !netlist_device_rebuilds_on_change(part->type))
            part->value = info->primary->initial;
    }
    FILE *file = tmpfile();
    if (!file)
    {
        bench_snapshot_clear(&normalized);
        return NULL;
    }
    *instrumented = measure_wires;
    NetlistResult result = netlist_write_live(file, &normalized, measure_wires);
    if (result == NETLIST_AMBIGUOUS_WIRES)
    {
        fclose(file);
        file = tmpfile();
        *instrumented = 0;
        result = file ? netlist_write_live(file, &normalized, 0) : NETLIST_IO_ERROR;
    }
    bench_snapshot_clear(&normalized);
    char *key = NULL;
    if (file && result == NETLIST_OK && !fflush(file) && !fseek(file, 0, SEEK_END))
    {
        long length = ftell(file);
        if (length >= 0 && !fseek(file, 0, SEEK_SET))
        {
            key = malloc((size_t)length + 1);
            if (key)
            {
                if (fread(key, 1, (size_t)length, file) != (size_t)length)
                {
                    free(key);
                    key = NULL;
                }
                else
                    key[length] = 0;
            }
        }
    }
    if (file)
        fclose(file);
    return key;
}

static int load_session(SpiceLive *live, const Workbench *bench, const char *executable, char *message, size_t capacity)
{
    char directory[MAX_PATH], path[MAX_PATH];
    if (!GetTempPathA(sizeof(directory), directory) || !GetTempFileNameA(directory, "blc", 0, path))
    {
        snprintf(message, capacity, tr(TXT_ENG_TEMP_FILE_WINDOWS), GetLastError());
        return 0;
    }
    FILE *file = fopen(path, "w");
    if (!file)
    {
        DeleteFileA(path);
        snprintf(message, capacity, "%s", tr(TXT_ENG_TEMP_FILE_FAILED));
        return 0;
    }
    NetlistResult netlist = netlist_write_live_protected(file, bench, live->measures_wires, &live->protection);
    int failed = fclose(file);
    if (netlist != NETLIST_OK || failed)
    {
        DeleteFileA(path);
        const char *reason = netlist != NETLIST_OK ? netlist_result_message(netlist) : tr(TXT_ENG_TEMP_FILE_FAILED);
        snprintf(message, capacity, "%s", reason);
        return 0;
    }
    spice_session_close(live->session);
    live->session = spice_session_open(executable, message, capacity);
    spice_session_set_timeout(live->session, live->timeout_ms);
    int success = live->session && spice_session_load(live->session, path, message, capacity);
    DeleteFileA(path);
    live->started = 0;
    live->time = 0;
    live->requested_time = 0;
    live->probe_armed = 0;
    memset(live->burnt, 0, sizeof(live->burnt));
    memset(live->alert, 0, sizeof(live->alert));
    live->protection_cursor = 0;
    return success;
}

static int alter(SpiceLive *live, const char *name, double value, char *message, size_t capacity)
{
    char command[128];
    snprintf(command, sizeof(command), "alter %s = %.17g", name, value);
    return spice_session_command(live->session, command, message, capacity);
}

static int primary_source(const Workbench *bench)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (component_electrical_type(bench->parts[i].type) == PART_SOURCE)
            return i;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == PART_CLOCK)
            return i;
    return -1;
}

/* A burnt board drives nothing: level 0, output and pull-up resistors open (controls come in threes). */
static double burnt_board_control(int control)
{
    return control % 3 ? NETLIST_LIVE_OPEN_OHMS : 0;
}

static int apply_controls(SpiceLive *live, const Workbench *bench, char *message, size_t capacity)
{
    int primary = primary_source(bench);
    for (int i = 0; i < bench->part_count; ++i)
    {
        const Part *part = &bench->parts[i];
        char name[48];
        if (component_electrical_type(part->type) == PART_SOURCE || part->type == PART_CLOCK)
        {
            snprintf(name, sizeof(name), "Rpower%d", i);
            if (!alter(live, name, part->on ? component_source_resistance(part->type) : NETLIST_LIVE_OPEN_OHMS, message, capacity))
                return 0;
            if (component_electrical_type(part->type) == PART_SOURCE)
            {
                if (i == primary)
                    snprintf(name, sizeof(name), "Vsource");
                else
                    snprintf(name, sizeof(name), "Vsource%d", i);
                if (!alter(live, name, part->value, message, capacity))
                    return 0;
                snprintf(name, sizeof(name), "Vilim%d", i);
                if (part->type == PART_SOURCE && !alter(live, name, component_source_limit(part), message, capacity))
                    return 0;
            }
        }
        if (component_electrical_type(part->type) == PART_SWITCH || part->type == PART_BUTTON)
        {
            snprintf(name, sizeof(name), "Rcontact%d", i);
            if (!alter(live, name, component_effective_on(part) ? NETLIST_LIVE_CLOSED_OHMS : NETLIST_LIVE_OPEN_OHMS, message, capacity))
                return 0;
        }
        if (component_electrical_type(part->type) == PART_RESISTOR || part->type == PART_CAPACITOR || part->type == PART_INDUCTOR)
        {
            char prefix = 'L';
            if (component_electrical_type(part->type) == PART_RESISTOR)
                prefix = 'R';
            else if (part->type == PART_CAPACITOR)
                prefix = 'C';
            snprintf(name, sizeof(name), "%c%d", prefix, i);
            if (!alter(live, name, component_effective_value(part), message, capacity))
                return 0;
        }
        if (part->type == PART_TEMPERATURE)
        {
            snprintf(name, sizeof(name), "Venv%d", i);
            if (!alter(live, name, component_environment_value(part), message, capacity))
                return 0;
        }
        if (part->type == PART_SOIL)
        {
            snprintf(name, sizeof(name), "Rsoil%d", i);
            if (!alter(live, name, component_effective_value(part), message, capacity))
                return 0;
        }
        if (part->type == PART_PHOTODIODE || part->type == PART_AMBIENT_LIGHT)
        {
            snprintf(name, sizeof(name), "Iphoto%d", i);
            if (!alter(live, name, component_photocurrent(part), message, capacity))
                return 0;
        }
        for (int control = 0; control < netlist_device_control_count(part); ++control)
        {
            double value;
            if (!netlist_device_control(part, i, control, name, sizeof(name), &value))
                continue;
            if (live->burnt[i] && mcu_board(part->type))
                value = burnt_board_control(control);
            if (!alter(live, name, value, message, capacity))
                return 0;
        }
        if (part->type == PART_LED)
        {
            snprintf(name, sizeof(name), "Rguard%d", i);
            if (!alter(live, name, NETLIST_LIVE_CLOSED_OHMS, message, capacity))
                return 0;
        }
        if (part->type == PART_POTENTIOMETER)
        {
            const double minimum_segment_ohms = NETLIST_POT_MIN_SEGMENT_OHMS;
            snprintf(name, sizeof(name), "Rpot%d_a", i);
            if (!alter(live, name, fmax(minimum_segment_ohms, part->value * part->control), message, capacity))
                return 0;
            snprintf(name, sizeof(name), "Rpot%d_b", i);
            if (!alter(live, name, fmax(minimum_segment_ohms, part->value * (1 - part->control)), message, capacity))
                return 0;
        }
    }
    memset(live->burnt, 0, sizeof(live->burnt));
    return 1;
}

static int advance(SpiceLive *live, double step, double span, char *message, size_t capacity)
{
    char command[160];
    double target = live->requested_time + span;
    if (live->started && target <= live->time)
    {
        live->requested_time = target;
        return 1;
    }
    if (!spice_session_command(live->session, "delete all", message, capacity))
        return 0;
    /* delete all also removes the save rule; restore single-sample storage before tran/resume. */
    if (!spice_session_command(live->session, "save none", message, capacity))
        return 0;
    snprintf(command, sizeof(command), "stop when time >= %.17g", target);
    if (!spice_session_command(live->session, command, message, capacity))
        return 0;
    if (live->started)
        snprintf(command, sizeof(command), "resume");
    else
        snprintf(command, sizeof(command), "tran %.17g 1e12 0 %.17g uic", step, step);
    if (!spice_session_command(live->session, command, message, capacity) ||
        !spice_session_value(live->session, "time", &live->time, message, capacity))
        return 0;
    live->started = 1;
    live->requested_time = target;
    return 1;
}

_Static_assert(SPICE_SESSION_MAX_SAMPLES <= SIM_LIVE_MAX_SAMPLES, "live samples must fit the result");

/* Solver vector of the probed point; returns 0 when the point has none (for example ground). */
static int probe_vector(const SpiceLive *live, const Workbench *bench, const SimLiveProbe *probe,
                        char *vector, size_t capacity)
{
    const Pin target = probe->target;
    if (probe->kind == SIM_LIVE_PROBE_VOLTAGE)
    {
        int node = netlist_live_node(bench, live->measures_wires, target);
        return node > 0 && snprintf(vector, capacity, "%d", node) < (int)capacity;
    }
    if (probe->kind == SIM_LIVE_PROBE_PART_CURRENT)
        return target.component >= 0 && target.component < bench->part_count &&
               component_current_vector(bench->parts[target.component].type, target.component, vector, capacity);
    if (probe->kind == SIM_LIVE_PROBE_WIRE_CURRENT)
        return live->measures_wires && target.component >= 0 && target.component < bench->wire_count &&
               snprintf(vector, capacity, "vwire%d#branch", target.component) < (int)capacity;
    return 0;
}

static int arm_probe(SpiceLive *live, const Workbench *bench, const SimLiveProbe *probe, char *message,
                     size_t capacity)
{
    char vector[sizeof(live->probe_vector)] = "";
    int active = probe->kind != SIM_LIVE_PROBE_NONE && isfinite(probe->interval) && probe->interval > 0;
    live->probe_valid = active && probe_vector(live, bench, probe, vector, sizeof(vector));
    if (!live->probe_valid)
        vector[0] = 0;
    double interval = live->probe_valid ? probe->interval : 1;
    if (live->probe_armed && !strcmp(vector, live->probe_vector) && interval == live->probe_interval)
        return 1;
    if (!spice_session_probe(live->session, vector, interval, message, capacity))
        return 0;
    snprintf(live->probe_vector, sizeof(live->probe_vector), "%s", vector);
    live->probe_interval = interval;
    live->probe_armed = 1;
    return 1;
}

static int read_samples(SpiceLive *live, SimAutoResult *result)
{
    SimLiveSamples *samples = &result->live_samples;
    samples->valid = live->probe_valid;
    if (!live->probe_valid)
        return 1;
    SpiceSampleBlock block;
    if (!spice_session_samples(live->session, &block, result->message, sizeof(result->message)))
        return 0;
    samples->start = block.start;
    samples->interval = block.interval;
    samples->count = block.count;
    memcpy(samples->values, block.values, sizeof(double) * (size_t)block.count);
    return 1;
}

static int read_currents(SpiceLive *live, const Workbench *bench, SimAutoResult *result)
{
    char vector[48];
    for (int i = 0; i < bench->part_count; ++i)
    {
        if (!component_current_vector(bench->parts[i].type, i, vector, sizeof(vector)))
            continue;
        if (!spice_session_value(live->session, vector, &result->currents[i], result->message, sizeof(result->message)))
            return 0;
    }
    for (int i = 0; i < bench->part_count; ++i)
        for (int element = 0; element < COMPONENT_MAX_ELEMENTS; ++element)
            if (component_element_vector(bench->parts[i].type, i, element, vector, sizeof(vector)) &&
                !spice_session_value(live->session, vector, &result->elements[i][element], result->message,
                                     sizeof(result->message)))
                return 0;
    if (primary_source(bench) >= 0 &&
        !spice_session_value(live->session, "vsource#branch", &result->source_current,
                             result->message, sizeof(result->message)))
        return 0;
    if (live->measures_wires)
        for (int wire = 0; wire < bench->wire_count; ++wire)
        {
            snprintf(vector, sizeof(vector), "vwire%d#branch", wire);
            if (!spice_session_value(live->session, vector, &result->wire_currents[wire],
                                     result->message, sizeof(result->message)))
                return 0;
        }
    result->measures_wires = live->measures_wires;
    return 1;
}

static int controls_changed(const Workbench *previous, const Workbench *bench)
{
    if (previous->part_count != bench->part_count)
        return 1;
    for (int i = 0; i < bench->part_count; ++i)
        if (previous->parts[i].on != bench->parts[i].on || previous->parts[i].value != bench->parts[i].value ||
            previous->parts[i].control != bench->parts[i].control || previous->parts[i].states != bench->parts[i].states ||
            memcmp(previous->parts[i].io_level, bench->parts[i].io_level, sizeof(bench->parts[i].io_level)) ||
            memcmp(previous->parts[i].io_mode, bench->parts[i].io_mode, sizeof(bench->parts[i].io_mode)))
            return 1;
    return 0;
}

static int protect_sources(SpiceLive *live, const Workbench *bench, char *message, size_t capacity)
{
    if (!live->shorted_valid || live->shorted_revision != bench->revision)
    {
        netlist_shorted_sources(bench, live->shorted);
        live->shorted_revision = bench->revision;
        live->shorted_valid = 1;
    }
    for (int i = 0; i < bench->part_count; ++i)
        /* The bench supply limits its current (CV/CC); batteries and clocks have no such protection. */
        if (live->shorted[i] && bench->parts[i].on && !live->burnt[i] && bench->parts[i].type != PART_SOURCE)
        {
            char name[48];
            snprintf(name, sizeof(name), "Rpower%d", i);
            if (!alter(live, name, NETLIST_LIVE_OPEN_OHMS, message, capacity))
                return 0;
            live->burnt[i] = 1;
            live->reason[i] = BURN_SHORT;
            live->stress[i] = 0;
        }
    return 1;
}

/* Current that decides a burn: the part's own, or the largest segment/color of a multi-LED device. */
static double burn_current(const Workbench *bench, int part, const SimAutoResult *result)
{
    PartType type = bench->parts[part].type;
    if (type != PART_SEVEN_SEGMENT && type != PART_RGB_LED)
        return result->currents[part];
    double largest = 0;
    for (int element = 0; element < COMPONENT_MAX_ELEMENTS; ++element)
        largest = fmax(largest, fabs(result->elements[part][element]));
    return largest;
}

/* Opens what carries a burnt part's current: LED and segment guards, or the resistor itself. */
static int open_burnt_part(SpiceLive *live, const Workbench *bench, int part, SimAutoResult *result)
{
    static const char colors[] = "rgb";
    PartType type = bench->parts[part].type;
    int guards = type == PART_SEVEN_SEGMENT ? COMPONENT_SEGMENTS : type == PART_RGB_LED ? 3 : 1;
    for (int guard = 0; guard < guards; ++guard)
    {
        char name[48];
        if (type == PART_SEVEN_SEGMENT)
            snprintf(name, sizeof(name), "Rguard%d_s%d", part, guard);
        else if (type == PART_RGB_LED)
            snprintf(name, sizeof(name), "Rguard%d_%c", part, colors[guard]);
        else
            snprintf(name, sizeof(name), type == PART_LED ? "Rguard%d" : "R%d", part);
        if (!alter(live, name, NETLIST_LIVE_OPEN_OHMS, result->message, sizeof(result->message)))
            return 0;
    }
    return 1;
}

static int protect_parts(SpiceLive *live, const Workbench *bench, const SimAutoOptions *options, SimAutoResult *result)
{
    for (int pass = 0; pass < bench->part_count; ++pass)
    {
        int worst = -1;
        double highest = 1;
        for (int i = 0; i < bench->part_count; ++i)
        {
            BurnReason reason;
            double measured;
            double stress = burn_stress(&bench->parts[i], burn_current(bench, i, result), &options->limits, &reason,
                                        &measured);
            if (!live->burnt[i] && stress > highest)
            {
                highest = stress;
                worst = i;
                live->reason[i] = (unsigned char)reason;
                live->stress[i] = measured;
            }
        }
        if (worst < 0)
            return 1;
        if (!open_burnt_part(live, bench, worst, result))
            return 0;
        live->burnt[worst] = 1;
        if (!advance(live, options->step, options->step, result->message, sizeof(result->message)) ||
            !read_currents(live, bench, result))
            return 0;
    }
    return 1;
}

/* A bench supply delivering its limit is in constant current (CC); the front panel shows it. */
static int mark_limited_supplies(SpiceLive *live, const Workbench *bench, SimAutoResult *result)
{
    int primary = primary_source(bench);
    for (int i = 0; i < bench->part_count; ++i)
    {
        const Part *part = &bench->parts[i];
        if (part->type != PART_SOURCE || !part->on || live->burnt[i])
            continue;
        double current = result->source_current;
        char vector[48];
        snprintf(vector, sizeof(vector), "vsource%d#branch", i);
        if (i != primary &&
            !spice_session_value(live->session, vector, &current, result->message, sizeof(result->message)))
            return 0;
        if (-current < component_source_limit(part) * SOURCE_LIMIT_SHOWN_RATIO || -current < SOURCE_LIMIT_SHOWN_MIN_AMPS)
            continue;
        result->alert[i] = PART_ALERT_CURRENT_LIMIT;
        result->alert_value[i] = -current;
    }
    return 1;
}

/* A burnt logic part opens its outputs: packaged chips lose `alive`, loose gates their output resistor. */
static int kill_logic_part(SpiceLive *live, const Workbench *bench, int part, BurnReason reason, double value,
                           SimAutoResult *result)
{
    if (live->burnt[part])
        return 1;
    char name[48];
    PartType type = bench->parts[part].type;
    double open = 0;
    if (component_has_flag(type, COMPONENT_GATE_SYMBOL))
    {
        snprintf(name, sizeof(name), "Rgout%d", part);
        open = NETLIST_LIVE_OPEN_OHMS;
    }
    else
        snprintf(name, sizeof(name), component_has_flag(type, COMPONENT_QUAD_GATE) ? "Vchip%d_alive" : "Vl%d_alive",
                 part);
    if (!alter(live, name, open, result->message, sizeof(result->message)))
        return 0;
    live->burnt[part] = 1;
    live->reason[part] = (unsigned char)reason;
    live->stress[part] = value;
    return 1;
}

static void note_alert(SpiceLive *live, int part, PartAlert alert, int pin, double value)
{
    if (live->alert[part] && fabs(live->alert_value[part]) >= fabs(value))
        return;
    live->alert[part] = (unsigned char)alert;
    live->alert_pin[part] = (unsigned char)(pin < 0 ? 0 : pin);
    live->alert_value[part] = value;
}

/* Node names of one output term in reading order; "0" (ground) is never requested. */
enum { OUTPUT_READINGS = 6, PROTECTION_OUTPUTS_PER_STEP = SPICE_HOST_VALUES_MAX / OUTPUT_READINGS };
static const char *output_node(const ProtectionCurrent *t, int reading)
{
    const char *names[OUTPUT_READINGS] = {t->high, t->vcc, t->node, t->low_plus, t->low_minus, t->gnd};
    return names[reading];
}

/* Reads the next PROTECTION_OUTPUTS_PER_STEP outputs (round robin) in one request and stores their currents. */
static int measure_outputs(SpiceLive *live, SimAutoResult *result)
{
    NetlistProtection *p = &live->protection;
    const char *names[SPICE_HOST_VALUES_MAX];
    double values[SPICE_HOST_VALUES_MAX];
    int terms[PROTECTION_OUTPUTS_PER_STEP], count = 0, wanted = 0;
    for (; count < PROTECTION_OUTPUTS_PER_STEP && count < p->current_count; ++count)
    {
        terms[count] = live->protection_cursor;
        live->protection_cursor = (live->protection_cursor + 1) % p->current_count;
        for (int r = 0; r < OUTPUT_READINGS; ++r)
            if (strcmp(output_node(&p->current[terms[count]], r), "0"))
                names[wanted++] = output_node(&p->current[terms[count]], r);
    }
    if (wanted && !spice_session_values(live->session, names, wanted, values, result->message,
                                        sizeof(result->message)))
        return 0;
    for (int k = 0, used = 0; k < count; ++k)
    {
        double v[OUTPUT_READINGS];
        ProtectionCurrent *t = &p->current[terms[k]];
        for (int r = 0; r < OUTPUT_READINGS; ++r)
            v[r] = strcmp(output_node(t, r), "0") ? values[used++] : 0;
        t->measured_amps = ((v[0] > LOGIC_SWITCH_ON_CONTROL ? fabs(v[1] - v[2]) : 0) +
                            (v[3] - v[4] > LOGIC_SWITCH_ON_CONTROL ? fabs(v[2] - v[5]) : 0)) / LOGIC_OUTPUT_OHMS;
    }
    return 1;
}

static int measure_supplies(SpiceLive *live, SimAutoResult *result)
{
    NetlistProtection *p = &live->protection;
    for (int first = 0; first < p->voltage_count; first += SPICE_HOST_VALUES_MAX)
    {
        const char *names[SPICE_HOST_VALUES_MAX];
        double values[SPICE_HOST_VALUES_MAX];
        int count = p->voltage_count - first < SPICE_HOST_VALUES_MAX ? p->voltage_count - first : SPICE_HOST_VALUES_MAX;
        for (int i = 0; i < count; ++i)
            names[i] = p->voltage[first + i].supply;
        if (!spice_session_values(live->session, names, count, values, result->message, sizeof(result->message)))
            return 0;
        for (int i = 0; i < count; ++i)
            p->voltage[first + i].measured_volts = values[i];
    }
    return 1;
}

/* Supply terms; part -1 is the hidden supply shared by every loose gate symbol. */
static int judge_supply(SpiceLive *live, const Workbench *bench, const ProtectionVoltage *t, SimAutoResult *result)
{
    double volts = t->measured_volts;
    int burns = volts > t->absolute_max || -volts > LOGIC_REVERSE_ABSOLUTE_MAX;
    if (!burns && volts <= t->recommended_max)
        return 1;
    for (int part = 0; part < bench->part_count; ++part)
    {
        if (t->part >= 0 ? part != t->part : !component_has_flag(bench->parts[part].type, COMPONENT_GATE_SYMBOL))
            continue;
        if (burns && !kill_logic_part(live, bench, part, BURN_OVERVOLTAGE, volts, result))
            return 0;
        if (!live->burnt[part])
            note_alert(live, part, PART_ALERT_OVERVOLTAGE, -1, volts);
    }
    return 1;
}

static int kill_board(SpiceLive *live, const Workbench *bench, int part, BurnReason reason, double value,
                      SimAutoResult *result)
{
    if (live->burnt[part])
        return 1;
    live->burnt[part] = 1;
    live->reason[part] = (unsigned char)reason;
    live->stress[part] = value;
    for (int control = 0; control < netlist_device_control_count(&bench->parts[part]); ++control)
    {
        char name[48];
        double value_now;
        if (netlist_device_control(&bench->parts[part], part, control, name, sizeof(name), &value_now) &&
            !alter(live, name, burnt_board_control(control), result->message, sizeof(result->message)))
            return 0;
    }
    return 1;
}

/* Board pins against their chip's datasheet: output current, pin voltage and (VCC-fed chips) supply. */
static int protect_board(SpiceLive *live, const Workbench *bench, int part, SimAutoResult *result)
{
    const Part *board_part = &bench->parts[part];
    const McuBoard *board = mcu_board(board_part->type);
    const McuRatings *ratings = mcu_board_ratings(board_part->type);
    char names[2 * WORKBENCH_MAX_IO + 1][32];
    const char *list[2 * WORKBENCH_MAX_IO + 1];
    double values[2 * WORKBENCH_MAX_IO + 1];
    int count = 0;
    for (int k = 0; k < board->io_count; ++k)
    {
        snprintf(names[count], sizeof(names[count]), "mcu%d_in%d", part, k);
        list[count] = names[count];
        ++count;
        snprintf(names[count], sizeof(names[count]), "@rmcu%d_o%d[i]", part, k);
        list[count] = names[count];
        ++count;
    }
    if (board->supply_volts <= 0)
    {
        snprintf(names[count], sizeof(names[count]), "mcu%d_vcc", part);
        list[count] = names[count];
        ++count;
    }
    if (!spice_session_values(live->session, list, count, values, result->message, sizeof(result->message)))
        return 0;
    double supply = board->supply_volts > 0 ? board->supply_volts : values[count - 1];
    if (board->supply_volts <= 0 && supply > ratings->supply_absolute_max)
        return kill_board(live, bench, part, BURN_OVERVOLTAGE, supply, result);
    if (board->supply_volts <= 0 && supply > ratings->supply_recommended_max)
        note_alert(live, part, PART_ALERT_OVERVOLTAGE, board->supply_terminal, supply);
    for (int k = 0; k < board->io_count; ++k)
    {
        double volts = values[2 * k], amps = fabs(values[2 * k + 1]);
        if (volts > fmax(supply, 0) + ratings->pin_over_supply || volts < -ratings->pin_over_supply)
            return kill_board(live, bench, part, BURN_PIN_VOLTAGE, volts, result);
        if (amps >= ratings->pin_amps * PROTECTION_BURN_CURRENT_RATIO)
            return kill_board(live, bench, part, BURN_OVERCURRENT, amps, result);
        if (amps > ratings->pin_amps)
            note_alert(live, part, PART_ALERT_OVERCURRENT, board->io_terminal[k], amps);
    }
    return 1;
}

/* Datasheet check of logic parts: every supply each step, outputs in round robin; alerts come from the
 * latest reading of every term, burns happen as soon as a reading crosses the burn level. */
static int protect_logic(SpiceLive *live, const Workbench *bench, SimAutoResult *result)
{
    NetlistProtection *p = &live->protection;
    if (!measure_outputs(live, result) || !measure_supplies(live, result))
        return 0;
    memset(live->alert, 0, sizeof(live->alert));
    for (int i = 0; i < p->voltage_count; ++i)
        if (!judge_supply(live, bench, &p->voltage[i], result))
            return 0;
    for (int i = 0; i < p->current_count; ++i)
    {
        const ProtectionCurrent *t = &p->current[i];
        double stress = t->measured_amps / t->limit_amps;
        if (stress < 1 || live->burnt[t->part])
            continue;
        if (stress >= PROTECTION_BURN_CURRENT_RATIO)
        {
            if (!kill_logic_part(live, bench, t->part, BURN_OVERCURRENT, t->measured_amps, result))
                return 0;
            continue;
        }
        note_alert(live, t->part, PART_ALERT_OVERCURRENT, t->pin, t->measured_amps);
    }
    for (int part = 0; part < bench->part_count; ++part)
        if (mcu_board(bench->parts[part].type) && !live->burnt[part] && !protect_board(live, bench, part, result))
            return 0;
    for (int part = 0; part < bench->part_count; ++part)
        if (live->floating[part] >= 0 && !live->alert[part] && !live->burnt[part])
            note_alert(live, part, PART_ALERT_FLOATING_INPUT, live->floating[part], 0);
    memcpy(result->alert, live->alert, sizeof(result->alert));
    memcpy(result->alert_pin, live->alert_pin, sizeof(result->alert_pin));
    memcpy(result->alert_value, live->alert_value, sizeof(result->alert_value));
    return 1;
}

int spice_live_step(SpiceLive *live, const Workbench *bench, const char *executable,
                    const SimAutoOptions *options, double span, SimAutoResult *result)
{
    if (!live || !bench || !options || !result || !isfinite(span) || span <= 0 ||
        !isfinite(options->step) || options->step <= 0)
        return 0;
    memset(result, 0, sizeof(*result));
    live->restarted = 0;
    int rebuilt = 0;
    /* Microcontroller pin drive changes without a model revision; it still needs `alter`. */
    if (!live->session || bench->revision != live->previous.revision || controls_changed(&live->previous, bench))
    {
        int instrumented;
        char *key = topology_key(bench, options->instrument_wires, &instrumented);
        if (!key)
        {
            snprintf(result->message, sizeof(result->message), "%s", tr(TXT_ENG_CALCULATION_PREPARE_FAILED));
            return 0;
        }
        if (!live->session || !live->topology || strcmp(key, live->topology))
        {
            rebuilt = live->session != NULL || live->discarded;
            live->measures_wires = instrumented;
            if (!load_session(live, bench, executable, result->message, sizeof(result->message)))
            {
                free(key);
                return discard_session(live);
            }
            live->discarded = 0;
        }
        else if (controls_changed(&live->previous, bench) &&
                 !apply_controls(live, bench, result->message, sizeof(result->message)))
        {
            free(key);
            return discard_session(live);
        }
        free(live->topology);
        live->topology = key;
        if (netlist_floating_inputs(bench, live->floating) < 0)
            memset(live->floating, -1, sizeof(live->floating));
        if (!bench_snapshot_clone(&live->previous, bench))
        {
            snprintf(result->message, sizeof(result->message), "%s", tr(TXT_ENG_NO_MEMORY));
            return 0;
        }
    }
    if (!arm_probe(live, bench, &options->live_probe, result->message, sizeof(result->message)) ||
        !protect_sources(live, bench, result->message, sizeof(result->message)) ||
        !advance(live, options->step, span, result->message, sizeof(result->message)) ||
        !read_currents(live, bench, result) || !protect_parts(live, bench, options, result) ||
        !protect_logic(live, bench, result) || !mark_limited_supplies(live, bench, result) ||
        !read_samples(live, result))
        return discard_session(live);
    memcpy(result->burnt, live->burnt, sizeof(result->burnt));
    memcpy(result->burn_reason, live->reason, sizeof(result->burn_reason));
    memcpy(result->burn_value, live->stress, sizeof(result->burn_value));
    for (int i = 0; i < bench->part_count; ++i)
        result->burnt_count += live->burnt[i] != 0;
    result->has_dc = 1;
    for (int i = 0; i < bench->part_count; ++i)
        if ((component_electrical_type(bench->parts[i].type) == PART_SOURCE || bench->parts[i].type == PART_CLOCK || bench->parts[i].type == PART_FUNCTION_GENERATOR) && bench->parts[i].on)
            result->powered = 1;
        else if (mcu_board(bench->parts[i].type) && mcu_board(bench->parts[i].type)->supply_volts > 0)
            result->powered = 1;
    live->restarted = rebuilt;
    TextId status = TXT_ENG_LIVE_TIME;
    if (rebuilt)
        status = TXT_ENG_LIVE_RESTARTED;
    else if (!result->powered)
        status = TXT_ENG_LIVE_TIME_NO_SOURCE;
    snprintf(result->message, sizeof(result->message), tr(status), live->time);
    return 1;
}

int spice_live_voltage(SpiceLive *live, const Workbench *bench, Pin pin, double *voltage,
                       char *message, size_t capacity)
{
    if (!live || !live->session || !voltage)
        return 0;
    int node = netlist_live_node(bench, live->measures_wires, pin);
    if (node < 0)
        return 0;
    if (!node)
    {
        *voltage = 0;
        return 1;
    }
    char vector[48];
    snprintf(vector, sizeof(vector), "%d", node);
    return spice_session_value(live->session, vector, voltage, message, capacity);
}
