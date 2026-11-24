#include "bench_path.h"
#include "workbench.h"
#include "workbench_pack.h"
#include "component_devices.h"
#include "component_registry.h"
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static void layers_clear(BenchPartLayer *layer)
{
    while (layer)
    {
        BenchPartLayer *next = layer->next;
        free(layer->port_names);
        free(layer);
        layer = next;
    }
}

int bench_part_push_layer(Part *part)
{
    BenchPartLayer *layer = calloc(1, sizeof(*layer));
    if (!layer)
        return 0;
    layer->group = part->group;
    memcpy(layer->exposed, part->exposed, sizeof(layer->exposed));
    layer->port_names = part->port_names;
    layer->next = part->layers;
    part->layers = layer;
    part->port_names = layer->port_names ? _strdup(layer->port_names) : NULL;
    if (layer->port_names && !part->port_names)
    {
        part->port_names = layer->port_names;
        part->layers = layer->next;
        free(layer);
        return 0;
    }
    memset(part->exposed, 0, sizeof(part->exposed));
    return 1;
}

void bench_part_pop_layer(Part *part)
{
    BenchPartLayer *layer = part->layers;
    if (!layer)
    {
        part->group = 0;
        memset(part->exposed, 0, sizeof(part->exposed));
        return;
    }
    free(part->port_names);
    part->port_names = layer->port_names;
    part->group = layer->group;
    memcpy(part->exposed, layer->exposed, sizeof(part->exposed));
    part->layers = layer->next;
    free(layer);
}

void bench_groups_prune(Workbench *bench)
{
    unsigned char used[WORKBENCH_MAX_COMPONENTS] = {0};
    for (int i = 0; i < bench->part_count; ++i)
    {
        if (bench->parts[i].group)
            used[bench->parts[i].group - 1] = 1;
        for (const BenchPartLayer *layer = bench->parts[i].layers; layer; layer = layer->next)
            if (layer->group)
                used[layer->group - 1] = 1;
    }
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        if (!used[g])
            memset(&bench->groups[g], 0, sizeof(bench->groups[g]));
}

void bench_part_clear(Part *part)
{
    if (!part)
        return;
    if (part->terminals)
    {
        for (size_t pin = 0; pin < part->terminals->pin_count; ++pin)
            bench_path_clear(&part->terminals->paths[pin]);
        free(part->terminals->paths);
        free(part->terminals);
    }
    free(part->code);
    free(part->port_names);
    layers_clear(part->layers);
    memset(part, 0, sizeof(*part));
}

int bench_part_clone(Part *destination, const Part *source)
{
    if (!destination || !source)
        return 0;
    if (destination == source)
        return 1;
    Part copy = *source;
    copy.terminals = NULL;
    copy.code = NULL;
    copy.port_names = NULL;
    copy.layers = NULL;
    if (source->code && !(copy.code = _strdup(source->code)))
        return 0;
    if (source->port_names && !(copy.port_names = _strdup(source->port_names)))
    {
        free(copy.code);
        return 0;
    }
    if (source->terminals)
    {
        copy.terminals = calloc(1, sizeof(*copy.terminals));
        if (!copy.terminals)
        {
            free(copy.code);
            free(copy.port_names);
            return 0;
        }
        copy.terminals->pin_count = source->terminals->pin_count;
        copy.terminals->paths = calloc(copy.terminals->pin_count, sizeof(BenchPath));
        if (!copy.terminals->paths)
        {
            free(copy.terminals);
            free(copy.code);
            free(copy.port_names);
            return 0;
        }
        for (size_t i = 0; i < copy.terminals->pin_count; ++i)
            if (bench_path_clone(&copy.terminals->paths[i], &source->terminals->paths[i]) != BENCH_PATH_OK)
            {
                bench_part_clear(&copy);
                return 0;
            }
    }
    BenchPartLayer **tail = &copy.layers;
    for (const BenchPartLayer *layer = source->layers; layer; layer = layer->next)
    {
        *tail = calloc(1, sizeof(**tail));
        if (!*tail)
        {
            bench_part_clear(&copy);
            return 0;
        }
        **tail = *layer;
        (*tail)->next = NULL;
        (*tail)->port_names = layer->port_names ? _strdup(layer->port_names) : NULL;
        if (layer->port_names && !(*tail)->port_names)
        {
            bench_part_clear(&copy);
            return 0;
        }
        tail = &(*tail)->next;
    }
    bench_part_clear(destination);
    *destination = copy;
    return 1;
}

void bench_part_move(Part *destination, Part *source)
{
    if (!destination || !source || destination == source)
        return;
    bench_part_clear(destination);
    *destination = *source;
    memset(source, 0, sizeof(*source));
}

int bench_snapshot_clone(Workbench *destination, const Workbench *source)
{
    if (!destination || !source || source->part_count < 0 || source->part_count > WORKBENCH_MAX_COMPONENTS)
        return 0;
    if (destination == source)
        return 1;
    Workbench copy = *source;
    memset(copy.parts, 0, sizeof(copy.parts));
    for (int i = 0; i < source->part_count; ++i)
        if (!bench_part_clone(&copy.parts[i], &source->parts[i]))
        {
            bench_snapshot_clear(&copy);
            return 0;
        }
    bench_snapshot_move(destination, &copy);
    return 1;
}

void bench_snapshot_clear(Workbench *snapshot)
{
    if (!snapshot)
        return;
    for (int i = 0; i < snapshot->part_count && i < WORKBENCH_MAX_COMPONENTS; ++i)
        bench_part_clear(&snapshot->parts[i]);
    memset(snapshot, 0, sizeof(*snapshot));
}

void bench_snapshot_move(Workbench *destination, Workbench *source)
{
    if (!destination || !source || destination == source)
        return;
    bench_snapshot_clear(destination);
    *destination = *source;
    memset(source, 0, sizeof(*source));
}

int bench_replace(Workbench *bench, const Workbench *replacement)
{
    if (!bench || !replacement)
        return 0;
    uint64_t revision = bench->revision + 1;
    if (!bench_snapshot_clone(bench, replacement))
        return 0;
    bench->revision = revision;
    return 1;
}

int bench_set_control(Workbench *bench, int component, double position)
{
    if (!bench || component < 0 || component >= bench->part_count ||
        (bench->parts[component].type != PART_POTENTIOMETER && !component_environment(bench->parts[component].type)) || !isfinite(position) || position < 0 || position > 1)
        return 0;
    if (bench->parts[component].control != position)
    {
        bench->parts[component].control = position;
        ++bench->revision;
    }
    return 1;
}

float bench_part_scale(const Part *part)
{
    return part->scale == 0 ? 1 : part->scale;
}

static int keep_contact_tips(Workbench *bench, int component)
{
    Part *part = &bench->parts[component];
    if (part->terminals || !bench_lead_count(part, 0))
        return 1;
    WirePoint points[WORKBENCH_MAX_PINS];
    unsigned char fixed[WORKBENCH_MAX_PINS] = {0};
    int count = bench_pin_count(part->type), found = 0;
    for (int pin = 0; pin < count; ++pin)
    {
        Pin contact;
        if (bench_find_contact(bench, component, pin, &contact))
        {
            bench_pin_position(part, pin, &points[pin].x, &points[pin].y);
            fixed[pin] = 1;
            found = 1;
        }
    }
    if (!found)
        return 1;
    Part before = {0};
    if (!bench_part_clone(&before, part))
        return 0;
    for (int pin = 0; pin < count; ++pin)
        if (fixed[pin] && !bench_lead_move(bench, component, pin, 0, points[pin]))
        {
            bench_part_move(part, &before);
            ++bench->revision;
            return 0;
        }
    bench_part_clear(&before);
    return 1;
}

