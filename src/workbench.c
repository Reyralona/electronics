#include "workbench.h"
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>

int bench_parse_value(const char *text,double *value)
{
    char buffer[64], *end; size_t i,n; double parsed;
    if(!text || !value) return 0;
    n=strlen(text); if(!n || n>=sizeof(buffer)) return 0;
    for(i=0;i<n;++i) {
        char c=text[i];
        if(c==',') c='.';
        if(!((c>='0' && c<='9') || c=='.' || c=='e' || c=='E' || c=='+' || c=='-')) return 0;
        buffer[i]=c;
    }
    buffer[n]='\0'; errno=0; parsed=strtod(buffer,&end);
    if(errno==ERANGE || end==buffer || *end || !isfinite(parsed) || parsed<1e-12 || parsed>1e12) return 0;
    *value=parsed; return 1;
}

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
    return type == PART_BREADBOARD ? BREADBOARD_PINS : ((type >= PART_AND && type <= PART_XOR) || type == PART_DFF) ? 3 : (type == PART_NOT || type == PART_CLOCK) ? 2 : ((type >= PART_SOURCE && type <= PART_LED) || type == PART_RESISTOR || type == PART_CAPACITOR || type == PART_INDUCTOR ? 2 : 0);
}

int bench_board_group_first(int pin)
{
    if(pin<0 || pin>=BREADBOARD_PINS) return -1;
    return pin<300?(pin/5)*5:300+((pin-300)/30)*30;
}

void bench_pin_position(const Part *p,int pin,float *x,float *y)
{
    if(p->type==PART_BREADBOARD) {
        int group=pin/5,row=pin%5;
        if(group<20) { *x=p->x-63.0f+(group%10)*14.0f; *y=p->y+(p->board_layout?(group<10?-55.0f:15.0f):(group<10?-39.0f:11.0f))+row*(p->board_layout?10.0f:7.0f); }
        else if(group<36) { int extra=(group-20)%8; *x=p->x+(extra<4?-119.0f+extra*14.0f:77.0f+(extra-4)*14.0f); *y=p->y+(p->board_layout?(group<28?-55.0f:15.0f):(group<28?-39.0f:11.0f))+row*(p->board_layout?10.0f:7.0f); }
        else if(pin<300) { int extra=(group-36)%12; *x=p->x+(extra<6?-203.0f+extra*14.0f:133.0f+(extra-6)*14.0f); *y=p->y+(p->board_layout?(group<48?-55.0f:15.0f):(group<48?-39.0f:11.0f))+row*(p->board_layout?10.0f:7.0f); }
        else { int rail=(pin-300)/30, column=(pin-300)%30; *x=p->x-203.0f+column*14.0f; *y=p->y+(rail==0?-98.0f:rail==1?-82.0f:rail==2?78.0f:94.0f); }
    } else if((p->type>=PART_AND && p->type<=PART_XOR) || p->type==PART_DFF) {
        float dx=pin==2?49.0f:-49.0f,dy=pin==0?-15.0f:pin==1?15.0f:0.0f;
        switch(p->rotation) {
        case 1: *x=p->x-dy; *y=p->y+dx; break;
        case 2: *x=p->x-dx; *y=p->y-dy; break;
        case 3: *x=p->x+dy; *y=p->y-dx; break;
        default: *x=p->x+dx; *y=p->y+dy; break;
        }
    } else {
        float distance=pin?49.0f:-49.0f;
        switch(p->rotation) {
        case 1: *x=p->x; *y=p->y+distance; break;
        case 2: *x=p->x-distance; *y=p->y; break;
        case 3: *x=p->x; *y=p->y-distance; break;
        default: *x=p->x+distance; *y=p->y; break;
        }
    }
}

int bench_find_contact(const Workbench *b,int component,int pin,Pin *contact)
{
    int board,hole; float x,y;
    if(!b || !contact || component<0 || component>=b->part_count || b->parts[component].type==PART_BREADBOARD || pin<0 || pin>=bench_pin_count(b->parts[component].type)) return 0;
    bench_pin_position(&b->parts[component],pin,&x,&y);
    for(board=0;board<b->part_count;++board) if(b->parts[board].type==PART_BREADBOARD) {
        for(hole=0;hole<BREADBOARD_PINS;++hole) { float hx,hy; bench_pin_position(&b->parts[board],hole,&hx,&hy);
            if(fabsf(x-hx)<.01f && fabsf(y-hy)<.01f) { *contact=(Pin){board,hole}; return 1; }
        }
    }
    return 0;
}

int bench_snap_to_breadboard(Workbench *b,int component)
{
    int board,hole,pin,found=0; float best=64.0f,dx=0,dy=0;
    if(!b || component<0 || component>=b->part_count || b->parts[component].type==PART_BREADBOARD) return 0;
    for(pin=0;pin<bench_pin_count(b->parts[component].type);++pin) {
        float x,y; bench_pin_position(&b->parts[component],pin,&x,&y);
        for(board=0;board<b->part_count;++board) if(b->parts[board].type==PART_BREADBOARD) {
            for(hole=0;hole<BREADBOARD_PINS;++hole) { float hx,hy,distance; bench_pin_position(&b->parts[board],hole,&hx,&hy);
                distance=(hx-x)*(hx-x)+(hy-y)*(hy-y);
                if(distance<best) { best=distance; dx=hx-x; dy=hy-y; found=1; }
            }
        }
    }
    if(found) { b->parts[component].x+=dx; b->parts[component].y+=dy; }
    return found;
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
    b->parts[id] = (Part){type, x, y, type == PART_SOURCE || type == PART_CLOCK,
        type==PART_SOURCE?5.0:type==PART_RESISTOR?1000.0:type==PART_CAPACITOR?1e-6:type==PART_INDUCTOR?.01:type==PART_CLOCK?1000.0:0.0,0,type==PART_BREADBOARD};
    return id;
}

int bench_rotate_part(Workbench *b,int component)
{
    if(!b || component<0 || component>=b->part_count || b->parts[component].type==PART_BREADBOARD) return 0;
    b->parts[component].rotation=(b->parts[component].rotation+1)%4;
    return 1;
}

int bench_set_value(Workbench *b, int component, double value)
{
    PartType type;
    if(!b || component<0 || component>=b->part_count || !isfinite(value) || value<1e-12 || value>1e12) return 0;
    type=b->parts[component].type;
    if(type!=PART_SOURCE && type!=PART_RESISTOR && type!=PART_CAPACITOR && type!=PART_INDUCTOR && type!=PART_CLOCK) return 0;
    b->parts[component].value=value;
    return 1;
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
        if(b->parts[i].type==PART_RESISTOR || b->parts[i].type==PART_CAPACITOR || b->parts[i].type==PART_INDUCTOR || (b->parts[i].type>=PART_AND && b->parts[i].type<=PART_DFF)) return 0;
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
            int hole;
            for (hole = 0; hole < BREADBOARD_PINS; ++hole)
                join(parent, base + bench_board_group_first(hole), base + hole);
        }
    }
    for(i=0;i<b->part_count;++i) if(b->parts[i].type!=PART_BREADBOARD) {
        int pin; for(pin=0;pin<bench_pin_count(b->parts[i].type);++pin) { Pin contact;
            if(bench_find_contact(b,i,pin,&contact)) join(parent,i*WORKBENCH_MAX_PINS+pin,contact.component*WORKBENCH_MAX_PINS+contact.pin);
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
