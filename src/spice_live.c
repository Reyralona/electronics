#include "spice_live.h"
#include "spice_session.h"
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
    unsigned char burnt[WORKBENCH_MAX_COMPONENTS], reason[WORKBENCH_MAX_COMPONENTS];
    double stress[WORKBENCH_MAX_COMPONENTS];
};

SpiceLive *spice_live_create(void)
{
    return calloc(1, sizeof(SpiceLive));
}

void spice_live_destroy(SpiceLive *live)
{
    if (!live)
        return;
    spice_session_close(live->session);
    bench_snapshot_clear(&live->previous);
    free(live->topology);
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
    NetlistResult netlist = netlist_write_live(file, bench, live->measures_wires);
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
    memset(live->burnt, 0, sizeof(live->burnt));
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
            if (netlist_device_control(part, i, control, name, sizeof(name), &value) &&
                !alter(live, name, value, message, capacity))
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
    unsigned char shorted[WORKBENCH_MAX_COMPONENTS];
    netlist_shorted_sources(bench, shorted);
    for (int i = 0; i < bench->part_count; ++i)
        if (shorted[i] && bench->parts[i].on && !live->burnt[i])
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
            double stress = burn_stress(&bench->parts[i], result->currents[i], &options->limits, &reason, &measured);
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
        char name[48];
        snprintf(name, sizeof(name), bench->parts[worst].type == PART_LED ? "Rguard%d" : "R%d", worst);
        if (!alter(live, name, NETLIST_LIVE_OPEN_OHMS, result->message, sizeof(result->message)))
            return 0;
        live->burnt[worst] = 1;
        if (!advance(live, options->step, options->step, result->message, sizeof(result->message)) ||
            !read_currents(live, bench, result))
            return 0;
    }
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
        if (!bench_snapshot_clone(&live->previous, bench))
        {
            snprintf(result->message, sizeof(result->message), "%s", tr(TXT_ENG_NO_MEMORY));
            return 0;
        }
    }
    if (!protect_sources(live, bench, result->message, sizeof(result->message)) ||
        !advance(live, options->step, span, result->message, sizeof(result->message)) ||
        !read_currents(live, bench, result) || !protect_parts(live, bench, options, result))
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