int bench_set_scale(Workbench *bench, int component, float scale)
{
    if (!bench || component < 0 || component >= bench->part_count || !isfinite(scale) ||
        scale < PART_SCALE_MIN || scale > PART_SCALE_MAX)
        return 0;
    if (bench_part_scale(&bench->parts[component]) != scale)
    {
        if (!keep_contact_tips(bench, component))
            return 0;
        Part *part = &bench->parts[component];
        float ratio = bench_part_scale(part) / scale;
        if (part->terminals)
            for (size_t pin = 0; pin < part->terminals->pin_count; ++pin)
                for (size_t point = 0; point < part->terminals->paths[pin].count; ++point)
                {
                    part->terminals->paths[pin].points[point].x *= ratio;
                    part->terminals->paths[pin].points[point].y *= ratio;
                }
        bench->parts[component].scale = scale;
        ++bench->revision;
    }
    return 1;
}

int bench_move_part(Workbench *bench, int component, float x, float y)
{
    if (!bench || component < 0 || component >= bench->part_count || !isfinite(x) || !isfinite(y))
        return 0;
    Part *part = &bench->parts[component];
    if (part->x != x || part->y != y)
    {
        part->x = x;
        part->y = y;
        ++bench->revision;
    }
    return 1;
}

int bench_set_on(Workbench *bench, int component, int on)
{
    if (!bench || component < 0 || component >= bench->part_count || (on != 0 && on != 1))
        return 0;
    if (bench->parts[component].on != on)
    {
        bench->parts[component].on = on;
        ++bench->revision;
    }
    return 1;
}

int bench_set_states(Workbench *bench, int component, unsigned states)
{
    if (!bench || component < 0 || component >= bench->part_count ||
        !component_states_valid(bench->parts[component].type, states))
        return 0;
    if (bench->parts[component].states != states)
    {
        bench->parts[component].states = states;
        ++bench->revision;
    }
    return 1;
}

static int wire_valid(const Workbench *bench, int wire)
{
    return bench && wire >= 0 && wire < bench->wire_count;
}

int bench_color_from_legacy_wire(int legacy)
{
    static const int colors[BENCH_LEGACY_WIRE_COLORS] = {BENCH_COLOR_BLUE,  BENCH_COLOR_RED,    BENCH_COLOR_BLACK,
                                                         BENCH_COLOR_GREEN, BENCH_COLOR_YELLOW, BENCH_COLOR_WHITE};
    if (legacy < 0 || legacy >= BENCH_LEGACY_WIRE_COLORS)
        return -1;
    return colors[legacy];
}

int bench_set_wire_color(Workbench *bench, int wire, int color)
{
    if (!wire_valid(bench, wire) || color < 0 || color >= WIRE_COLOR_COUNT)
        return 0;
    if (bench->wires[wire].color != color)
    {
        bench->wires[wire].color = color;
        ++bench->revision;
    }
    return 1;
}

int bench_set_wire_route(Workbench *bench, int wire, const WirePoint *points, int count)
{
    int i, changed;
    if (!wire_valid(bench, wire) || count < 0 || count > WIRE_MAX_KNEES || (count && !points))
        return 0;
    changed = count != bench->wires[wire].knee_count;
    for (i = 0; i < count; ++i)
    {
        if (!isfinite(points[i].x) || !isfinite(points[i].y))
            return 0;
        changed |= points[i].x != bench->wires[wire].knees[i].x || points[i].y != bench->wires[wire].knees[i].y;
    }
    if (changed)
    {
        if (count)
            memmove(bench->wires[wire].knees, points, (size_t)count * sizeof(*points));
        bench->wires[wire].knee_count = count;
        ++bench->revision;
    }
    return 1;
}

int bench_move_knee(Workbench *bench, int wire, int knee, WirePoint point)
{
    if (!wire_valid(bench, wire) || knee < 0 || knee >= bench->wires[wire].knee_count || !isfinite(point.x) ||
        !isfinite(point.y))
        return 0;
    WirePoint *old = &bench->wires[wire].knees[knee];
    if (old->x != point.x || old->y != point.y)
    {
        *old = point;
        ++bench->revision;
    }
    return 1;
}

int bench_remove_knee(Workbench *bench, int wire, int knee)
{
    if (!wire_valid(bench, wire) || knee < 0 || knee >= bench->wires[wire].knee_count)
        return 0;
    Wire *route = &bench->wires[wire];
    memmove(&route->knees[knee], &route->knees[knee + 1], (size_t)(route->knee_count - knee - 1) * sizeof(WirePoint));
    --route->knee_count;
    ++bench->revision;
    return 1;
}

int bench_remove_wire(Workbench *bench, int wire)
{
    if (!wire_valid(bench, wire))
        return 0;
    memmove(&bench->wires[wire], &bench->wires[wire + 1], (size_t)(bench->wire_count - wire - 1) * sizeof(Wire));
    --bench->wire_count;
    ++bench->revision;
    return 1;
}

