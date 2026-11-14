#include "workbench.h"
#include <string.h>
#include <math.h>

int wire_path(const Wire *wire, WirePoint start, WirePoint end, float radius, WirePoint output[WIRE_PATH_CAPACITY])
{
    int i, count=0;
    output[count++]=start;
    for(i=0;i<wire->knee_count && i<WIRE_MAX_KNEES;++i) {
        WirePoint previous=i?wire->knees[i-1]:start;
        WirePoint corner=wire->knees[i];
        WirePoint next=i+1<wire->knee_count?wire->knees[i+1]:end;
        float before=hypotf(previous.x-corner.x,previous.y-corner.y);
        float after=hypotf(next.x-corner.x,next.y-corner.y);
        float r=fminf(radius,fminf(before*.45f,after*.45f));
        if(r<=0 || before<.001f || after<.001f) output[count++]=corner;
        else {
            int step;
            WirePoint a={corner.x+(previous.x-corner.x)*r/before,corner.y+(previous.y-corner.y)*r/before};
            WirePoint b={corner.x+(next.x-corner.x)*r/after,corner.y+(next.y-corner.y)*r/after};
            for(step=0;step<=8;++step) {
                float t=step/8.0f, u=1-t;
                output[count++]=(WirePoint){u*u*a.x+2*u*t*corner.x+t*t*b.x,u*u*a.y+2*u*t*corner.y+t*t*b.y};
            }
        }
    }
    output[count++]=end;
    return count;
}

int bench_pin_count(PartType type)
{
    return type == PART_BREADBOARD ? 100 : ((type >= PART_SOURCE && type <= PART_LED) || type == PART_RESISTOR || type == PART_CAPACITOR || type == PART_INDUCTOR ? 2 : 0);
}

static int pin_valid(const Workbench *b, Pin p)
{
    return p.component >= 0 && p.component < b->part_count && p.pin >= 0 &&
           p.pin < bench_pin_count(b->parts[p.component].type);
}

static int same_pin(Pin a, Pin b) { return a.component == b.component && a.pin == b.pin; }

int bench_add(Workbench *b, PartType type, float x, float y)
{
    int id;
    if (b->part_count >= WORKBENCH_MAX_COMPONENTS || bench_pin_count(type) == 0) return -1;
    id = b->part_count++;
    b->parts[id] = (Part){type, x, y, type == PART_SOURCE};
    return id;
}

int bench_toggle_wire(Workbench *b, Pin a, Pin c)
{
    int i;
    if (!pin_valid(b, a) || !pin_valid(b, c) || same_pin(a, c)) return 0;
    for (i = 0; i < b->wire_count; ++i) {
        Wire w = b->wires[i];
        if ((same_pin(w.a, a) && same_pin(w.b, c)) || (same_pin(w.a, c) && same_pin(w.b, a))) {
            memmove(&b->wires[i], &b->wires[i + 1], (size_t)(b->wire_count - i - 1) * sizeof(Wire));
            --b->wire_count;
            return 2;
        }
    }
    if (b->wire_count == WORKBENCH_MAX_WIRES) return 0;
    b->wires[b->wire_count++] = (Wire){.a=a, .b=c};
    return 1;
}

void bench_remove(Workbench *b, int index)
{
    int i;
    if (index < 0 || index >= b->part_count) return;
    for (i = b->wire_count - 1; i >= 0; --i) {
        Wire *w = &b->wires[i];
        if (w->a.component == index || w->b.component == index) {
            memmove(w, w + 1, (size_t)(b->wire_count - i - 1) * sizeof(Wire));
            --b->wire_count;
        } else {
            if (w->a.component > index) --w->a.component;
            if (w->b.component > index) --w->b.component;
        }
    }
    memmove(&b->parts[index], &b->parts[index + 1], (size_t)(b->part_count - index - 1) * sizeof(Part));
    --b->part_count;
}

static int root(int *parent, int n)
{
    while (parent[n] != n) n = parent[n];
    return n;
}

static void join(int *parent, int a, int b) { parent[root(parent, a)] = root(parent, b); }

int bench_boolean_supported(const Workbench *b)
{
    int i;
    for(i=0;i<b->part_count;++i)
        if(b->parts[i].type==PART_RESISTOR || b->parts[i].type==PART_CAPACITOR || b->parts[i].type==PART_INDUCTOR) return 0;
    return 1;
}

void bench_simulate(const Workbench *b, int led_on[WORKBENCH_MAX_COMPONENTS])
{
    int parent[WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS];
    int i, j;
    memset(led_on, 0, sizeof(int) * WORKBENCH_MAX_COMPONENTS);
    for (i = 0; i < WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS; ++i) parent[i] = i;
    for (i = 0; i < b->wire_count; ++i)
        join(parent, b->wires[i].a.component * WORKBENCH_MAX_PINS + b->wires[i].a.pin,
             b->wires[i].b.component * WORKBENCH_MAX_PINS + b->wires[i].b.pin);
    for (i = 0; i < b->part_count; ++i) {
        int base = i * WORKBENCH_MAX_PINS;
        if (b->parts[i].type == PART_SWITCH && b->parts[i].on) join(parent, base, base + 1);
        if (b->parts[i].type == PART_BREADBOARD) {
            int group, hole;
            for (group = 0; group < 20; ++group)
                for (hole = 1; hole < 5; ++hole) join(parent, base + group * 5, base + group * 5 + hole);
        }
    }
    for (i = 0; i < b->part_count; ++i) {
        if (b->parts[i].type != PART_LED) continue;
        for (j = 0; j < b->part_count; ++j) {
            int positive, negative;
            if (b->parts[j].type != PART_SOURCE || !b->parts[j].on) continue;
            positive = root(parent, j * WORKBENCH_MAX_PINS);
            negative = root(parent, j * WORKBENCH_MAX_PINS + 1);
            if (positive != negative && root(parent, i * WORKBENCH_MAX_PINS) == positive && root(parent, i * WORKBENCH_MAX_PINS + 1) == negative)
                led_on[i] = 1;
        }
    }
}
