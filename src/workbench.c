#include "bench_path.h"
#include "workbench.h"
#include "component_devices.h"
#include "component_registry.h"
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>

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
    if (source->terminals)
    {
        copy.terminals = calloc(1, sizeof(*copy.terminals));
        if (!copy.terminals)
            return 0;
        copy.terminals->pin_count = source->terminals->pin_count;
        copy.terminals->paths = calloc(copy.terminals->pin_count, sizeof(BenchPath));
        if (!copy.terminals->paths)
        {
            free(copy.terminals);
            return 0;
        }
        for (size_t i = 0; i < copy.terminals->pin_count; ++i)
            if (bench_path_clone(&copy.terminals->paths[i], &source->terminals->paths[i]) != BENCH_PATH_OK)
            {
                bench_part_clear(&copy);
                return 0;
            }
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

int bench_set_scale(Workbench *bench, int component, float scale)
{
    if (!bench || component < 0 || component >= bench->part_count || !isfinite(scale) ||
        scale < PART_SCALE_MIN || scale > PART_SCALE_MAX)
        return 0;
    if (bench_part_scale(&bench->parts[component]) != scale)
    {
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

static int wire_path_indexed(const Wire *wire, WirePoint start, WirePoint end, float radius,
                             WirePoint output[WIRE_PATH_CAPACITY], int *slots)
{
    int i, count = 0;
    output[count++] = start;
    for (i = 0; i < wire->knee_count && i < WIRE_MAX_KNEES; ++i)
    {
        WirePoint previous = i ? wire->knees[i - 1] : start;
        WirePoint corner = wire->knees[i];
        WirePoint next = i + 1 < wire->knee_count ? wire->knees[i + 1] : end;
        float before = hypotf(previous.x - corner.x, previous.y - corner.y);
        float after = hypotf(next.x - corner.x, next.y - corner.y);
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
    if (pin < 0 || pin >= BREADBOARD_PINS)
        return -1;
    return pin < 300 ? (pin / 5) * 5 : 300 + ((pin - 300) / 30) * 30;
}

WirePoint bench_part_to_world(const Part *part, WirePoint local)
{
    float scale = bench_part_scale(part);
    local.x *= scale;
    local.y *= scale;
    WirePoint rotated = local;
    int rotation = part->type == PART_BREADBOARD ? 0 : part->rotation;
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
    return (WirePoint){part->x + rotated.x, part->y + rotated.y};
}

WirePoint bench_part_to_local(const Part *part, WirePoint world)
{
    float scale = bench_part_scale(part);
    WirePoint offset = {(world.x - part->x) / scale, (world.y - part->y) / scale};
    int rotation = part->type == PART_BREADBOARD ? 0 : part->rotation;
    switch (rotation)
    {
    case 1:
        return (WirePoint){offset.y, -offset.x};
    case 2:
        return (WirePoint){-offset.x, -offset.y};
    case 3:
        return (WirePoint){-offset.y, offset.x};
    default:
        return offset;
    }
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
    if (p->type == PART_BREADBOARD)
    {
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
    else if (component_has_flag(p->type, COMPONENT_QUAD_GATE))
    {
        *x = (float)((pin < 7 ? pin : 13 - pin) * 14 - 42);
        *y = pin < 7 ? -15.0f : 15.0f;
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
    return device && bench_pin_count(type) > 3 && type != PART_RGB_LED;
}

static int lead_editable(const Part *part, int pin)
{
    return part && pin >= 0 && pin < bench_pin_count(part->type) && part->type != PART_BREADBOARD &&
           !component_has_flag(part->type, COMPONENT_QUAD_GATE) &&
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
        float angle = (float)(-part->rotation * 3.141592653589793 * .5);
        float c = cosf(angle), s = sinf(angle), x = pin ? 10 : -10;
        local = (WirePoint){x * c - 8 * s, x * s + 8 * c};
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

int bench_find_contact_after(const Workbench *b, int component, int pin, int after, Pin *contact)
{
    int board, hole;
    float x, y;
    if (!b || !contact || component < 0 || component >= b->part_count || b->parts[component].type == PART_BREADBOARD ||
        pin < 0 || pin >= bench_pin_count(b->parts[component].type))
        return 0;
    bench_pin_position(&b->parts[component], pin, &x, &y);
    for (board = 0; board < b->part_count; ++board)
        if (board != component && b->parts[board].group == b->parts[component].group)
        {
            for (hole = 0; hole < bench_pin_count(b->parts[board].type); ++hole)
            {
                float hx, hy;
                bench_pin_position(&b->parts[board], hole, &hx, &hy);
                if (board * WORKBENCH_MAX_PINS + hole > after &&
                    fabsf(x - hx) < .01f && fabsf(y - hy) < .01f)
                {
                    *contact = (Pin){board, hole};
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
    if (!b || !part || !snapped_x || !snapped_y || part->type == PART_BREADBOARD)
        return 0;
    for (pin = 0; pin < bench_pin_count(part->type); ++pin)
    {
        float x, y;
        bench_pin_position(part, pin, &x, &y);
        for (board = 0; board < b->part_count; ++board)
            if (b->parts[board].type == PART_BREADBOARD && b->parts[board].group == part->group)
            {
                for (hole = 0; hole < BREADBOARD_PINS; ++hole)
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
                          .board_layout = type == PART_BREADBOARD};
    ++b->revision;
    return id;
}

int bench_rotate_part(Workbench *b, int component)
{
    if (!b || component < 0 || component >= b->part_count ||
        !component_has_flag(b->parts[component].type, COMPONENT_ROTATES))
        return 0;
    b->parts[component].rotation = (b->parts[component].rotation + 1) % 4;
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
    b->wires[b->wire_count++] = (Wire){.a = a, .b = c};
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
        if (b->parts[i].type == PART_BREADBOARD)
        {
            int hole;
            for (hole = 0; hole < BREADBOARD_PINS; ++hole)
                join(parent, base + bench_board_group_first(hole), base + hole);
        }
    }
    for (i = 0; i < b->part_count; ++i)
        if (b->parts[i].type != PART_BREADBOARD)
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