int bench_parse_value(const char *text, double *value)
{
    char buffer[64], *end;
    size_t i, n;
    double parsed;
    if (!text || !value)
        return 0;
    n = strlen(text);
    if (!n || n >= sizeof(buffer))
        return 0;
    for (i = 0; i < n; ++i)
    {
        char c = text[i];
        if (c == ',')
            c = '.';
        if (!((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-'))
            return 0;
        buffer[i] = c;
    }
    buffer[n] = '\0';
    errno = 0;
    parsed = strtod(buffer, &end);
    if (errno == ERANGE || end == buffer || *end || !isfinite(parsed) || parsed < 1e-12 || parsed > 1e12)
        return 0;
    *value = parsed;
    return 1;
}

#define WIRE_REVERSAL_COSINE -.85f
#define WIRE_STUB_LENGTH 14.0f
static int wire_path_indexed(const Wire *wire, WirePoint start, WirePoint end, float radius,
                             WirePoint output[WIRE_PATH_CAPACITY], int *slots)
{
    int i, count = 0;
    WirePoint kept = start; /* last point actually drawn; pruned knees do not count */
    output[count++] = start;
    for (i = 0; i < wire->knee_count && i < WIRE_MAX_KNEES; ++i)
    {
        WirePoint previous = kept;
        WirePoint corner = wire->knees[i];
        WirePoint next = i + 1 < wire->knee_count ? wire->knees[i + 1] : end;
        float before = hypotf(previous.x - corner.x, previous.y - corner.y);
        float after = hypotf(next.x - corner.x, next.y - corner.y);
        /* A knee that doubles back over a short stub only draws a spike: skip it. */
        float turn = before > .001f && after > .001f
                         ? ((corner.x - previous.x) * (next.x - corner.x) + (corner.y - previous.y) * (next.y - corner.y)) /
                               (before * after)
                         : 0;
        if (turn < WIRE_REVERSAL_COSINE && fminf(before, after) < WIRE_STUB_LENGTH)
            continue;
        kept = corner;
        float r = fminf(radius, fminf(before * .45f, after * .45f));
        if (r <= 0 || before < .001f || after < .001f)
        {
            if (slots)
                slots[count] = i;
            output[count++] = corner;
        }
        else
        {
            int step;
            WirePoint a = {corner.x + (previous.x - corner.x) * r / before,
                           corner.y + (previous.y - corner.y) * r / before};
            WirePoint b = {corner.x + (next.x - corner.x) * r / after, corner.y + (next.y - corner.y) * r / after};
            for (step = 0; step <= 8; ++step)
            {
                float t = step / 8.0f, u = 1 - t;
                if (slots)
                    slots[count] = i + (step > 4);
                output[count++] = (WirePoint){u * u * a.x + 2 * u * t * corner.x + t * t * b.x,
                                              u * u * a.y + 2 * u * t * corner.y + t * t * b.y};
            }
        }
    }
    if (slots)
        slots[count] = wire->knee_count;
    output[count++] = end;
    return count;
}

int wire_path(const Wire *wire, WirePoint start, WirePoint end, float radius, WirePoint output[WIRE_PATH_CAPACITY])
{
    return wire_path_indexed(wire, start, end, radius, output, NULL);
}

int wire_pick_segment(const Wire *wire, WirePoint start, WirePoint end, float radius,
                      WirePoint point, float tolerance, int *slot, WirePoint *projected)
{
    if (!wire || !slot || !projected || wire->knee_count < 0 || wire->knee_count > WIRE_MAX_KNEES ||
        !isfinite(point.x) || !isfinite(point.y) || !isfinite(tolerance) || tolerance < 0 ||
        !isfinite(start.x) || !isfinite(start.y) || !isfinite(end.x) || !isfinite(end.y) ||
        !isfinite(radius) || radius < 0)
        return 0;
    for (int i = 0; i < wire->knee_count; ++i)
        if (!isfinite(wire->knees[i].x) || !isfinite(wire->knees[i].y))
            return 0;
    WirePoint path[WIRE_PATH_CAPACITY];
    int slots[WIRE_PATH_CAPACITY];
    int count = wire_path_indexed(wire, start, end, radius, path, slots);
    BenchPathHit hit;
    if (!bench_polyline_pick(path, (size_t)count, point, tolerance, &hit))
        return 0;
    *slot = slots[hit.segment + 1];
    *projected = hit.point;
    return 1;
}

int bench_split_wire(Workbench *bench, int wire, int slot, WirePoint point)
{
    if (!wire_valid(bench, wire) || slot < 0 || slot > bench->wires[wire].knee_count ||
        bench->wire_count >= WORKBENCH_MAX_WIRES || bench->part_count >= WORKBENCH_MAX_COMPONENTS ||
        !isfinite(point.x) || !isfinite(point.y))
        return -1;
    Wire original = bench->wires[wire];
    int junction = bench_add(bench, PART_JUNCTION, point.x, point.y);
    if (junction < 0)
        return -1;
    Wire first = {.a = original.a, .b = {junction, 0}, .color = original.color, .knee_count = slot};
    Wire second = {.a = {junction, 0}, .b = original.b, .color = original.color,
                   .knee_count = original.knee_count - slot};
    memcpy(first.knees, original.knees, (size_t)slot * sizeof(WirePoint));
    memcpy(second.knees, original.knees + slot, (size_t)second.knee_count * sizeof(WirePoint));
    bench->wires[wire] = first;
    bench->wires[bench->wire_count++] = second;
    ++bench->revision;
    return junction;
}

static int junction_wired(const Workbench *bench, int part)
{
    for (int i = 0; i < bench->wire_count; ++i)
        if (bench->wires[i].a.component == part || bench->wires[i].b.component == part)
            return 1;
    return 0;
}

int bench_prune_junctions(Workbench *bench)
{
    int removed = 0;
    for (int i = bench->part_count - 1; i >= 0; --i)
        if (bench->parts[i].type == PART_JUNCTION && !bench->parts[i].port_names && !junction_wired(bench, i))
        {
            bench_remove(bench, i);
            ++removed;
        }
    return removed;
}

int bench_insert_knee(Workbench *bench, int wire, int slot, WirePoint point)
{
    if (!wire_valid(bench, wire) || slot < 0 || slot > bench->wires[wire].knee_count ||
        bench->wires[wire].knee_count >= WIRE_MAX_KNEES || !isfinite(point.x) || !isfinite(point.y))
        return 0;
    Wire *route = &bench->wires[wire];
    memmove(&route->knees[slot + 1], &route->knees[slot], (size_t)(route->knee_count - slot) * sizeof(WirePoint));
    route->knees[slot] = point;
    ++route->knee_count;
    ++bench->revision;
    return 1;
}

int bench_pin_count(PartType type)
{
    const ComponentInfo *info = component_info(type);
    return info ? info->pin_count : 0;
}

int bench_board_group_first(int pin)
{
    if (pin < 0 || pin >= BREADBOARD_WIDE_PINS)
        return -1;
    if (pin >= 720)
        return 300 + ((pin - 720) / 30) * 30;
    if (pin >= 420)
        return 420 + ((pin - 420) / 5) * 5;
    return pin < 300 ? (pin / 5) * 5 : 300 + ((pin - 300) / 30) * 30;
}

WirePoint bench_part_to_world(const Part *part, WirePoint local)
{
    float scale = bench_part_scale(part);
    local.x *= scale;
    local.y *= scale;
    if (part->mirrored && !bench_is_board(part->type))
        local.x = -local.x;
    WirePoint rotated = local;
    int rotation = bench_is_board(part->type) ? 0 : part->rotation;
    switch (rotation)
    {
    case 1:
        rotated = (WirePoint){-local.y, local.x};
        break;
    case 2:
        rotated = (WirePoint){-local.x, -local.y};
        break;
    case 3:
        rotated = (WirePoint){local.y, -local.x};
        break;
    }
    if (part->rotation_diagonal && !bench_is_board(part->type))
    {
        const float diagonal = 0.7071067811865476f;
        rotated = (WirePoint){(rotated.x - rotated.y) * diagonal,
                              (rotated.x + rotated.y) * diagonal};
    }
    return (WirePoint){part->x + rotated.x, part->y + rotated.y};
}

WirePoint bench_part_to_local(const Part *part, WirePoint world)
{
    float scale = bench_part_scale(part);
    WirePoint offset = {(world.x - part->x) / scale, (world.y - part->y) / scale};
    if (part->rotation_diagonal && !bench_is_board(part->type))
    {
        const float diagonal = 0.7071067811865476f;
        offset = (WirePoint){(offset.x + offset.y) * diagonal,
                             (offset.y - offset.x) * diagonal};
    }
    int rotation = bench_is_board(part->type) ? 0 : part->rotation;
    WirePoint local = offset;
    switch (rotation)
    {
    case 1:
        local = (WirePoint){offset.y, -offset.x};
        break;
    case 2:
        local = (WirePoint){-offset.x, -offset.y};
        break;
    case 3:
        local = (WirePoint){-offset.y, offset.x};
        break;
    }
    if (part->mirrored && !bench_is_board(part->type))
        local.x = -local.x;
    return local;
}

static void pin_local_position(const Part *p, int pin, float *x, float *y)
{
    const ComponentDevice *device = component_device(p->type);
    if (device)
    {
        *x = device->pins[pin][0];
        *y = device->pins[pin][1];
        return;
    }
    if (bench_is_board(p->type))
    {
        if (p->type == PART_BREADBOARD_WIDE && pin >= BREADBOARD_PINS)
        {
            int extra = pin - BREADBOARD_PINS;
            if (extra < 300)
            {
                int column = (extra / 5) % 30;
                *x = 7 + column * 14.0f;
                *y = (extra < 150 ? -55.0f : 15.0f) + (extra % 5) * 10.0f;
            }
            else
            {
                int rail = (extra - 300) / 30;
                *x = 7 + ((extra - 300) % 30) * 14.0f;
                *y = rail == 0 ? -98 : rail == 1 ? -82 : rail == 2 ? 78 : 94;
            }
            return;
        }
        int group = pin / 5, row = pin % 5;
        if (group < 20)
        {
            *x = -63.0f + (group % 10) * 14.0f;
            *y = (p->board_layout ? (group < 10 ? -55.0f : 15.0f) : (group < 10 ? -39.0f : 11.0f)) +
                 row * (p->board_layout ? 10.0f : 7.0f);
        }
        else if (group < 36)
        {
            int extra = (group - 20) % 8;
            *x = (extra < 4 ? -119.0f + extra * 14.0f : 77.0f + (extra - 4) * 14.0f);
            *y = (p->board_layout ? (group < 28 ? -55.0f : 15.0f) : (group < 28 ? -39.0f : 11.0f)) +
                 row * (p->board_layout ? 10.0f : 7.0f);
        }
        else if (pin < 300)
        {
            int extra = (group - 36) % 12;
            *x = (extra < 6 ? -203.0f + extra * 14.0f : 133.0f + (extra - 6) * 14.0f);
            *y = (p->board_layout ? (group < 48 ? -55.0f : 15.0f) : (group < 48 ? -39.0f : 11.0f)) +
                 row * (p->board_layout ? 10.0f : 7.0f);
        }
        else
        {
            int rail = (pin - 300) / 30, column = (pin - 300) % 30;
            *x = -203.0f + column * 14.0f;
            *y = (rail == 0 ? -98.0f : rail == 1 ? -82.0f : rail == 2 ? 78.0f : 94.0f);
        }
        if (p->type == PART_BREADBOARD_WIDE)
            *x -= 210;
    }
    else if (component_radial_pins(p->type))
    {
        *x = bench_pin_count(p->type) == 3 ? (pin - 1) * 20.0f : (pin ? 10.0f : -10.0f);
        *y = p->type == PART_SOIL ? -58 : 49;
    }
    else if (p->type == PART_BATTERY_9V)
    {
        *x = pin ? -13 : 13;
        *y = -58;
    }
    else if (p->type == PART_BATTERY_AA)
    {
        *x = 0;
        *y = pin ? 58 : -58;
    }
    else if (p->type == PART_POTENTIOMETER)
    {
        *x = pin == 0 ? -49 : pin == 2 ? 49 : 0;
        *y = pin == 1 ? 49 : 0;
    }
    else if (p->type == PART_JUNCTION)
    {
        *x = 0;
        *y = 0;
    }
    else if (component_has_flag(p->type, COMPONENT_DIP))
    {
        /* Pin 1 at the left of the top row, counted around the package like a real DIP. */
        int count = bench_pin_count(p->type), side = count / 2;
        *x = (float)((pin < side ? pin : count - 1 - pin) * BENCH_DIP_PITCH - (side - 1) * BENCH_DIP_PITCH / 2);
        *y = pin < side ? -15.0f : 15.0f;
    }
    else if ((p->type >= PART_AND && p->type <= PART_XOR) || p->type == PART_DFF ||
             p->type == PART_NOR || p->type == PART_XNOR)
    {
        *x = pin == 2 ? 49.0f : -49.0f;
        *y = 0;
        if (pin == 0)
            *y = -15;
        else if (pin == 1)
            *y = 15;
    }
    else
    {
        *x = pin ? 49.0f : -49.0f;
        *y = 0;
    }
}

/* Multi-row packages and instruments keep fixed terminals, like ICs. */
static int rigid_device(PartType type)
{
    const ComponentDevice *device = component_device(type);
    return device && bench_pin_count(type) > 3 && type != PART_RGB_LED && type != PART_DIP_SWITCH;
}

static int lead_editable(const Part *part, int pin)
{
    return part && pin >= 0 && pin < bench_pin_count(part->type) && !bench_is_board(part->type) &&
           !component_has_flag(part->type, COMPONENT_GATE_SYMBOL) && !rigid_device(part->type);
}

size_t bench_lead_count(const Part *part, int pin)
{
    if (!lead_editable(part, pin))
        return 0;
    if (part->terminals && part->terminals->paths[pin].count)
        return part->terminals->paths[pin].count;
    return 1;
}

WirePoint bench_lead_point(const Part *part, int pin, size_t point)
{
    WirePoint local = {0};
    if (!part || pin < 0 || pin >= bench_pin_count(part->type))
        return local;
    if (part->terminals && part->terminals->paths[pin].count > point)
        local = part->terminals->paths[pin].points[point];
    else
        pin_local_position(part, pin, &local.x, &local.y);
    return bench_part_to_world(part, local);
}

WirePoint bench_lead_anchor(const Part *part, int pin)
{
    WirePoint local;
    pin_local_position(part, pin, &local.x, &local.y);
    local.x *= .38f;
    local.y *= .38f;
    if (component_radial_pins(part->type))
        local.y = part->type == PART_SOIL ? -30 : 8;
    if (part->type == PART_LED)
    {
        const float lead_x = 10, lead_y = 8;
        float x = pin ? lead_x : -lead_x, scale = bench_part_scale(part);
        if (part->mirrored)
            x = -x;
        return (WirePoint){part->x + x * scale, part->y + lead_y * scale};
    }
    return bench_part_to_world(part, local);
}

void bench_lead_walk(const Part *part, int pin, float radius, BenchLeadVertex vertex, void *context)
{
    size_t count = bench_lead_count(part, pin);
    if (!count || !vertex)
        return;
    vertex(context, bench_lead_anchor(part, pin), 0);
    for (size_t corner = 0; corner + 1 < count; ++corner)
    {
        Wire bend = {0};
        bend.knee_count = 1;
        bend.knees[0] = bench_lead_point(part, pin, corner);
        WirePoint previous = corner ? bench_lead_point(part, pin, corner - 1) : bench_lead_anchor(part, pin);
        WirePoint next = bench_lead_point(part, pin, corner + 1);
        WirePoint curve[WIRE_PATH_CAPACITY];
        int vertices = wire_path(&bend, previous, next, radius, curve);
        for (int i = 1; i + 1 < vertices; ++i)
            vertex(context, curve[i], corner + (vertices > 3 && i > 5));
    }
    vertex(context, bench_lead_point(part, pin, count - 1), count - 1);
}

typedef struct
{
    WirePoint previous, query, projected;
    float tolerance;
    size_t slot;
    double distance;
    int started, found;
} LeadPick;

static void pick_lead_vertex(void *context, WirePoint point, size_t slot)
{
    LeadPick *pick = context;
    if (pick->started)
    {
        WirePoint segment[] = {pick->previous, point};
        BenchPathHit hit;
        if (bench_polyline_pick(segment, 2, pick->query, pick->tolerance, &hit) &&
            (!pick->found || hit.distance_squared < pick->distance))
        {
            pick->found = 1;
            pick->distance = hit.distance_squared;
            pick->projected = hit.point;
            pick->slot = slot;
        }
    }
    pick->previous = point;
    pick->started = 1;
}

int bench_lead_pick(const Part *part, int pin, float radius, WirePoint point, float tolerance,
                    size_t *slot, WirePoint *projected)
{
    if (!slot || !projected)
        return 0;
    LeadPick pick = {.query = point, .tolerance = tolerance};
    bench_lead_walk(part, pin, radius, pick_lead_vertex, &pick);
    if (!pick.found)
        return 0;
    *slot = pick.slot;
    *projected = pick.projected;
    return 1;
}

static int edit_lead(Workbench *bench, int component, int pin, size_t point, WirePoint world, int operation)
{
    const float coordinate_limit = 100000;
    if (!bench || component < 0 || component >= bench->part_count ||
        !lead_editable(&bench->parts[component], pin) || !isfinite(world.x) || !isfinite(world.y))
        return 0;
    Part *part = &bench->parts[component];
    WirePoint local = bench_part_to_local(part, world);
    if (fabsf(local.x) > coordinate_limit || fabsf(local.y) > coordinate_limit)
        return 0;
    BenchPath path = {0};
    if (part->terminals && part->terminals->paths[pin].count)
    {
        if (bench_path_clone(&path, &part->terminals->paths[pin]) != BENCH_PATH_OK)
            return 0;
    }
    else
    {
        WirePoint tip;
        pin_local_position(part, pin, &tip.x, &tip.y);
        if (bench_path_insert(&path, 0, tip) != BENCH_PATH_OK)
            return 0;
    }
    BenchPathStatus status = BENCH_PATH_RANGE;
    if (operation == 0)
        status = bench_path_move(&path, point, local);
    else if (operation == 1 && point < path.count)
        status = bench_path_insert(&path, point, local);
    else if (operation == 2 && point + 1 < path.count)
        status = bench_path_remove(&path, point);
    if (status != BENCH_PATH_OK)
    {
        bench_path_clear(&path);
        return 0;
    }
    if (!part->terminals)
    {
        struct BenchTerminals *geometry = calloc(1, sizeof(*geometry));
        if (geometry)
        {
            geometry->pin_count = (size_t)bench_pin_count(part->type);
            geometry->paths = calloc(geometry->pin_count, sizeof(BenchPath));
        }
        if (!geometry || !geometry->paths)
        {
            free(geometry);
            bench_path_clear(&path);
            return 0;
        }
        part->terminals = geometry;
    }
    bench_path_clear(&part->terminals->paths[pin]);
    part->terminals->paths[pin] = path;
    ++bench->revision;
    return 1;
}

int bench_lead_move(Workbench *bench, int component, int pin, size_t point, WirePoint world)
{
    return edit_lead(bench, component, pin, point, world, 0);
}

int bench_lead_insert(Workbench *bench, int component, int pin, size_t before, WirePoint world)
{
    return edit_lead(bench, component, pin, before, world, 1);
}

int bench_lead_remove(Workbench *bench, int component, int pin, size_t point)
{
    return edit_lead(bench, component, pin, point, (WirePoint){0}, 2);
}

void bench_pin_position(const Part *part, int pin, float *x, float *y)
{
    WirePoint local;
    pin_local_position(part, pin, &local.x, &local.y);
    if (part->terminals && (size_t)pin < part->terminals->pin_count && part->terminals->paths[pin].count)
    {
        const BenchPath *path = &part->terminals->paths[pin];
        local = path->points[path->count - 1];
    }
    WirePoint world = bench_part_to_world(part, local);
    *x = world.x;
    *y = world.y;
}

/* Internal contacts use the original circuit. Cross-package contacts use only visible ports. */
static int contact_positions(const Workbench *bench, Pin a, Pin b, WirePoint *pa, WirePoint *pb)
{
    const Part *first = &bench->parts[a.component];
    const Part *second = &bench->parts[b.component];
    if (first->group == second->group)
    {
        const BenchPartLayer *left = first->layers;
        const BenchPartLayer *right = second->layers;
        while (left || right)
        {
            int left_group = left ? left->group : 0;
            int right_group = right ? right->group : 0;
            /* Cross-package contacts were frozen as wires when the parent was created.
             * Hidden leaf positions must never connect independent child instances. */
            if (left_group != right_group)
                return 0;
            if (!left_group)
                break;
            left = left->next;
            right = right->next;
        }
        bench_pin_position(first, a.pin, &pa->x, &pa->y);
        bench_pin_position(second, b.pin, &pb->x, &pb->y);
        return 1;
    }
    if ((first->group && !bench_pin_exposed(first, a.pin)) ||
        (second->group && !bench_pin_exposed(second, b.pin)))
        return 0;
    bench_terminal_position(bench, a, &pa->x, &pa->y);
    bench_terminal_position(bench, b, &pb->x, &pb->y);
    return 1;
}

int bench_find_contact_after(const Workbench *b, int component, int pin, int after, Pin *contact)
{
    const float contact_tolerance = .01f;
    if (!b || !contact || component < 0 || component >= b->part_count ||
        pin < 0 || pin >= bench_pin_count(b->parts[component].type))
        return 0;
    for (int other = 0; other < b->part_count; ++other)
    {
        if (other == component)
            continue;
        for (int terminal = 0; terminal < bench_pin_count(b->parts[other].type); ++terminal)
        {
            if (other * WORKBENCH_MAX_PINS + terminal <= after)
                continue;
            WirePoint a, c;
            Pin candidate = {other, terminal};
            if (contact_positions(b, (Pin){component, pin}, candidate, &a, &c) &&
                fabsf(a.x - c.x) < contact_tolerance && fabsf(a.y - c.y) < contact_tolerance)
            {
                *contact = candidate;
                return 1;
            }
        }
    }
    return 0;
}

int bench_find_contact(const Workbench *bench, int component, int pin, Pin *contact)
{
    return bench_find_contact_after(bench, component, pin, -1, contact);
}

int bench_breadboard_snap_position(const Workbench *b, const Part *part, float *snapped_x, float *snapped_y)
{
    int board, hole, pin, found = 0;
    float best = 64.0f, dx = 0, dy = 0;
    if (!b || !part || !snapped_x || !snapped_y || bench_is_board(part->type))
        return 0;
    for (pin = 0; pin < bench_pin_count(part->type); ++pin)
    {
        float x, y;
        bench_pin_position(part, pin, &x, &y);
        for (board = 0; board < b->part_count; ++board)
            if (bench_is_board(b->parts[board].type) && b->parts[board].group == part->group)
            {
                if (!part->terminals && lead_editable(part, pin))
                {
                    Part footprint = *part;
                    footprint.scale = bench_part_scale(&b->parts[board]);
                    bench_pin_position(&footprint, pin, &x, &y);
                }
                for (hole = 0; hole < bench_pin_count(b->parts[board].type); ++hole)
                {
                    float hx, hy, distance;
                    bench_pin_position(&b->parts[board], hole, &hx, &hy);
                    distance = (hx - x) * (hx - x) + (hy - y) * (hy - y);
                    if (distance < best)
                    {
                        best = distance;
                        dx = hx - x;
                        dy = hy - y;
                        found = 1;
                    }
                }
            }
    }
    if (found)
    {
        *snapped_x = part->x + dx;
        *snapped_y = part->y + dy;
    }
    return found;
}

int bench_snap_to_breadboard(Workbench *b, int component)
{
    if (!b || component < 0 || component >= b->part_count)
        return 0;
    float x, y;
    if (!bench_breadboard_snap_position(b, &b->parts[component], &x, &y))
        return 0;
    bench_move_part(b, component, x, y);
    Part footprint = b->parts[component];
    for (int board = 0; board < b->part_count; ++board)
    {
        if (!bench_is_board(b->parts[board].type) || b->parts[board].group != footprint.group)
            continue;
        if (!footprint.terminals)
            footprint.scale = bench_part_scale(&b->parts[board]);
        for (int pin = 0; pin < bench_pin_count(footprint.type); ++pin)
        {
            if (!lead_editable(&footprint, pin))
                continue;
            Pin contact;
            if (bench_find_contact(b, component, pin, &contact))
                continue;
            WirePoint point;
            if (footprint.terminals)
                bench_pin_position(&b->parts[component], pin, &point.x, &point.y);
            else
                bench_pin_position(&footprint, pin, &point.x, &point.y);
            float best = 64;
            WirePoint target = point;
            int found = 0;
            for (int hole = 0; hole < bench_pin_count(b->parts[board].type); ++hole)
            {
                WirePoint candidate;
                bench_pin_position(&b->parts[board], hole, &candidate.x, &candidate.y);
                float dx = candidate.x - point.x, dy = candidate.y - point.y;
                float distance = dx * dx + dy * dy;
                if (distance < best)
                {
                    best = distance;
                    target = candidate;
                    found = 1;
                }
            }
            if (found)
            {
                WirePoint current;
                bench_pin_position(&b->parts[component], pin, &current.x, &current.y);
                if (hypotf(current.x - target.x, current.y - target.y) > .001f &&
                    !bench_lead_move(b, component, pin, 0, target))
                    return 0;
            }
        }
    }
    return 1;
}

static int pin_valid(const Workbench *b, Pin p)
{
    return p.component >= 0 && p.component < b->part_count && p.pin >= 0 &&
           p.pin < bench_pin_count(b->parts[p.component].type);
}

static int same_pin(Pin a, Pin b)
{
    return a.component == b.component && a.pin == b.pin;
}

int bench_add(Workbench *b, PartType type, float x, float y)
{
    int id;
    const ComponentInfo *info = component_info(type);
    if (!b || !info || b->part_count >= WORKBENCH_MAX_COMPONENTS)
        return -1;
    id = b->part_count++;
    b->parts[id] = (Part){.type = type,
                          .x = x,
                          .y = y,
                          .on = component_has_flag(type, COMPONENT_STARTS_ON),
                          .control = component_default_control(type),
                          .value = info->primary ? info->primary->initial : 0,
                          .board_layout = bench_is_board(type)};
    ++b->revision;
    return id;
}

/* Replaces a placed part by another type in the same spot. Wires on terminals that the new type
 * still has stay connected by index; wires on terminals it lacks are removed. */
int bench_swap_type(Workbench *b, int component, PartType type)
{
    const ComponentInfo *info = component_info(type);
    if (!b || !info || component < 0 || component >= b->part_count || bench_is_board(type))
        return 0;
    Part *part = &b->parts[component];
    if (part->group || bench_is_board(part->type) || part->type == type)
        return 0;
    int pins = bench_pin_count(type);
    for (int i = b->wire_count - 1; i >= 0; --i)
    {
        const Wire *wire = &b->wires[i];
        if ((wire->a.component == component && wire->a.pin >= pins) ||
            (wire->b.component == component && wire->b.pin >= pins))
            bench_remove_wire(b, i);
    }
    float x = part->x, y = part->y, scale = part->scale;
    int rotation = component_has_flag(type, COMPONENT_ROTATES) ? part->rotation : 0;
    int mirrored = component_has_flag(type, COMPONENT_ROTATES) ? part->mirrored : 0;
    int diagonal = component_has_flag(type, COMPONENT_ROTATES) ? part->rotation_diagonal : 0;
    bench_part_clear(part);
    *part = (Part){.type = type,
                   .x = x,
                   .y = y,
                   .on = component_has_flag(type, COMPONENT_STARTS_ON),
                   .control = component_default_control(type),
                   .value = info->primary ? info->primary->initial : 0,
                   .rotation = rotation,
                   .rotation_diagonal = diagonal,
                   .mirrored = mirrored,
                   .scale = scale};
    ++b->revision;
    return 1;
}

int bench_rotate_part(Workbench *b, int component)
{
    if (b && component >= 0 && component < b->part_count && b->parts[component].group)
    {
        BenchGroup *group = &b->groups[b->parts[component].group - 1];
        group->rotation = (group->rotation + 1) % 4;
        ++b->revision;
        return 1;
    }
    if (!b || component < 0 || component >= b->part_count ||
        !component_has_flag(b->parts[component].type, COMPONENT_ROTATES))
        return 0;
    b->parts[component].rotation = (b->parts[component].rotation + 1) % 4;
    ++b->revision;
    return 1;
}

int bench_rotate_part_45(Workbench *b, int component)
{
    if (!b || component < 0 || component >= b->part_count)
        return 0;
    Part *part = &b->parts[component];
    if (part->group)
    {
        BenchGroup *group = &b->groups[part->group - 1];
        if (group->rotation_diagonal)
            group->rotation = (group->rotation + 1) % 4;
        group->rotation_diagonal = !group->rotation_diagonal;
    }
    else
    {
        if (!component_has_flag(part->type, COMPONENT_ROTATES))
            return 0;
        if (part->rotation_diagonal)
            part->rotation = (part->rotation + 1) % 4;
        part->rotation_diagonal = !part->rotation_diagonal;
    }
    ++b->revision;
    return 1;
}

int bench_mirror_part(Workbench *b, int component)
{
    if (!b || component < 0 || component >= b->part_count)
        return 0;
    if (b->parts[component].group)
    {
        BenchGroup *group = &b->groups[b->parts[component].group - 1];
        group->mirrored = !group->mirrored;
        ++b->revision;
        return 1;
    }
    if (!component_has_flag(b->parts[component].type, COMPONENT_ROTATES))
        return 0;
    b->parts[component].mirrored = !b->parts[component].mirrored;
    ++b->revision;
    return 1;
}

int bench_set_value(Workbench *b, int component, double value)
{
    if (!b || component < 0 || component >= b->part_count || !component_value_valid(b->parts[component].type, value))
        return 0;
    if (b->parts[component].value != value)
    {
        b->parts[component].value = value;
        ++b->revision;
    }
    return 1;
}

int bench_toggle_wire(Workbench *b, Pin a, Pin c)
{
    int i;
    if (!pin_valid(b, a) || !pin_valid(b, c) || same_pin(a, c))
        return 0;
    for (i = 0; i < b->wire_count; ++i)
    {
        Wire w = b->wires[i];
        if ((same_pin(w.a, a) && same_pin(w.b, c)) || (same_pin(w.a, c) && same_pin(w.b, a)))
        {
            bench_remove_wire(b, i);
            return 2;
        }
    }
    if (b->wire_count == WORKBENCH_MAX_WIRES)
        return 0;
    b->wires[b->wire_count++] = (Wire){.a = a, .b = c, .color = BENCH_DEFAULT_WIRE_COLOR};
    ++b->revision;
    return 1;
}

void bench_remove(Workbench *b, int index)
{
    int i;
    if (index < 0 || index >= b->part_count)
        return;
    int removed_group = b->parts[index].group;
    for (i = b->wire_count - 1; i >= 0; --i)
    {
        Wire *w = &b->wires[i];
        if (w->a.component == index || w->b.component == index)
        {
            memmove(w, w + 1, (size_t)(b->wire_count - i - 1) * sizeof(Wire));
            --b->wire_count;
        }
        else
        {
            if (w->a.component > index)
                --w->a.component;
            if (w->b.component > index)
                --w->b.component;
        }
    }
    bench_part_clear(&b->parts[index]);
    for (int part = index + 1; part < b->part_count; ++part)
        bench_part_move(&b->parts[part - 1], &b->parts[part]);
    --b->part_count;
    bench_groups_prune(b);
    if (removed_group)
    {
        int members = 0;
        for (int p = 0; p < b->part_count; ++p)
            members += b->parts[p].group == removed_group;
        if (!members)
            b->groups[removed_group - 1].active = 0;
    }
    ++b->revision;
}

static int root(int *parent, int n)
{
    while (parent[n] != n)
        n = parent[n];
    return n;
}

static void join(int *parent, int a, int b)
{
    parent[root(parent, a)] = root(parent, b);
}

int bench_boolean_supported(const Workbench *b)
{
    for (int i = 0; i < b->part_count; ++i)
        if (!component_has_flag(b->parts[i].type, COMPONENT_CONNECTIVITY))
            return 0;
    return 1;
}

void bench_simulate(const Workbench *b, int led_on[WORKBENCH_MAX_COMPONENTS])
{
    int parent[WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS];
    int i, j;
    memset(led_on, 0, sizeof(int) * WORKBENCH_MAX_COMPONENTS);
    for (i = 0; i < WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS; ++i)
        parent[i] = i;
    for (i = 0; i < b->wire_count; ++i)
        join(parent, b->wires[i].a.component * WORKBENCH_MAX_PINS + b->wires[i].a.pin,
             b->wires[i].b.component * WORKBENCH_MAX_PINS + b->wires[i].b.pin);
    for (i = 0; i < b->part_count; ++i)
    {
        int base = i * WORKBENCH_MAX_PINS;
        if ((b->parts[i].type == PART_SWITCH || b->parts[i].type == PART_BUTTON) && b->parts[i].on)
            join(parent, base, base + 1);
        if (bench_is_board(b->parts[i].type))
        {
            int hole;
            for (hole = 0; hole < bench_pin_count(b->parts[i].type); ++hole)
                join(parent, base + bench_board_group_first(hole), base + hole);
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
                    after = contact.component * WORKBENCH_MAX_PINS + contact.pin;
                    join(parent, i * WORKBENCH_MAX_PINS + pin, after);
                }
            }
        }
    for (i = 0; i < b->part_count; ++i)
    {
        if (b->parts[i].type != PART_LED)
            continue;
        for (j = 0; j < b->part_count; ++j)
        {
            int positive, negative;
            if (component_electrical_type(b->parts[j].type) != PART_SOURCE || !b->parts[j].on)
                continue;
            positive = root(parent, j * WORKBENCH_MAX_PINS);
            negative = root(parent, j * WORKBENCH_MAX_PINS + 1);
            if (positive != negative && root(parent, i * WORKBENCH_MAX_PINS) == positive &&
                root(parent, i * WORKBENCH_MAX_PINS + 1) == negative)
                led_on[i] = 1;
        }
    }
}

int bench_set_code(Workbench *bench, int component, const char *code)
{
    if (!bench || component < 0 || component >= bench->part_count || !code)
        return 0;
    Part *part = &bench->parts[component];
    if (part->code && !strcmp(part->code, code))
        return 1;
    char *copy = _strdup(code);
    if (!copy)
        return 0;
    free(part->code);
    part->code = copy;
    ++bench->revision;
    return 1;
}

int bench_port_name_valid(const char *name)
{
    size_t length = name ? strlen(name) : 0;
    if (!length || length >= BENCH_PORT_NAME_CAPACITY)
        return 0;
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char)name[i] < 32 || (unsigned char)name[i] == 127)
            return 0;
    return strspn(name, " ") != length;
}

/* Start of the "<pin>\t" line of `pin` in a port name list, or NULL. */
static const char *port_name_line(const char *names, int pin)
{
    for (const char *line = names; line && *line;)
    {
        char *tab;
        long number = strtol(line, &tab, 10);
        if (*tab == '\t' && number == pin)
            return line;
        const char *next = strchr(line, '\n');
        line = next ? next + 1 : NULL;
    }
    return NULL;
}

int bench_port_name(const Part *part, int pin, char *name, size_t capacity)
{
    if (!capacity)
        return 0;
    name[0] = 0;
    const char *line = part ? port_name_line(part->port_names, pin) : NULL;
    if (!line)
        return 0;
    const char *start = strchr(line, '\t') + 1;
    size_t length = strcspn(start, "\n");
    if (length >= capacity)
        length = capacity - 1;
    memcpy(name, start, length);
    name[length] = 0;
    return 1;
}

/* Port name list without the line of `pin`, plus "<pin>\t<name>\n" when name is not empty. */
static char *port_names_with(const char *names, int pin, const char *name)
{
    size_t old_length = names ? strlen(names) : 0;
    char *result = malloc(old_length + BENCH_PORT_NAME_CAPACITY + 16);
    if (!result)
        return NULL;
    size_t used = 0;
    const char *skip = port_name_line(names, pin);
    for (const char *line = names; line && *line;)
    {
        const char *next = strchr(line, '\n');
        size_t length = next ? (size_t)(next - line) + 1 : strlen(line);
        if (line != skip)
        {
            memcpy(result + used, line, length);
            used += length;
        }
        line += length;
    }
    if (name[0])
        used += (size_t)sprintf(result + used, "%d\t%s\n", pin, name);
    result[used] = 0;
    return result;
}

int bench_set_port_name(Workbench *bench, Pin pin, const char *name)
{
    if (!bench || !name || pin.component < 0 || pin.component >= bench->part_count || pin.pin < 0 ||
        pin.pin >= bench_pin_count(bench->parts[pin.component].type) || (name[0] && !bench_port_name_valid(name)))
        return 0;
    Part *part = &bench->parts[pin.component];
    char current[BENCH_PORT_NAME_CAPACITY];
    bench_port_name(part, pin.pin, current, sizeof(current));
    if (!strcmp(current, name))
        return 1;
    char *names = port_names_with(part->port_names, pin.pin, name);
    if (!names)
        return 0;
    free(part->port_names);
    part->port_names = names[0] ? names : NULL;
    if (!names[0])
        free(names);
    ++bench->revision;
    return 1;
}

int bench_pin_exposed(const Part *part, int pin)
{
    return pin >= 0 && pin < WORKBENCH_MAX_PINS && (part->exposed[pin / 64] & (UINT64_C(1) << (pin % 64))) != 0;
}

int bench_group_first(const Workbench *bench, int group)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group == group)
            return i;
    return -1;
}

int bench_group_port(const Workbench *bench, int group, int index, Pin *pin)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group == group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                if (bench_pin_exposed(&bench->parts[i], p) && index-- == 0)
                {
                    *pin = (Pin){i, p};
                    return 1;
                }
    return 0;
}

int bench_group_ports(const Workbench *bench, int group)
{
    int count = 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group == group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                count += bench_pin_exposed(&bench->parts[i], p);
    return count;
}

float bench_group_width(const Workbench *bench, int group)
{
    return bench->groups[group - 1].width > 0 ? bench->groups[group - 1].width : 98;
}

int bench_group_resize(Workbench *bench, int group, float width, float height)
{
    const float minimum_width = 64, minimum_height = 44, maximum_extent = 10000;
    if (!bench || group < 1 || group > WORKBENCH_MAX_COMPONENTS || !bench->groups[group - 1].active ||
        !isfinite(width) || !isfinite(height) || width < minimum_width || width > maximum_extent ||
        height < minimum_height || height > maximum_extent)
        return 0;
    BenchGroup *package = &bench->groups[group - 1];
    if (package->width != width || package->height != height)
    {
        package->width = width;
        package->height = height;
        ++bench->revision;
    }
    return 1;
}

static int group_layout_custom(const Workbench *bench, int group, int count)
{
    return count > 0 && count <= BENCH_GROUP_LAYOUT_MAX && bench->groups[group - 1].port_layout == count;
}

static BenchPortPlace group_place(const Workbench *bench, int group, int count, int index)
{
    if (group_layout_custom(bench, group, count))
        return bench->groups[group - 1].port_places[index];
    int left = (count + 1) / 2;
    BenchPortPlace place = {(unsigned char)(index >= left), (unsigned char)(index < left ? index : index - left)};
    return place;
}

/* Stored rows may have gaps or ties (hand-edited files); slots are always 0..n-1 by (row, index). */
static int place_before(BenchPortPlace a, int a_index, BenchPortPlace b, int b_index)
{
    return a.row < b.row || (a.row == b.row && a_index < b_index);
}

void bench_group_port_place(const Workbench *bench, int group, int index, int *side, int *slot, int *side_count)
{
    int count = bench_group_ports(bench, group);
    BenchPortPlace own = group_place(bench, group, count, index);
    *side = own.side;
    *slot = 0;
    *side_count = 0;
    for (int other = 0; other < count; ++other)
    {
        BenchPortPlace place = group_place(bench, group, count, other);
        if (place.side != own.side)
            continue;
        ++*side_count;
        if (other != index && place_before(place, other, own, index))
            ++*slot;
    }
}

static int group_rows(const Workbench *bench, int group)
{
    int count = bench_group_ports(bench, group), sides[2] = {0, 0};
    for (int index = 0; index < count; ++index)
        ++sides[group_place(bench, group, count, index).side != 0];
    return sides[0] > sides[1] ? sides[0] : sides[1];
}

float bench_group_height(const Workbench *bench, int group)
{
    int rows = group_rows(bench, group);
    float minimum = rows > 2 ? rows * BENCH_GROUP_PIN_SPACING + 14.0f : 44.0f;
    return fmaxf(minimum, bench->groups[group - 1].height);
}

int bench_group_port_index(const Workbench *bench, Pin pin)
{
    if (pin.component < 0 || pin.component >= bench->part_count)
        return -1;
    const Part *part = &bench->parts[pin.component];
    if (!part->group || !bench_pin_exposed(part, pin.pin))
        return -1;
    int index = 0;
    for (int i = 0; i <= pin.component; ++i)
        if (bench->parts[i].group == part->group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                if (bench_pin_exposed(&bench->parts[i], p) && (i < pin.component || p < pin.pin))
                    ++index;
    return index;
}

/* Terminals of one side in slot order. */
static int side_order(const Workbench *bench, int group, int count, int side, int order[BENCH_GROUP_LAYOUT_MAX])
{
    int length = 0;
    for (int index = 0; index < count; ++index)
    {
        int own_side, slot, side_count;
        bench_group_port_place(bench, group, index, &own_side, &slot, &side_count);
        if (own_side != side)
            continue;
        order[slot] = index;
        ++length;
    }
    return length;
}

static void remove_slot(int order[BENCH_GROUP_LAYOUT_MAX], int *length, int slot)
{
    for (int k = slot; k + 1 < *length; ++k)
        order[k] = order[k + 1];
    --*length;
}

static void insert_slot(int order[BENCH_GROUP_LAYOUT_MAX], int *length, int slot, int index)
{
    for (int k = *length; k > slot; --k)
        order[k] = order[k - 1];
    order[slot] = index;
    ++*length;
}

int bench_group_move_port(Workbench *bench, int group, int index, int side, int slot)
{
    if (!bench || group < 1 || group > WORKBENCH_MAX_COMPONENTS || !bench->groups[group - 1].active ||
        (side != BENCH_PORT_LEFT && side != BENCH_PORT_RIGHT))
        return 0;
    int count = bench_group_ports(bench, group);
    if (index < 0 || index >= count || count > BENCH_GROUP_LAYOUT_MAX)
        return 0;
    int order[2][BENCH_GROUP_LAYOUT_MAX], length[2];
    for (int s = 0; s < 2; ++s)
        length[s] = side_order(bench, group, count, s, order[s]);
    int from_side, from_slot, side_count;
    bench_group_port_place(bench, group, index, &from_side, &from_slot, &side_count);
    remove_slot(order[from_side], &length[from_side], from_slot);
    slot = slot < 0 ? 0 : slot > length[side] ? length[side] : slot;
    if (side == from_side && slot == from_slot && group_layout_custom(bench, group, count))
        return 1;
    insert_slot(order[side], &length[side], slot, index);
    BenchGroup *package = &bench->groups[group - 1];
    for (int s = 0; s < 2; ++s)
        for (int k = 0; k < length[s]; ++k)
            package->port_places[order[s][k]] = (BenchPortPlace){(unsigned char)s, (unsigned char)k};
    package->port_layout = count;
    ++bench->revision;
    return 1;
}

void bench_terminal_position(const Workbench *bench, Pin pin, float *x, float *y)
{
    const Part *part = &bench->parts[pin.component];
    if (!part->group || !bench_pin_exposed(part, pin.pin))
    {
        bench_pin_position(part, pin.pin, x, y);
        return;
    }
    int side, slot, rows;
    bench_group_port_place(bench, part->group, bench_group_port_index(bench, pin), &side, &slot, &rows);
    const BenchGroup *group = &bench->groups[part->group - 1];
    float half_width = bench_group_width(bench, part->group) * .5f + 11;
    WirePoint local = {side == BENCH_PORT_LEFT ? -half_width : half_width,
                       (slot - (rows - 1) * .5f) * BENCH_GROUP_PIN_SPACING};
    WirePoint world = bench_group_to_world(group, local);
    *x = world.x;
    *y = world.y;
}

WirePoint bench_group_to_world(const BenchGroup *group, WirePoint local)
{
    Part frame = {.x = group->x, .y = group->y, .rotation = group->rotation,
                  .rotation_diagonal = group->rotation_diagonal, .mirrored = group->mirrored};
    return bench_part_to_world(&frame, local);
}

WirePoint bench_group_to_local(const BenchGroup *group, WirePoint world)
{
    Part frame = {.x = group->x, .y = group->y, .rotation = group->rotation,
                  .rotation_diagonal = group->rotation_diagonal, .mirrored = group->mirrored};
    return bench_part_to_local(&frame, world);
}

