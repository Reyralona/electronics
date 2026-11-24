#include "workbench_grid.h"
#include "app_controls.h"
#include "app_editing.h"
#include <windows.h>
#include "app_render.h"
#include "app_editor.h"
#include "app_text_utf8.h"
#include "app_layout.h"
#include "app_palette.h"
#include "component_registry.h"
#include "workbench_pack.h"
#include "workbench_examples.h"
#include "component_devices.h"
#include "mcu_lang.h"
#include "mcu_board.h"
#include "code_text.h"
#include "app_code_editor.h"
#include "ui_text.h"
#include "netlist_logic.h"
#include "chip_docs.h"
#include "netlist.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL3/SDL.h>

/* Transparent ("raio-x") part bodies: every primitive of a part is drawn at XRAY_ALPHA through the
 * constant blend colour, so terminals and wires behind it stay visible. glBlendColor is GL 1.4 and is
 * loaded at run time; without it parts draw opaque and only hit testing changes. */
#define XRAY_ALPHA .32f
typedef void(APIENTRY *BlendColorProc)(GLfloat, GLfloat, GLfloat, GLfloat);
static int xray_active;

static int xray_begin(void)
{
    static BlendColorProc blend_color;
    static int loaded;
    if (!loaded)
    {
        blend_color = (BlendColorProc)(void *)wglGetProcAddress("glBlendColor");
        loaded = 1;
    }
    if (!blend_color)
        return 0;
    blend_color(0, 0, 0, XRAY_ALPHA);
    glEnable(GL_BLEND);
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    xray_active = 1;
    return 1;
}

static void xray_end(void)
{
    if (!xray_active)
        return;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    xray_active = 0;
}

/* Glows inside part art use their own alpha, then return to the x-ray blend when it is on. */
static void glow_blend_begin(void)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void glow_blend_end(void)
{
    if (xray_active)
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    else
        glDisable(GL_BLEND);
}

static void rect(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}
/* Theme affects interface surfaces and ink; physical parts and wire colors keep their palette. */
static int dark_interface;
static float drawing_scale = 1;
static int interface_layer;
static void color(float r, float g, float b)
{
    if (dark_interface && interface_layer)
    {
        float minimum = fminf(r, fminf(g, b));
        float maximum = fmaxf(r, fmaxf(g, b));
        if (minimum > .55f)
        {
            r = .10f + (1 - r) * .35f;
            g = .14f + (1 - g) * .35f;
            b = .19f + (1 - b) * .35f;
        }
        else if (maximum < .65f)
        {
            r = .70f + r * .35f;
            g = .73f + g * .35f;
            b = .77f + b * .30f;
        }
    }
    glColor3f(r, g, b);
}
typedef struct
{
    float r, g, b;
} SurfaceColor;

static void gradient_rect(float x, float y, float width, float height, SurfaceColor top, SurfaceColor bottom)
{
    glBegin(GL_QUADS);
    color(top.r, top.g, top.b);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    color(bottom.r, bottom.g, bottom.b);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

static void line(float x, float y, float x2, float y2)
{
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x2, y2);
    glEnd();
}
static void circle(float x, float y, float radius);
static void rounded_rect(float x, float y, float w, float h, float radius);
static void ui_shadow(AppRect r, float radius, float depth);
/* Shared palette (BenchColor): wire insulation and swatch tone, and the lit LED lens. */
typedef struct
{
    float wire[3];
    float lit[3];
} PaletteTone;
static const PaletteTone palette_tones[BENCH_COLOR_COUNT] = {
    {{.78f, .14f, .12f}, {1, .30f, .24f}},   {{.12f, .55f, .25f}, {.40f, 1, .45f}},
    {{.12f, .36f, .58f}, {.40f, .62f, 1}},   {{.90f, .72f, .08f}, {1, .92f, .30f}},
    {{.86f, .88f, .89f}, {1, 1, 1}},         {{.92f, .48f, .10f}, {1, .62f, .18f}},
    {{.48f, .24f, .70f}, {.78f, .45f, 1}},   {{.92f, .42f, .64f}, {1, .55f, .78f}},
    {{.10f, .66f, .76f}, {.35f, .95f, 1}},   {{.52f, .78f, .12f}, {.72f, 1, .25f}},
    {{.12f, .12f, .14f}, {.12f, .12f, .14f}}};

static const PaletteTone *palette_tone(int color)
{
    return &palette_tones[color >= 0 && color < BENCH_COLOR_COUNT ? color : BENCH_DEFAULT_WIRE_COLOR];
}

#define WIRE_TIP_INSULATION 14.0f /* colored end drawn over the hole, along the wire */
#define WIRE_TIP_METAL 6.0f       /* bare lead entering the hole */
static void wire_contact_tip(WirePoint hole, WirePoint next, int wire_color)
{
    float dx = next.x - hole.x, dy = next.y - hole.y, length = hypotf(dx, dy);
    if (length < .01f)
        return;
    dx /= length;
    dy /= length;
    /* Never past the first knee: a knee next to the hole would otherwise leave a spur beyond the bend. */
    float insulation = fminf(WIRE_TIP_INSULATION, length), metal = fminf(WIRE_TIP_METAL, length);
    const float *tone = palette_tone(wire_color)->wire;
    if (insulation > WIRE_TIP_METAL - 1)
    {
        color(tone[0], tone[1], tone[2]);
        glLineWidth(5);
        line(hole.x + dx * insulation, hole.y + dy * insulation, hole.x + dx * (WIRE_TIP_METAL - 1),
             hole.y + dy * (WIRE_TIP_METAL - 1));
    }
    color(.75f, .52f, .22f);
    glLineWidth(3);
    line(hole.x + dx * metal, hole.y + dy * metal, hole.x, hole.y);
    color(.98f, .8f, .43f);
    circle(hole.x, hole.y, 2.5f);
    glLineWidth(1);
}
/* Current flow: small yellow dots spaced along the wire path, moving in the current direction.
 * Speed grows with the measured current range (level 1..3); dots stay still while paused. */
#define FLOW_DOT_SPACING 26.0f
#define FLOW_DOT_RADIUS 2.6f
static int flow_running;

static void wire_direction_marker(const WirePoint *path, int count, int reverse, int level)
{
    const float speed_per_ms[4] = {0, .02f, .05f, .10f};
    float total = 0;
    for (int i = 1; i < count; ++i)
        total += hypotf(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
    if (total <= 0 || level < 1 || level > 3)
        return;
    float phase = flow_running ? fmodf((float)SDL_GetTicks() * speed_per_ms[level], FLOW_DOT_SPACING) : 0;
    if (reverse)
        phase = FLOW_DOT_SPACING - phase;
    float travelled = 0;
    glColor3f(1, .82f, .12f);
    for (int i = 1; i < count; ++i)
    {
        float dx = path[i].x - path[i - 1].x, dy = path[i].y - path[i - 1].y, length = hypotf(dx, dy);
        if (length <= 0)
            continue;
        float first = fmodf(phase - travelled, FLOW_DOT_SPACING);
        if (first < 0)
            first += FLOW_DOT_SPACING;
        for (float s = first; s < length; s += FLOW_DOT_SPACING)
            circle(path[i - 1].x + dx * s / length, path[i - 1].y + dy * s / length, FLOW_DOT_RADIUS + .4f * level);
        travelled += length;
    }
}
static void circle(float x, float y, float radius)
{
    int i;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (i = 0; i <= 32; ++i)
    {
        float a = (float)i * 6.2831853f / 32;
        glVertex2f(x + cosf(a) * radius, y + sinf(a) * radius);
    }
    glEnd();
}
static void circle_outline(float x, float y, float radius)
{
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; ++i)
    {
        float a = (float)i * 6.2831853f / 32;
        glVertex2f(x + cosf(a) * radius, y + sinf(a) * radius);
    }
    glEnd();
}

GLuint app_render_font_create(void)
{
    HDC dc = wglGetCurrentDC();
    GLuint base;
    HFONT font;
    HGDIOBJ old;
    if (!dc)
        return 0;
    base = glGenLists(128);
    if (!base)
        return 0;
    font = CreateFontA(-17, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, DEFAULT_PITCH,
                       "Segoe UI");
    if (!font)
    {
        glDeleteLists(base, 128);
        return 0;
    }
    old = SelectObject(dc, font);
    if (!old || old == HGDI_ERROR || !wglUseFontBitmapsA(dc, 0, 128, base))
    {
        if (old && old != HGDI_ERROR)
            SelectObject(dc, old);
        DeleteObject(font);
        glDeleteLists(base, 128);
        return 0;
    }
    SelectObject(dc, old);
    DeleteObject(font);
    return base;
}
static void text(GLuint font, float x, float y, const char *str)
{
    if (app_text_utf8(x, y, str, 2048))
        return;
    if (!font)
        return;
    glRasterPos2f(x, y);
    glListBase(font);
    glCallLists((GLsizei)strlen(str), GL_UNSIGNED_BYTE, str);
}

static void name_text(GLuint font, float x, float y, const char *name, int maximum_width)
{
    if (!app_text_utf8(x, y, name, maximum_width))
        text(font, x, y, tr(TXT_UI_TEXT_MISSING));
}

static void world_text(GLuint font, float x, float y, const char *label, float zoom)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(1 / zoom, 1 / zoom, 1);
    text(font, 0, 0, label);
    glPopMatrix();
}

/* Graph title; current is 0 for terminal voltage, 1 for part current, 2 for wire current. */
static void trace_title(char *title, size_t capacity, int pin, int current)
{
    if (current == 2)
        snprintf(title, capacity, "%s", tr(TXT_UI_GRAPH_WIRE_CURRENT));
    else if (current)
        snprintf(title, capacity, "%s", tr(TXT_UI_GRAPH_PIN_CURRENT));
    else
        snprintf(title, capacity, tr(TXT_UI_GRAPH_VOLTAGE), pin + 1);
}

/* Window, title and axes shared by the full curve and the live graph. */
static void graph_frame(GLuint font, const char *title)
{
    AppRect panel = app_layout_graph_panel();
    WirePoint offset = app_layout_graph_offset(); /* the caller already moved the drawing */
    panel.x -= offset.x;
    panel.y -= offset.y;
    ui_shadow(panel, 2, 8);
    color(.98f, .98f, .99f);
    rect(panel.x, panel.y, panel.w, panel.h);
    color(.14f, .22f, .28f);
    text(font, 856, 410, title);
    color(.72f, .77f, .79f);
    line(918, 431, 918, 565);
    line(918, 565, 1220, 565);
}

static void trace_draw(GLuint font, const SpiceTrace *trace, int selected, int pin, int current)
{
    int i;
    double low = trace->samples[0].value, high = low, first = trace->samples[0].time,
           last = trace->samples[trace->count - 1].time;
    double half_range, mid;
    char label[100];
    for (i = 1; i < trace->count; ++i)
    {
        double v = trace->samples[i].value;
        if (v < low)
            low = v;
        if (v > high)
            high = v;
    }
    mid = low * .5 + high * .5;
    half_range = high * .5 - low * .5;
    if (half_range < 1e-20)
        half_range = 1.0;
    trace_title(label, sizeof(label), pin, current);
    graph_frame(font, label);
    color(.14f, .22f, .28f);
    snprintf(label, sizeof(label), current ? "%.4g A" : "%.4g V", high);
    text(font, 856, 433, label);
    snprintf(label, sizeof(label), current ? "%.3g A" : "%.3g V", low);
    text(font, 856, 572, label);
    color(.12f, .47f, .7f);
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    for (i = 0; i < trace->count; ++i)
    {
        double x = last > first ? (trace->samples[i].time - first) / (last - first) : 0;
        double y = (trace->samples[i].value * .5 - mid * .5) / half_range + .5;
        glVertex2f((float)(918 + 302 * x), (float)(565 - 134 * y));
    }
    glEnd();
    glLineWidth(1);
    {
        double x = last > first ? (trace->samples[selected].time - first) / (last - first) : 0;
        color(.9f, .48f, .12f);
        line((float)(918 + 302 * x), 431, (float)(918 + 302 * x), 565);
    }
    color(.14f, .22f, .28f);
    snprintf(label, sizeof(label), "%.4g s", first);
    text(font, 918, 585, label);
    snprintf(label, sizeof(label), "%.4g s", last);
    text(font, 1140, 585, label);
    snprintf(label, sizeof(label), current ? "t=%.5g s   I=%.5g A" : "t=%.5g s   V=%.5g V",
             trace->samples[selected].time, trace->samples[selected].value);
    text(font, 856, 612, label);
}

/* Kind of quantity drawn: 0 terminal voltage, 1 part current, 2 wire current (as trace_title). */
static int live_graph_quantity(const SimLiveProbe *probe)
{
    if (probe->kind == SIM_LIVE_PROBE_WIRE_CURRENT)
        return 2;
    return probe->kind == SIM_LIVE_PROBE_PART_CURRENT;
}

/* First stored sample inside the last `span` seconds. */
static int live_graph_first_visible(const SimLiveGraph *graph, double span, double *first_time, double *last_time)
{
    double value;
    sim_live_graph_sample(graph, graph->count - 1, last_time, &value);
    /* Until a full window exists the graph fills from t = 0, like a scope sweep. */
    *first_time = fmax(*last_time - span, 0);
    int first = graph->count - 1;
    while (first > 0)
    {
        double time;
        sim_live_graph_sample(graph, first - 1, &time, &value);
        if (time < *first_time)
            break;
        first--;
    }
    return first;
}

static void live_graph_range(const SimLiveGraph *graph, int first, double *low, double *high)
{
    double time;
    sim_live_graph_sample(graph, first, &time, low);
    *high = *low;
    for (int i = first + 1; i < graph->count; ++i)
    {
        double value;
        sim_live_graph_sample(graph, i, &time, &value);
        *low = fmin(*low, value);
        *high = fmax(*high, value);
    }
}

static void live_graph_curve(const SimLiveGraph *graph, int first, double first_time, double span, double low,
                             double high)
{
    const double minimum_half_range = 1e-20;
    double mid = low * .5 + high * .5, half_range = high * .5 - low * .5;
    if (half_range < minimum_half_range)
        half_range = 1.0;
    color(.12f, .47f, .7f);
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    for (int i = first; i < graph->count; ++i)
    {
        double time, value;
        sim_live_graph_sample(graph, i, &time, &value);
        double x = (time - first_time) / span;
        double y = (value * .5 - mid * .5) / half_range + .5;
        glVertex2f((float)(918 + 302 * x), (float)(565 - 134 * y));
    }
    glEnd();
    glLineWidth(1);
}

/* Last "duration" seconds of the running simulation, newest sample at the right edge. */
static void live_graph_draw(const AppState *app, GLuint font)
{
    const SimLiveGraph *graph = &app->live_graph;
    int quantity = live_graph_quantity(&graph->probe);
    char kind[100], label[160];
    trace_title(kind, sizeof(kind), graph->probe.target.pin, quantity);
    snprintf(label, sizeof(label), tr(TXT_UI_GRAPH_LIVE), kind);
    graph_frame(font, label);
    color(.14f, .22f, .28f);
    if (!graph->valid || !graph->count)
    {
        TextId note = graph->valid ? TXT_UI_GRAPH_LIVE_WAITING : TXT_UI_GRAPH_LIVE_NO_READING;
        name_text(font, 930, 500, tr(note), 290);
        return;
    }
    double span = app->transient_stop, first_time, last_time, low, high, newest;
    int first = live_graph_first_visible(graph, span, &first_time, &last_time);
    live_graph_range(graph, first, &low, &high);
    const char *unit = quantity ? "A" : "V";
    snprintf(label, sizeof(label), "%.4g %s", high, unit);
    text(font, 856, 433, label);
    snprintf(label, sizeof(label), "%.3g %s", low, unit);
    text(font, 856, 572, label);
    live_graph_curve(graph, first, first_time, span, low, high);
    color(.14f, .22f, .28f);
    snprintf(label, sizeof(label), "%.4g s", first_time);
    text(font, 918, 585, label);
    snprintf(label, sizeof(label), "%.4g s", first_time + span);
    text(font, 1140, 585, label);
    sim_live_graph_sample(graph, graph->count - 1, &last_time, &newest);
    snprintf(label, sizeof(label), quantity ? "t=%.5g s   I=%.5g A" : "t=%.5g s   V=%.5g V", last_time, newest);
    text(font, 856, 612, label);
}

static void draw_part_board(const Part *p, int selected)
{
    float x = p->x, y = p->y;
    float extra = p->type == PART_BREADBOARD_WIDE ? 210 : 0;
    color(.67f, .70f, .72f);
    rect(x - 224 - extra, y - 108, 454 + 2 * extra, 224);
    color(.96f, .96f, .93f);
    rect(x - 227 - extra, y - 112, 454 + 2 * extra, 224);
    color(.80f, .19f, .17f);
    rect(x - 215 - extra, y - 109, 430 + 2 * extra, 2);
    rect(x - 215 - extra, y + 68, 430 + 2 * extra, 2);
    color(.16f, .31f, .47f);
    rect(x - 215 - extra, y - 72, 430 + 2 * extra, 2);
    rect(x - 215 - extra, y + 107, 430 + 2 * extra, 2);
    color(.84f, .85f, .82f);
    rect(x - 218 - extra, y - 6, 436 + 2 * extra, 10);
    if (selected)
    {
        color(.12f, .49f, .84f);
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x - 230 - extra, y - 115);
        glVertex2f(x + 230 + extra, y - 115);
        glVertex2f(x + 230 + extra, y + 115);
        glVertex2f(x - 230 - extra, y + 115);
        glEnd();
        glLineWidth(1);
    }
}

static void ellipse(float x, float y, float radius_x, float radius_y)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(radius_x, radius_y, 1);
    circle(0, 0, 1);
    glPopMatrix();
}

static SurfaceColor resistor_band(int digit)
{
    static const SurfaceColor colors[] = {{.08f,.08f,.08f}, {.38f,.18f,.08f}, {.8f,.08f,.05f},
        {1,.38f,.03f}, {.95f,.77f,.02f}, {.08f,.46f,.16f}, {.06f,.28f,.75f}, {.48f,.13f,.63f},
        {.44f,.46f,.47f}, {.94f,.94f,.9f}};
    if (digit == -1)
        return (SurfaceColor){.65f,.68f,.7f};
    if (digit == -2)
        return (SurfaceColor){.72f,.53f,.17f};
    return colors[digit];
}

/* Set while a mirrored part body is drawn. Digits, screens and labels drawn between readable_begin and
 * readable_end are flipped back around center_x, so they never read backwards. */
static int drawing_mirrored;

static void readable_begin(float center_x)
{
    glPushMatrix();
    if (!drawing_mirrored)
        return;
    glTranslatef(center_x, 0, 0);
    glScalef(-1, 1, 1);
    glTranslatef(-center_x, 0, 0);
}

static void readable_end(void)
{
    glPopMatrix();
}

/* Seven-segment digit (segments a..g, bit 0 = a). Shared by the supply display and future displays. */
static void seven_segment_digit(float x, float y, float height, int digit, int lit)
{
    static const unsigned char masks[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    const float w = height * .5f, t = height * .12f, half = height / 2;
    const float segments[7][4] = {{t, 0, w - 2 * t, t},        {w - t, t, t, half - t * 1.5f},
                                  {w - t, half + t / 2, t, half - t * 1.5f}, {t, height - t, w - 2 * t, t},
                                  {0, half + t / 2, t, half - t * 1.5f},     {0, t, t, half - t * 1.5f},
                                  {t, half - t / 2, w - 2 * t, t}};
    unsigned char mask = digit >= 0 && digit <= 9 ? masks[digit] : 0;
    for (int segment = 0; segment < 7; ++segment)
    {
        int on = lit && (mask >> segment & 1);
        glColor3f(on ? .40f : .10f, on ? .98f : .19f, on ? .58f : .14f);
        rect(x + segments[segment][0], y + segments[segment][1], segments[segment][2], segments[segment][3]);
    }
}

static void binding_post(float x, float y, float r, float g, float b)
{
    glColor3f(r * .55f, g * .55f, b * .55f);
    circle(x, y + 1, 6);
    glColor3f(r, g, b);
    circle(x, y, 5.5f);
    glColor3f(.78f, .78f, .74f);
    circle(x, y, 2.2f);
    glColor3f(fminf(1, r + .3f), fminf(1, g + .3f), fminf(1, b + .3f));
    circle(x - 2, y - 2, 1.4f);
}

/* Bench supply: display shows the set voltage only while the output is on. */
/* Front-panel knob; the pointer turns 270 degrees from minimum to maximum. */
static void source_knob(float x, float y, double fraction)
{
    glColor3f(.16f, .17f, .19f);
    circle(x, y, SOURCE_KNOB_RADIUS + 1.5f);
    glColor3f(.58f, .60f, .62f);
    circle(x, y, SOURCE_KNOB_RADIUS);
    const double half_turn = 3.14159265358979;
    float angle = (float)(half_turn * 1.25 - fmin(1, fmax(0, fraction)) * half_turn * 1.5);
    glColor3f(.20f, .21f, .23f);
    line(x, y, x + cosf(angle) * SOURCE_KNOB_RADIUS, y - sinf(angle) * SOURCE_KNOB_RADIUS);
}

static void draw_part_source(const Part *p, int current_limited)
{
    float x = p->x, y = p->y;
    glColor3f(.10f, .11f, .13f);
    rounded_rect(x - 40, y - 25, 82, 52, 7);
    glColor3f(.30f, .33f, .36f);
    rounded_rect(x - 41, y - 27, 82, 50, 7);
    gradient_rect(x - 37, y - 23, 74, 42, (SurfaceColor){.36f, .39f, .42f}, (SurfaceColor){.24f, .27f, .30f});
    glColor3f(.05f, .08f, .07f);
    rounded_rect(x - 33, y - 20, 42, 19, 3);
    double volts = fmin(99.9, fmax(0, p->value));
    int tenths = (int)lround(volts * 10);
    const float display_center = -12;
    readable_begin(x + display_center);
    seven_segment_digit(x - 29, y - 17, 13, tenths >= 100 ? tenths / 100 % 10 : -1, p->on);
    seven_segment_digit(x - 20, y - 17, 13, tenths / 10 % 10, p->on);
    glColor3f(p->on ? .40f : .10f, p->on ? .98f : .19f, p->on ? .58f : .14f);
    rect(x - 12.5f, y - 5.5f, 1.6f, 1.6f);
    seven_segment_digit(x - 9, y - 17, 13, tenths % 10, p->on);
    readable_end();
    source_knob(x + SOURCE_VOLTAGE_KNOB_X, y + SOURCE_KNOB_Y, p->value / SOURCE_VOLTAGE_MAX);
    source_knob(x + SOURCE_CURRENT_KNOB_X, y + SOURCE_KNOB_Y, p->control);
    binding_post(x - 21, y + 10, .82f, .12f, .10f);
    binding_post(x - 3, y + 10, .10f, .10f, .11f);
    /* CV lamp under the voltage knob, CC lamp under the current knob, as on a lab supply. */
    int limited = p->on && current_limited;
    glColor3f(.30f, p->on && !limited ? .92f : .33f, p->on && !limited ? .40f : .34f);
    circle(x + SOURCE_VOLTAGE_KNOB_X, y + 10, 2.6f);
    glColor3f(limited ? .95f : .33f, limited ? .22f : .33f, limited ? .16f : .34f);
    circle(x + SOURCE_CURRENT_KNOB_X, y + 10, 2.6f);
}

/* Slide switch: metal frame over a black housing; the actuator travels to the closed side. */
static void draw_part_switch(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.08f, .09f, .10f);
    rounded_rect(x - 30, y - 15, 60, 34, 4);
    gradient_rect(x - 29, y - 19, 58, 28, (SurfaceColor){.86f, .88f, .89f}, (SurfaceColor){.52f, .55f, .58f});
    glColor3f(.38f, .41f, .44f);
    circle(x - 24, y - 5, 1.8f);
    circle(x + 24, y - 5, 1.8f);
    glColor3f(.07f, .08f, .09f);
    rounded_rect(x - 17, y - 11, 34, 12, 3);
    float actuator = x + (p->on ? 7 : -7);
    glColor3f(.05f, .05f, .06f);
    rounded_rect(actuator - 7, y - 21, 14, 20, 3);
    gradient_rect(actuator - 6, y - 20, 12, 17, (SurfaceColor){.32f, .34f, .36f}, (SurfaceColor){.12f, .13f, .14f});
    glColor3f(.50f, .53f, .56f);
    for (int ridge = 0; ridge < 3; ++ridge)
        rect(actuator - 4 + ridge * 3.5f, y - 17, 1, 11);
    glColor3f(p->on ? .16f : .45f, p->on ? .66f : .13f, .12f);
    rounded_rect(x - 4, y + 11, 8, 3, 1.5f);
}

static void draw_part_clock(const Part *p)
{
    float x = p->x, y = p->y;
    color(.18f, .23f, .28f);
    rect(x - 34, y - 24, 68, 48);
    color(.25f, .43f, .51f);
    rect(x - 31, y - 26, 62, 44);
    color(.85f, .95f, .92f);
    line(x - 23, y + 8, x - 13, y + 8);
    line(x - 13, y + 8, x - 13, y - 8);
    line(x - 13, y - 8, x - 2, y - 8);
    line(x - 2, y - 8, x - 2, y + 8);
    line(x - 2, y + 8, x + 9, y + 8);
    line(x + 9, y + 8, x + 9, y - 8);
    line(x + 9, y - 8, x + 20, y - 8);
    color(p->on ? .25f : .55f, p->on ? .9f : .55f, .34f);
    circle(x + 23, y - 17, 3);
}

static void draw_part_chip(const Part *p)
{
    float x = p->x, y = p->y;
    int side = bench_pin_count(p->type) / 2;
    float half = (float)((side - 1) * BENCH_DIP_PITCH / 2), body = half + 7;
    color(.72f, .74f, .76f);
    for (int leg = 0; leg < side; ++leg)
    {
        rect(x - half - 2 + (float)(leg * BENCH_DIP_PITCH), y - 17, 4, 9);
        rect(x - half - 2 + (float)(leg * BENCH_DIP_PITCH), y + 8, 4, 9);
    }
    color(.14f, .16f, .18f);
    rect(x - body, y - 13, 2 * body, 28);
    color(.24f, .27f, .30f);
    rect(x - body, y - 16, 2 * body, 26);
    color(.72f, .76f, .78f);
    circle(x - body + 9, y - 8, 3);
}

static void draw_part_gate(const Part *p)
{
    float x = p->x, y = p->y;
    color(.16f, .19f, .22f);
    rect(x - 30, y - 24, 62, 50);
    color(.25f, .29f, .33f);
    rect(x - 30, y - 27, 60, 48);
    color(.84f, .89f, .92f);
    if (p->type == PART_DFF)
    {
        line(x - 17, y - 15, x + 17, y - 15);
        line(x + 17, y - 15, x + 17, y + 15);
        line(x + 17, y + 15, x - 17, y + 15);
        line(x - 17, y + 15, x - 17, y - 15);
        line(x - 17, y + 6, x - 9, y + 11);
        line(x - 9, y + 11, x - 17, y + 15);
    }
    else if (p->type == PART_NOT)
    {
        line(x - 16, y - 13, x - 16, y + 13);
        line(x - 16, y - 13, x + 18, y);
        line(x + 18, y, x - 16, y + 13);
    }
    else if (p->type == PART_OR || p->type == PART_XOR || p->type == PART_NOR || p->type == PART_XNOR)
    {
        line(x - 19, y - 13, x - 13, y);
        line(x - 13, y, x - 19, y + 13);
        line(x - 19, y - 13, x + 2, y - 11);
        line(x + 2, y - 11, x + 20, y);
        line(x + 20, y, x + 2, y + 11);
        line(x + 2, y + 11, x - 19, y + 13);
        if (p->type == PART_XOR || p->type == PART_XNOR)
        {
            line(x - 24, y - 13, x - 18, y);
            line(x - 18, y, x - 24, y + 13);
        }
    }
    else
    {
        line(x - 19, y - 13, x - 19, y + 13);
        line(x - 19, y - 13, x + 2, y - 13);
        line(x - 19, y + 13, x + 2, y + 13);
        line(x + 2, y - 13, x + 14, y - 8);
        line(x + 14, y - 8, x + 20, y);
        line(x + 20, y, x + 14, y + 8);
        line(x + 14, y + 8, x + 2, y + 13);
    }
    if (p->type == PART_NAND || p->type == PART_NOT || p->type == PART_NOR || p->type == PART_XNOR)
    {
        color(.84f, .89f, .92f);
        circle(x + 23, y, 3);
    }
}

static void draw_part_resistor(const Part *p)
{
    float x = p->x, y = p->y;
    color(.65f, .48f, .28f);
    ellipse(x - 16, y, 5, 8);
    ellipse(x + 16, y, 5, 8);
    gradient_rect(x - 16, y - 8, 32, 16, (SurfaceColor){.94f,.84f,.64f}, (SurfaceColor){.65f,.49f,.29f});
    if (!isfinite(p->value) || p->value <= 0)
        return;
    int exponent = (int)floor(log10(p->value)) - 1;
    int digits = (int)lround(p->value / pow(10, exponent));
    if (digits == 100)
    {
        digits = 10;
        ++exponent;
    }
    if (exponent < -2 || exponent > 9 || digits < 10 || digits > 99)
        return;
    int codes[] = {digits / 10, digits % 10, exponent, -2};
    const float positions[] = {-11, -4, 3, 12};
    for (int band = 0; band < 4; ++band)
    {
        SurfaceColor top = resistor_band(codes[band]);
        SurfaceColor bottom = {top.r * .7f, top.g * .7f, top.b * .7f};
        gradient_rect(x + positions[band], y - 8, 3, 16, top, bottom);
    }
}

/* Ceramic disc: non-polar, so there is no stripe; the dipped coating flares where the leads exit. */
static void draw_part_capacitor(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.62f, .36f, .10f);
    rounded_rect(x - 25, y - 3, 8, 6, 3);
    rounded_rect(x + 17, y - 3, 8, 6, 3);
    glColor3f(.46f, .25f, .06f);
    ellipse(x + 1, y + 2, 21, 20);
    glColor3f(.84f, .52f, .15f);
    ellipse(x, y, 20, 19);
    glColor3f(.91f, .64f, .26f);
    ellipse(x - 3, y - 4, 14, 12);
    glColor3f(.97f, .80f, .50f);
    ellipse(x - 8, y - 9, 4.5f, 3);
    glColor3f(.52f, .29f, .07f);
    for (int mark = 0; mark < 3; ++mark)
        rounded_rect(x - 8 + mark * 6, y + 6, 4, 6, 1.5f);
}

static void draw_part_inductor(const Part *p)
{
    float x = p->x, y = p->y;
    color(.16f, .18f, .19f);
    rect(x - 28, y - 10, 56, 20);
    ellipse(x - 28, y, 6, 10);
    ellipse(x + 28, y, 6, 10);
    for (int winding = 0; winding < 7; ++winding)
    {
        float center = x - 24 + winding * 8;
        glBegin(GL_QUAD_STRIP);
        for (int step = 0; step <= 32; ++step)
        {
            float angle = (float)(step * 6.283185307179586 / 32);
            float light = .65f + .25f * cosf(angle - .8f);
            color(light, light * .57f, light * .25f);
            glVertex2f(center + cosf(angle) * 4, y + sinf(angle) * 14);
            glVertex2f(center + cosf(angle) * 2, y + sinf(angle) * 11);
        }
        glEnd();
    }
}

static void draw_part_led(const Part *p, int led, float brightness)
{
    float x = p->x, y = p->y;
    /* The leads follow package rotation; the standing lens keeps its projection. */
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef((drawing_mirrored ? 1 : -1) *
                  (p->rotation * 90.0f + p->rotation_diagonal * 45.0f), 0, 0, 1);
    glTranslatef(-x, -y, 0);
    /* Unlit lens is much darker than the lit one, so a change of state is obvious at a glance. */
    const float unlit_share = .28f, rim_share = .18f, white_unlit = .40f;
    const float *lit = palette_tone(p->states < COMPONENT_LED_COLORS ? (int)p->states : 0)->lit;
    int white = p->states == BENCH_COLOR_WHITE;
    float tone[3][3];
    for (int c = 0; c < 3; ++c)
    {
        tone[1][c] = white ? white_unlit + .02f * c : lit[c] * unlit_share;
        tone[0][c] = white ? white_unlit * .7f : lit[c] * rim_share;
        tone[2][c] = lit[c];
    }
    glColor3f(tone[0][0], tone[0][1], tone[0][2]);
    ellipse(x, y + 9, 17, 5);
    rect(x - 14, y - 12, 28, 21);
    glColor3f(tone[1][0] * 1.15f, tone[1][1] * 1.15f, tone[1][2] * 1.15f);
    ellipse(x, y + 8, 16, 4);
    /* led: -1 unknown, 0 off, 1 lit, 2 burnt */
    if (led == 2)
    {
        color(.22f, .14f, .10f);
        circle(x, y - 14, 13);
        rect(x - 13, y - 14, 26, 23);
        color(.40f, .30f, .24f);
        circle(x - 4, y - 18, 3);
    }
    else
    {
        float glow = led == 1 ? .75f + .25f * brightness : 0;
        glColor3f(tone[1][0] + (tone[2][0] - tone[1][0]) * glow, tone[1][1] + (tone[2][1] - tone[1][1]) * glow,
                  tone[1][2] + (tone[2][2] - tone[1][2]) * glow);
        circle(x, y - 14, 13);
        rect(x - 13, y - 14, 26, 23);
        const float shine = .12f + .45f * glow;
        glColor3f(fminf(1, tone[1][0] + shine), fminf(1, tone[1][1] + shine), fminf(1, tone[1][2] + shine));
        circle(x - 4, y - 18, 4);
        rect(x - 8, y - 17, 3, 14);
    }
    glPopMatrix();
}

static void draw_polarity(float x, float y, int positive)
{
    color(.14f, .16f, .18f);
    rect(x - 3, y - .7f, 6, 1.4f);
    if (positive)
        rect(x - .7f, y - 3, 1.4f, 6);
}

static void voltage_mark_strokes(float x, float y, int digit, int decimal);

/* Printed voltage label ("9V", "1.5V", "3V") kept readable on mirrored batteries. */
static void voltage_mark(float x, float y, int digit, int decimal)
{
    const float label_center = decimal ? 14 : 8;
    readable_begin(x + label_center);
    voltage_mark_strokes(x, y, digit, decimal);
    readable_end();
}

static void voltage_mark_strokes(float x, float y, int digit, int decimal)
{
    static const unsigned char segments[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    unsigned char mask = segments[digit];
    const float bars[7][4] = {{1,0,4,1}, {5,1,1,4}, {5,6,1,4}, {1,10,4,1},
                             {0,6,1,4}, {0,1,1,4}, {1,5,4,1}};
    color(.18f,.20f,.22f);
    for (int i = 0; i < 7; ++i)
        if (mask & (1u << i))
            rect(x + bars[i][0], y + bars[i][1], bars[i][2], bars[i][3]);
    if (decimal)
    {
        rect(x + 8, y + 9, 1, 1);
        voltage_mark_strokes(x + 12, y, 5, 0);
        return;
    }
    glLineWidth(fmaxf(1, drawing_scale));
    line(x + 10, y, x + 13, y + 10);
    line(x + 13, y + 10, x + 16, y);
    glLineWidth(1);
}

static void draw_battery(const Part *part)
{
    float x = part->x, y = part->y;
    if (part->type == PART_BATTERY_9V)
    {
        color(.17f, .18f, .19f);
        rect(x - 25, y - 36, 50, 81);
        gradient_rect(x - 25, y - 36, 46, 78, (SurfaceColor){.84f,.86f,.87f}, (SurfaceColor){.43f,.46f,.48f});
        gradient_rect(x - 25, y - 38, 46, 24, (SurfaceColor){.23f,.25f,.27f}, (SurfaceColor){.06f,.07f,.08f});
        color(.16f,.18f,.20f);
        ellipse(x - 13, y - 39, 9, 5);
        color(.68f,.72f,.75f);
        ellipse(x - 13, y - 42, 9, 5);
        color(.27f,.30f,.33f);
        ellipse(x - 13, y - 42, 5, 3);
        color(.82f,.85f,.87f);
        ellipse(x + 13, y - 43, 5, 3);
        rect(x + 8, y - 43, 10, 5);
        color(.94f,.95f,.96f);
        ellipse(x + 13, y - 44, 5, 2);
        draw_polarity(x + 13, y - 6, 1);
        draw_polarity(x - 13, y - 6, 0);
        voltage_mark(x - 9, y + 4, 9, 0);
        color(.74f,.13f,.10f);
        rect(x - 25, y + 22, 46, 9);
    }
    else if (part->type == PART_BATTERY_AA)
    {
        color(.40f,.43f,.45f);
        ellipse(x, y + 42, 13, 4);
        gradient_rect(x - 13, y - 40, 26, 82, (SurfaceColor){.69f,.71f,.72f}, (SurfaceColor){.21f,.24f,.27f});
        gradient_rect(x - 10, y - 34, 7, 67, (SurfaceColor){.94f,.95f,.96f}, (SurfaceColor){.63f,.66f,.68f});
        color(.13f,.15f,.17f);
        rect(x - 13, y - 40, 26, 18);
        color(.84f,.86f,.88f);
        ellipse(x, y - 40, 13, 4);
        rect(x - 4, y - 46, 8, 5);
        color(.95f,.96f,.97f);
        ellipse(x, y - 46, 4, 2);
        draw_polarity(x, y - 12, 1);
        glPushMatrix();
        glTranslatef(x - 10, y + 1, 0);
        glScalef(.65f, .65f, 1);
        voltage_mark(0, 0, 1, 1);
        glPopMatrix();
        draw_polarity(x, y + 31, 0);
    }
    else
    {
        color(.30f,.33f,.36f);
        ellipse(x, y + 3, 27, 24);
        color(.72f,.75f,.78f);
        ellipse(x, y - 1, 27, 24);
        color(.91f,.93f,.94f);
        ellipse(x - 1, y - 3, 24, 21);
        color(.76f,.79f,.81f);
        ellipse(x, y - 2, 21, 18);
        draw_polarity(x, y - 8, 1);
        voltage_mark(x - 9, y + 1, 3, 0);
    }
}

static void draw_transistor(const Part *part)
{
    float x = part->x, y = part->y;
    color(.08f,.09f,.10f);
    ellipse(x, y - 10, 19, 12);
    rect(x - 19, y - 10, 38, 22);
    gradient_rect(x - 18, y - 7, 36, 19, (SurfaceColor){.29f,.31f,.33f}, (SurfaceColor){.09f,.10f,.11f});
    color(.53f,.55f,.56f);
    rect(x - 12, y - 3, 12, 1);
    rect(x - 12, y + 1, 20, 1);
    rect(x - 12, y + 5, part->type == PART_NPN ? 15 : 9, 1);
    if (part->type == PART_TEMPERATURE)
    {
        color(.22f,.55f,.75f);
        circle(x + 11, y - 2, 2);
    }
}

/* TO-220: black body with three legs below, metal tab with its mounting hole above. */
static void draw_regulator(const Part *part)
{
    float x = part->x, y = part->y;
    gradient_rect(x - 20, y - 36, 40, 18, (SurfaceColor){.80f, .82f, .84f}, (SurfaceColor){.55f, .58f, .60f});
    color(.30f, .32f, .34f);
    circle(x, y - 27, 4.5f);
    color(.08f, .09f, .10f);
    rect(x - 20, y - 18, 40, 30);
    gradient_rect(x - 19, y - 17, 38, 27, (SurfaceColor){.24f, .26f, .28f}, (SurfaceColor){.09f, .10f, .11f});
    color(.55f, .58f, .60f);
    rect(x - 12, y - 8, 24, 1);
    rect(x - 12, y - 3, 16, 1);
}

static void draw_sensor(const Part *part)
{
    float x = part->x, y = part->y;
    if (part->type == PART_SOIL)
    {
        gradient_rect(x - 18, y - 39, 36, 47, (SurfaceColor){.12f,.40f,.26f}, (SurfaceColor){.04f,.22f,.13f});
        color(.07f,.23f,.14f);
        rect(x - 18, y + 8, 12, 25);
        rect(x + 6, y + 8, 12, 25);
        color(.76f,.66f,.35f);
        for (int trace = 0; trace < 3; ++trace)
        {
            rect(x - 16 + trace * 3, y + 2, 1, 29);
            rect(x + 8 + trace * 3, y + 2, 1, 29);
        }
        color(.78f,.81f,.82f);
        rect(x - 10, y - 30, 20, 8);
        color(.18f,.19f,.20f);
        rect(x - 4, y - 18, 8, 12);
    }
    else if (part->type == PART_FLEX || part->type == PART_FORCE)
    {
        color(.44f,.30f,.10f);
        rect(x - 7, y - 22, 14, 44);
        gradient_rect(x - 6, y - 40, 12, 60, (SurfaceColor){.82f,.61f,.23f}, (SurfaceColor){.48f,.33f,.11f});
        color(.18f,.18f,.17f);
        rect(x - 4, y - 36, 8, 52);
        if (part->type == PART_FORCE)
        {
            color(.79f,.60f,.24f);
            circle(x, y - 20, 20);
            color(.17f,.18f,.18f);
            circle(x, y - 20, 17);
            color(.25f,.26f,.25f);
            circle(x - 2, y - 23, 13);
        }
        else
        {
            color(.45f,.47f,.44f);
            for (int i = 0; i < 8; ++i)
                rect(x - 3, y - 32 + i * 6, 6, 2);
        }
    }
    else if (part->type == PART_TILT)
    {
        gradient_rect(x - 13, y - 29, 26, 47, (SurfaceColor){.49f,.64f,.62f}, (SurfaceColor){.13f,.29f,.27f});
        color(.72f,.81f,.78f);
        ellipse(x, y - 29, 13, 4);
        color(.82f,.84f,.82f);
        circle(x, y + (component_effective_on(part) ? 10 : -19), 6);
    }
    else if (part->type == PART_LDR)
    {
        color(.45f,.32f,.15f);
        ellipse(x, y - 3, 20, 20);
        color(.89f,.76f,.51f);
        ellipse(x, y - 6, 19, 18);
        color(.59f,.23f,.15f);
        for (int i = 0; i < 5; ++i)
        {
            float py = y - 17 + i * 5;
            rect(x - 11, py, 22, 2);
            if (i < 4)
                rect(x + (i % 2 ? -11 : 9), py, 2, 6);
        }
    }
    else if (part->type == PART_AMBIENT_LIGHT)
    {
        /* Water-clear dome: a cool rim keeps the unlit lens legible on the light bench. */
        glColor3f(.66f, .72f, .76f);
        rounded_rect(x - 13, y + 4, 26, 6, 2);
        glColor3f(.55f, .63f, .68f);
        ellipse(x, y - 6, 13, 16);
        rect(x - 13, y - 6, 26, 12);
        glColor3f(.80f, .86f, .90f);
        ellipse(x, y - 6, 12, 15);
        rect(x - 12, y - 6, 24, 12);
        glColor3f(.30f, .32f, .34f);
        rect(x - 4, y - 2, 8, 6);
        glColor3f(.62f, .64f, .66f);
        line(x + 2, y - 2, x + 6, y - 9);
        glColor3f(.97f, .99f, 1);
        ellipse(x - 5, y - 12, 2.5f, 6);
    }
    else
    {
        color(.16f,.18f,.20f);
        ellipse(x, y - 3, 15, 19);
        gradient_rect(x - 12, y - 17, 24, 29, (SurfaceColor){.33f,.22f,.28f}, (SurfaceColor){.09f,.10f,.12f});
        color(.62f,.57f,.62f);
        ellipse(x - 5, y - 14, 3, 7);
    }
}

/* ---- Multi-element devices: measurements come from the frame context set by the workspace pass. ---- */
typedef struct
{
    const double *elements;
    const float *scope;
    int scope_count;
    double motor_angle;
    const LcdScreen *lcd;
    int current_limited; /* bench supply in CC */
} DeviceFrame;
static DeviceFrame device_frame;

static double device_element(int index)
{
    return device_frame.elements ? device_frame.elements[index] : 0;
}

static float led_glow(double amperes)
{
    return (float)sqrt(fmin(1.0, fmax(0.0, amperes / SPICE_LED_VISUAL_FULL_SCALE_AMPERES)));
}

static void radial_pcb(float x, float y, float w, float h, float r, float g, float b)
{
    glColor3f(r * .6f, g * .6f, b * .6f);
    rounded_rect(x - w / 2 + 1, y - h / 2 + 2, w, h, 3);
    glColor3f(r, g, b);
    rounded_rect(x - w / 2, y - h / 2, w, h, 3);
    glColor3f(.82f, .70f, .30f);
    for (int hole = -1; hole <= 1; ++hole)
        circle(x + hole * 14.0f, y + h / 2 - 5, 2.4f);
}

static void draw_rgb_led(const Part *p)
{
    float x = p->x, y = p->y;
    float r = led_glow(device_element(0)), g = led_glow(device_element(1)), b = led_glow(device_element(2));
    float mix = fmaxf(r, fmaxf(g, b));
    if (mix > 0)
    {
        glow_blend_begin();
        glColor4f(r, g, b, .35f * mix);
        circle(x, y - 6, 22 + 10 * mix);
        glow_blend_end();
    }
    glColor3f(.53f, .59f, .64f);
    rounded_rect(x - 17, y + 10, 34, 6, 2);
    glColor3f(.55f, .63f, .68f);
    ellipse(x, y - 8, 16, 16);
    rect(x - 16, y - 8, 32, 18);
    glColor3f(.86f + .14f * r, .88f + .12f * g, .90f + .10f * b);
    if (mix > 0)
        glColor3f(.55f + .45f * r, .55f + .45f * g, .55f + .45f * b);
    ellipse(x, y - 8, 15, 15);
    rect(x - 15, y - 8, 30, 18);
    glColor3f(1, 1, 1);
    ellipse(x - 6, y - 14, 3, 6);
}

static void segment_shape(float x, float y, float h, int segment, int lit, float glow)
{
    const float w = h * .55f, t = h * .11f, half = h / 2;
    const float shapes[8][4] = {{t, 0, w - 2 * t, t},           {w - t, t, t, half - t * 1.5f},
                                {w - t, half + t / 2, t, half - t * 1.5f}, {t, h - t, w - 2 * t, t},
                                {0, half + t / 2, t, half - t * 1.5f},     {0, t, t, half - t * 1.5f},
                                {t, half - t / 2, w - 2 * t, t},           {w + t, h - t, t, t}};
    if (lit)
        glColor3f(.45f + .55f * glow, .05f + .10f * glow, .04f);
    else
        glColor3f(.24f, .10f, .10f);
    rect(x + shapes[segment][0], y + shapes[segment][1], shapes[segment][2], shapes[segment][3]);
}

static void draw_seven_segment(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.08f, .08f, .09f);
    rounded_rect(x - 30, y - 44, 60, 88, 4);
    glColor3f(.16f, .16f, .18f);
    rounded_rect(x - 26, y - 40, 52, 80, 3);
    readable_begin(x);
    for (int segment = 0; segment < COMPONENT_SEGMENTS; ++segment)
    {
        float glow = led_glow(device_element(segment));
        segment_shape(x - 15, y - 30, 58, segment, glow > .02f, glow);
    }
    readable_end();
}

static void draw_dip_switch(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.55f, .08f, .08f);
    rounded_rect(x - 30, y - 22, 60, 44, 3);
    glColor3f(.95f, .95f, .93f);
    for (int contact = 0; contact < COMPONENT_DIP_POSITIONS; ++contact)
    {
        float cx = component_device(PART_DIP_SWITCH)->pins[contact][0];
        glColor3f(.25f, .04f, .04f);
        rect(x + cx - 5, y - 12, 10, 22);
        glColor3f(.96f, .96f, .94f);
        rect(x + cx - 5, (p->states >> contact & 1u) ? y - 12 : y, 10, 10);
    }
    glColor3f(.95f, .95f, .93f);
    rect(x - 26, y - 19, 8, 3);
}

static void draw_dpst(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.10f, .10f, .11f);
    rounded_rect(x - 30, y - 20, 60, 40, 5);
    glColor3f(.30f, .31f, .33f);
    rounded_rect(x - 22, y - 14, 44, 28, 4);
    glColor3f(p->on ? .16f : .62f, p->on ? .62f : .18f, .16f);
    rounded_rect(x + (p->on ? 2 : -18), y - 11, 16, 22, 3);
    glColor3f(.82f, .84f, .86f);
    rect(x - 30, y - 11, 6, 2);
    rect(x - 30, y + 9, 6, 2);
    rect(x + 24, y - 11, 6, 2);
    rect(x + 24, y + 9, 6, 2);
}

static void draw_relay(const Part *p)
{
    float x = p->x, y = p->y;
    int closed = device_element(1) > .5;
    glColor3f(.05f, .18f, .42f);
    rounded_rect(x - 34, y - 30, 68, 60, 4);
    gradient_rect(x - 31, y - 27, 62, 50, (SurfaceColor){.15f, .38f, .78f}, (SurfaceColor){.08f, .24f, .56f});
    glColor3f(.88f, .92f, .98f);
    rect(x - 22, y - 18, 30, 2);
    rect(x - 22, y - 12, 22, 2);
    rect(x - 22, y - 6, 26, 2);
    glColor3f(closed ? .25f : .55f, closed ? .90f : .55f, .35f);
    circle(x + 20, y - 15, 4);
    glColor3f(.88f, .92f, .98f);
    glLineWidth(2);
    line(x - 10, y + 12, x + 12, closed ? y + 6 : y + 16);
    glLineWidth(1);
    circle(x - 12, y + 12, 2.5f);
}

static void draw_motor_can(float x, float y, float radius, float angle, float shaft)
{
    glColor3f(.40f, .42f, .45f);
    circle(x + 1, y + 2, radius);
    gradient_rect(x - radius, y - radius * .8f, radius * 2, radius * 1.6f, (SurfaceColor){.86f, .87f, .88f},
                  (SurfaceColor){.55f, .57f, .60f});
    glColor3f(.72f, .74f, .76f);
    circle(x, y, radius * .62f);
    glColor3f(.30f, .31f, .33f);
    circle(x, y, radius * .22f);
    glColor3f(.95f, .80f, .20f);
    glLineWidth(3);
    line(x, y, x + cosf(angle) * shaft, y + sinf(angle) * shaft);
    glLineWidth(1);
}

static void draw_dc_motor(const Part *p)
{
    draw_motor_can(p->x, p->y, 24, (float)device_frame.motor_angle, 20);
}

static void draw_vibration_motor(const Part *p)
{
    float x = p->x, y = p->y, angle = (float)device_frame.motor_angle, shake = device_element(0) > 50 ? 1.5f : 0;
    glColor3f(.62f, .64f, .66f);
    circle(x + shake * cosf(angle * 7), y, 20);
    glColor3f(.82f, .84f, .86f);
    circle(x + shake * cosf(angle * 7), y, 16);
    glColor3f(.30f, .31f, .33f);
    circle(x + cosf(angle) * 8, y + sinf(angle) * 8, 6);
}

static void draw_encoder_motor(const Part *p)
{
    float x = p->x, y = p->y;
    radial_pcb(x, y + 14, 80, 40, .10f, .42f, .22f);
    draw_motor_can(x - 6, y - 8, 22, (float)device_frame.motor_angle, 18);
    glColor3f(.10f, .10f, .11f);
    circle(x + 26, y - 8, 9);
    glColor3f(.85f, .86f, .87f);
    for (int slot = 0; slot < 6; ++slot)
    {
        float a = (float)device_frame.motor_angle + slot * 1.047f;
        circle(x + 26 + cosf(a) * 6, y - 8 + sinf(a) * 6, 1.3f);
    }
}

static void draw_servo(const Part *p)
{
    float x = p->x, y = p->y, angle = (float)(device_element(0) * 3.14159265 / 180.0);
    glColor3f(.05f, .22f, .55f);
    rounded_rect(x - 36, y - 22, 72, 44, 4);
    gradient_rect(x - 33, y - 19, 66, 38, (SurfaceColor){.20f, .48f, .90f}, (SurfaceColor){.10f, .32f, .70f});
    glColor3f(.08f, .25f, .62f);
    rect(x - 44, y - 6, 88, 10);
    glColor3f(.96f, .96f, .96f);
    circle(x - 12, y - 2, 9);
    glLineWidth(6);
    line(x - 12, y - 2, x - 12 - cosf(angle) * 28, y - 2 - sinf(angle) * 28);
    glLineWidth(1);
    glColor3f(.70f, .72f, .74f);
    circle(x - 12, y - 2, 2.5f);
}

static void draw_piezo(const Part *p)
{
    float x = p->x, y = p->y;
    if (fabs(device_element(0)) > 1e-5)
    {
        glColor3f(.40f, .60f, .90f);
        for (int ring = 1; ring <= 2; ++ring)
            for (int dot = -2; dot <= 2; ++dot)
                circle(x + 26 + ring * 6, y - 8 + dot * (4 + ring * 2), 1.4f);
    }
    glColor3f(.06f, .06f, .07f);
    circle(x, y - 6, 22);
    glColor3f(.16f, .16f, .18f);
    circle(x, y - 6, 18);
    glColor3f(.04f, .04f, .05f);
    circle(x, y - 6, 4);
}

static void draw_wave_icon(float x, float y, unsigned mode)
{
    glColor3f(.40f, .98f, .58f);
    glBegin(GL_LINE_STRIP);
    for (int step = 0; step <= 24; ++step)
    {
        float t = step / 24.0f, v;
        if (mode == 1)
            v = fmodf(t * 2, 1) < .5f ? -1 : 1;
        else if (mode == 2)
            v = 1 - 4 * fabsf(fmodf(t * 2, 1) - .5f);
        else
            v = sinf(t * 12.566f);
        glVertex2f(x + t * 36, y - v * 6);
    }
    glEnd();
}

static void draw_function_generator(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.12f, .13f, .15f);
    rounded_rect(x - 42, y - 30, 84, 60, 5);
    gradient_rect(x - 39, y - 27, 78, 54, (SurfaceColor){.42f, .45f, .48f}, (SurfaceColor){.28f, .30f, .33f});
    glColor3f(.04f, .07f, .06f);
    rounded_rect(x - 34, y - 22, 44, 22, 3);
    const float wave_center = -12;
    readable_begin(x + wave_center);
    draw_wave_icon(x - 30, y - 11, p->states);
    readable_end();
    glColor3f(.16f, .17f, .19f);
    circle(x + 24, y - 11, 8);
    glColor3f(.62f, .64f, .66f);
    circle(x + 24, y - 11, 6);
    binding_post(x - 20, y + 15, .82f, .12f, .10f);
    binding_post(x, y + 15, .10f, .10f, .11f);
    glColor3f(p->on ? .30f : .30f, p->on ? .92f : .33f, p->on ? .40f : .34f);
    circle(x + 24, y + 15, 2.6f);
}

static void draw_number(float x, float y, float h, double value, int decimals)
{
    char digits[16];
    snprintf(digits, sizeof(digits), "%*.*f", 5, decimals, fabs(value) > 9999 ? 9999.0 : fabs(value));
    float cursor = x;
    for (const char *c = digits; *c; ++c)
    {
        if (*c == '.')
        {
            glColor3f(.10f, .10f, .10f);
            rect(cursor - 2, y + h - 2, 2, 2);
            continue;
        }
        if (*c >= '0' && *c <= '9')
            seven_segment_digit(cursor, y, h, *c - '0', 1);
        cursor += h * .62f;
    }
}

static void draw_multimeter(const Part *p)
{
    float x = p->x, y = p->y;
    double reading = p->states == 1 ? device_element(1) * 1000 : p->states == 2 ? device_element(0) / 1e-3 : device_element(0);
    glColor3f(.55f, .45f, .05f);
    rounded_rect(x - 36, y - 52, 72, 104, 8);
    gradient_rect(x - 32, y - 48, 64, 96, (SurfaceColor){.98f, .80f, .15f}, (SurfaceColor){.88f, .66f, .08f});
    glColor3f(.62f, .70f, .58f);
    rounded_rect(x - 27, y - 42, 54, 24, 3);
    readable_begin(x);
    draw_number(x - 25, y - 38, 15, reading, p->states == 2 ? 0 : 2);
    readable_end();
    glColor3f(.15f, .15f, .16f);
    circle(x, y + 8, 16);
    glColor3f(.85f, .85f, .86f);
    float dial = -2.2f + p->states * .9f;
    glLineWidth(3);
    line(x, y + 8, x + cosf(dial) * 13, y + 8 + sinf(dial) * 13);
    glLineWidth(1);
    binding_post(x - 21, y + 38, .10f, .10f, .11f);
    binding_post(x, y + 38, .82f, .12f, .10f);
    binding_post(x + 21, y + 38, .82f, .12f, .10f);
}

static void draw_oscilloscope(const Part *p)
{
    float x = p->x, y = p->y;
    const float screen_w = 100, screen_h = 64, volts_per_screen = 10;
    glColor3f(.18f, .20f, .23f);
    rounded_rect(x - 70, y - 52, 140, 104, 6);
    /* Time runs left to right on the screen even when the instrument is mirrored. */
    readable_begin(x - 64 + screen_w / 2);
    glColor3f(.03f, .08f, .06f);
    rect(x - 64, y - 46, screen_w, screen_h);
    glColor3f(.08f, .22f, .16f);
    for (int grid = 1; grid < 4; ++grid)
    {
        rect(x - 64 + grid * screen_w / 4, y - 46, 1, screen_h);
        rect(x - 64, y - 46 + grid * screen_h / 4, screen_w, 1);
    }
    glColor3f(.40f, 1, .55f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < device_frame.scope_count; ++i)
    {
        float v = fmaxf(-1, fminf(1, device_frame.scope[i] / (volts_per_screen / 2)));
        glVertex2f(x - 64 + screen_w * i / (float)(COMPONENT_SCOPE_HISTORY - 1), y - 46 + screen_h / 2 - v * screen_h / 2);
    }
    glEnd();
    readable_end();
    glColor3f(.55f, .57f, .60f);
    for (int knob = 0; knob < 3; ++knob)
        circle(x + 52, y - 36 + knob * 22, 6);
    binding_post(x - 14, y + 34, .95f, .80f, .15f);
    binding_post(x + 14, y + 34, .10f, .10f, .11f);
}

static void draw_sensor_module(const Part *p)
{
    float x = p->x, y = p->y;
    if (p->type == PART_PIR)
    {
        radial_pcb(x, y + 8, 44, 40, .08f, .40f, .22f);
        glColor3f(.64f, .68f, .66f);
        circle(x, y - 6, 19);
        glColor3f(.95f, .95f, .93f);
        circle(x, y - 6, 18);
        glColor3f(.85f, .85f, .83f);
        for (int ring = 0; ring < 3; ++ring)
            circle(x - 6 + ring * 6, y - 8, 4);
    }
    else if (p->type == PART_GAS)
    {
        radial_pcb(x, y + 8, 48, 40, .08f, .28f, .62f);
        glColor3f(.62f, .64f, .66f);
        circle(x, y - 6, 17);
        glColor3f(.45f, .47f, .49f);
        for (int mesh = -2; mesh <= 2; ++mesh)
            rect(x - 14, y - 6 + mesh * 5.0f, 28, 1.4f);
    }
    else if (p->type == PART_IR_OBSTACLE)
    {
        radial_pcb(x, y + 8, 48, 40, .10f, .12f, .14f);
        glColor3f(.85f, .88f, .92f);
        circle(x - 10, y - 8, 7);
        glColor3f(.10f, .10f, .12f);
        circle(x + 10, y - 8, 7);
    }
    else
    {
        glColor3f(.10f, .10f, .11f);
        rounded_rect(x - 34, y - 14, 68, 28, 6);
        glColor3f(.30f, .32f, .36f);
        circle(x - 16, y, 8);
        circle(x + 16, y, 8);
        glColor3f(.70f, .20f, .25f);
        circle(x - 16, y, 4);
    }
}

/* ---- Microcontroller boards and the character LCD ---- */
/* Classic 5x7 character generator (columns, LSB at the top), ASCII 32..126. */
static const unsigned char lcd_font[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x01,0x01},{0x3E,0x41,0x41,0x51,0x32},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x7F,0x20,0x18,0x20,0x7F},
    {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},{0x00,0x00,0x7F,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20},{0x41,0x41,0x7F,0x00,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x08,0x14,0x54,0x54,0x3C},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x00,0x7F,0x10,0x28,0x44},
    {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x08,0x08,0x2A,0x1C,0x08}};

static void header_strip(float x, float y, int pins)
{
    glColor3f(.08f, .08f, .09f);
    rect(x - 7, y - 6, pins * 14.0f, 12);
    glColor3f(.30f, .30f, .32f);
    for (int pin = 0; pin < pins; ++pin)
        rect(x - 2 + pin * 14.0f, y - 2, 4, 4);
}

static void board_led(float x, float y, int lit, float r, float g, float b)
{
    glColor3f(lit ? r : r * .3f, lit ? g : g * .3f, lit ? b : b * .3f);
    rounded_rect(x - 4, y - 2.5f, 8, 5, 1.5f);
}

static int io_high(const Part *p, int io)
{
    return p->io_mode[io] == MCU_OUTPUT && p->io_level[io] > .5f;
}

static void draw_arduino_uno(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.02f, .26f, .34f);
    rounded_rect(x - 150, y - 76, 300, 156, 8);
    glColor3f(.04f, .40f, .50f);
    rounded_rect(x - 148, y - 78, 296, 152, 8);
    glColor3f(.72f, .74f, .76f);
    rect(x - 162, y - 50, 40, 34);
    glColor3f(.08f, .08f, .09f);
    rect(x - 160, y + 18, 30, 30);
    header_strip(x - 56, y - 92, 14);
    header_strip(x - 98, y + 92, 3);
    header_strip(x - 28, y + 92, 6);
    glColor3f(.10f, .11f, .12f);
    rect(x - 20, y + 14, 120, 26);
    glColor3f(.70f, .72f, .74f);
    for (int leg = 0; leg < 14; ++leg)
        rect(x - 17 + leg * 8.4f, y + 10, 3, 4);
    glColor3f(.85f, .86f, .87f);
    circle(x - 120, y - 62, 5);
    circle(x + 130, y - 62, 5);
    circle(x + 130, y + 62, 5);
    board_led(x + 60, y - 60, io_high(p, 13), 1, .62f, .10f);
    board_led(x + 92, y - 26, 1, .30f, .95f, .40f);
    glColor3f(.92f, .95f, .96f);
    rect(x - 100, y - 24, 60, 3);
    rect(x - 100, y - 16, 44, 3);
}

static void draw_esp32(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.07f, .07f, .08f);
    rounded_rect(x - 160, y - 46, 320, 92, 6);
    header_strip(x - 140, y - 58, 11);
    header_strip(x - 140, y + 58, 11);
    glColor3f(.72f, .74f, .76f);
    rounded_rect(x + 30, y - 32, 96, 64, 3);
    glColor3f(.60f, .62f, .64f);
    rect(x + 38, y - 24, 80, 48);
    glColor3f(.85f, .70f, .20f);
    glLineWidth(2);
    line(x + 130, y - 30, x + 150, y - 30);
    line(x + 150, y - 30, x + 150, y + 30);
    line(x + 150, y + 30, x + 136, y + 30);
    glLineWidth(1);
    glColor3f(.72f, .74f, .76f);
    rect(x - 174, y - 12, 20, 24);
    board_led(x - 120, y + 20, io_high(p, 0), .30f, .55f, 1);
    board_led(x - 120, y - 20, 1, 1, .20f, .15f);
}

static void draw_attiny(const Part *p)
{
    float x = p->x, y = p->y;
    glColor3f(.70f, .72f, .74f);
    for (int leg = 0; leg < 4; ++leg)
    {
        rect(x - 23 + leg * 14.0f, y - 22, 4, 9);
        rect(x - 23 + leg * 14.0f, y + 13, 4, 9);
    }
    glColor3f(.14f, .16f, .18f);
    rect(x - 30, y - 12, 60, 26);
    glColor3f(.24f, .27f, .30f);
    rect(x - 30, y - 14, 60, 24);
    glColor3f(.72f, .76f, .78f);
    circle(x - 22, y + 5, 2.5f);
    glColor3f(.14f, .16f, .18f);
    circle(x - 30, y - 2, 5);
}

static void lcd_character(float x, float y, unsigned char c, float pixel, int lit)
{
    if (c < 32 || c > 126)
        c = '?';
    for (int column = 0; column < 5; ++column)
        for (int row = 0; row < 7; ++row)
        {
            int on = lcd_font[c - 32][column] >> row & 1;
            if (on)
                glColor3f(lit ? .92f : .08f, lit ? .97f : .14f, lit ? 1 : .10f);
            else
                glColor3f(lit ? .14f : .46f, lit ? .30f : .56f, lit ? .80f : .20f);
            rect(x + column * pixel, y + row * pixel, pixel * .82f, pixel * .82f);
        }
}

static void draw_lcd(const Part *p)
{
    float x = p->x, y = p->y;
    const LcdScreen *screen = device_frame.lcd;
    int powered = device_element(0) > 4.0;
    int lit = powered && screen && screen->backlight;
    glColor3f(.04f, .30f, .12f);
    rounded_rect(x - 124, y - 46, 248, 92, 5);
    glColor3f(.85f, .86f, .87f);
    for (int hole = 0; hole < 4; ++hole)
        circle(x + (hole % 2 ? 116 : -116), y + (hole / 2 ? 38 : -38), 3.5f);
    const float screen_center = 2;
    readable_begin(x + screen_center);
    glColor3f(.06f, .07f, .08f);
    rect(x - 108, y - 34, 220, 68);
    glColor3f(lit ? .10f : .36f, lit ? .25f : .46f, lit ? .70f : .16f);
    rect(x - 102, y - 28, 208, 56);
    const float pixel = 2.0f, cell = 12.8f;
    for (int row = 0; row < LCD_ROWS; ++row)
        for (int column = 0; column < LCD_COLUMNS; ++column)
        {
            unsigned char c = powered && screen && screen->used ? (unsigned char)screen->text[row][column] : ' ';
            lcd_character(x - 100 + column * cell, y - 23 + row * 26.0f, c ? c : ' ', pixel, lit);
        }
    readable_end();
}

/* Rigid multi-pin packages: metal legs from each terminal to the nearest body edge. */
static void draw_device_legs(const Part *p)
{
    const ComponentDevice *device = component_device(p->type);
    if (bench_pin_count(p->type) <= 3 || p->type == PART_RGB_LED)
        return;
    glColor3f(.70f, .72f, .74f);
    for (int pin = 0; pin < bench_pin_count(p->type); ++pin)
    {
        float px = device->pins[pin][0], py = device->pins[pin][1];
        float ex = fmaxf(-device->half_width, fminf(device->half_width, px));
        float ey = fmaxf(-device->half_height, fminf(device->half_height, py));
        if (fabsf(py) > device->half_height)
            rect(p->x + px - 1.5f, p->y + fminf(py, ey), 3, fabsf(py - ey));
        else
            rect(p->x + fminf(px, ex), p->y + py - 1.5f, fabsf(px - ex), 3);
    }
}

static int draw_device(const Part *p)
{
    draw_device_legs(p);
    switch (p->type)
    {
    case PART_RGB_LED:
        draw_rgb_led(p);
        return 1;
    case PART_SEVEN_SEGMENT:
        draw_seven_segment(p);
        return 1;
    case PART_DIP_SWITCH:
        draw_dip_switch(p);
        return 1;
    case PART_DPST:
        draw_dpst(p);
        return 1;
    case PART_RELAY:
        draw_relay(p);
        return 1;
    case PART_DC_MOTOR:
        draw_dc_motor(p);
        return 1;
    case PART_VIBRATION_MOTOR:
        draw_vibration_motor(p);
        return 1;
    case PART_ENCODER_MOTOR:
        draw_encoder_motor(p);
        return 1;
    case PART_SERVO:
        draw_servo(p);
        return 1;
    case PART_PIEZO:
        draw_piezo(p);
        return 1;
    case PART_FUNCTION_GENERATOR:
        draw_function_generator(p);
        return 1;
    case PART_MULTIMETER:
        draw_multimeter(p);
        return 1;
    case PART_OSCILLOSCOPE:
        draw_oscilloscope(p);
        return 1;
    case PART_ARDUINO_UNO:
        draw_arduino_uno(p);
        return 1;
    case PART_ESP32:
        draw_esp32(p);
        return 1;
    case PART_ATTINY85:
        draw_attiny(p);
        return 1;
    case PART_LCD_I2C:
        draw_lcd(p);
        return 1;
    case PART_PIR:
    case PART_GAS:
    case PART_IR_OBSTACLE:
    case PART_DISTANCE:
        draw_sensor_module(p);
        return 1;
    default:
        return 0;
    }
}

/* Indicator lamp on a socket: glass tint, filament color and halo follow the measured current. */
static void draw_lamp(const Part *part, float brightness)
{
    float x = part->x, y = part->y, glow = fminf(1, fmaxf(0, brightness));
    glColor3f(.44f, .46f, .48f);
    rounded_rect(x - 30, y - 4, 60, 10, 3);
    gradient_rect(x - 14, y - 10, 28, 16, (SurfaceColor){.82f, .80f, .70f}, (SurfaceColor){.50f, .48f, .40f});
    glColor3f(.40f, .38f, .32f);
    for (int ridge = 0; ridge < 3; ++ridge)
        rect(x - 14, y - 7 + ridge * 5, 28, 1.2f);
    if (glow > 0)
    {
        glow_blend_begin();
        glColor4f(1, .86f, .40f, .35f * glow);
        circle(x, y - 30, 22 + 14 * glow);
        glow_blend_end();
    }
    glColor3f(.78f + .2f * glow, .84f + .12f * glow, .88f - .25f * glow);
    circle(x, y - 30, 19);
    rect(x - 9, y - 18, 18, 9);
    glColor3f(.97f, .99f, 1);
    ellipse(x - 8, y - 38, 3, 6);
    glColor3f(.35f + .65f * glow, .30f + .55f * glow, .25f);
    glLineWidth(1.5f);
    line(x - 5, y - 10, x - 5, y - 28);
    line(x + 5, y - 10, x + 5, y - 28);
    line(x - 5, y - 28, x - 2, y - 33);
    line(x - 2, y - 33, x + 2, y - 27);
    line(x + 2, y - 27, x + 5, y - 28);
    glLineWidth(1);
}

static void draw_diode(const Part *part)
{
    float x = part->x, y = part->y;
    color(.12f,.13f,.14f);
    ellipse(x - 18, y, 3, 8);
    ellipse(x + 18, y, 3, 8);
    gradient_rect(x - 18, y - 8, 36, 16, (SurfaceColor){.36f,.37f,.38f}, (SurfaceColor){.06f,.07f,.08f});
    gradient_rect(x + 11, y - 8, 5, 16, (SurfaceColor){.91f,.92f,.93f}, (SurfaceColor){.54f,.57f,.60f});
}

/* Mostly flat paper, with a small lifted corner and a soft cast shadow. */
static void draw_note_paper(const Part *part, float width, float height)
{
    float x = part->x - width / 2, y = part->y - height / 2;
    const float *bg = palette_tone(((part->states & 255) + BENCH_COLOR_YELLOW) % BENCH_COLOR_COUNT)->wire;
    ui_shadow((AppRect){x + 1, y + 2, width - 2, height - 2}, 2, 2);
    ui_shadow((AppRect){x + width - 46, y + height - 25, 42, 19}, 8, 4);
    for (int row = 0; row < 12; ++row)
    {
        glBegin(GL_QUAD_STRIP);
        for (int col = 0; col <= 20; ++col)
            for (int side = 0; side < 2; ++side)
            {
                float u = col / 20.0f, v = (row + side) / 12.0f;
                float curl = powf(u, 8) * powf(v, 8);
                float shade = 1 + .025f * v + .10f * curl;
                glColor3f(fminf(1, bg[0] * shade), fminf(1, bg[1] * shade), fminf(1, bg[2] * shade));
                glVertex2f(x + width * u - fminf(14, width * .12f) * curl,
                           y + height * v - height * .22f * curl);
            }
        glEnd();
    }
    float tape = fminf(26, width * .16f);
    glColor4f(.92f, .85f, .65f, .76f);
    glBegin(GL_QUADS);
    glVertex2f(part->x - tape, y - 12);
    glVertex2f(part->x + tape, y - 15);
    glVertex2f(part->x + tape - 2, y + 6);
    glVertex2f(part->x - tape - 1, y + 9);
    glEnd();
}

static void part_draw(const Part *p, int selected, int led, float brightness)
{
    float x = p->x, y = p->y;
    if (p->type == PART_NOTE)
    {
        draw_note_paper(p, 240, 110);
        glColor3f(.24f, .20f, .10f);
        glLineWidth(1);
        for (int row = 0; row < 3; ++row)
            line(x - 84, y - 18 + row * 20, x + (row == 2 ? 25 : 78), y - 18 + row * 20);
        return;
    }
    if (p->type == PART_JUNCTION)
    {
        color(.10f, .38f, .22f);
        line(x - 18, y, x + 18, y);
        line(x, y - 18, x, y + 18);
        circle(x, y, 7);
        return;
    }
    if (bench_is_board(p->type))
    {
        draw_part_board(p, selected);
        return;
    }
    if (is_gate(p->type))
    {
        Part nominal = *p;
        nominal.rotation = 0;
        nominal.mirrored = 0;
        nominal.scale = 1;
        for (int pin = 0; pin < bench_pin_count(p->type); ++pin)
        {
            float px, py;
            bench_pin_position(&nominal, pin, &px, &py);
            color(.68f, .71f, .73f);
            rect(px < x ? x - 51 : x + 23, py - 2, 28, 4);
        }
    }
    if (component_device(p->type))
        draw_device(p);
    else if (p->type == PART_BATTERY_9V || p->type == PART_BATTERY_AA || p->type == PART_BATTERY_COIN)
        draw_battery(p);
    else if (p->type == PART_LAMP)
        draw_lamp(p, brightness);
    else if (p->type == PART_DIODE)
        draw_diode(p);
    else if (p->type == PART_NPN || p->type == PART_PNP || p->type == PART_TEMPERATURE)
        draw_transistor(p);
    else if (p->type == PART_LM7805)
        draw_regulator(p);
    else if (component_environment(p->type) && p->type != PART_SOURCE)
        draw_sensor(p);
    else if (p->type == PART_SOURCE)
    {
        draw_part_source(p, device_frame.current_limited);
    }
    else if (p->type == PART_BUTTON)
    {
        color(.22f, .24f, .26f);
        rect(x - 25, y - 18, 50, 40);
        gradient_rect(x - 25, y - 22, 50, 36, (SurfaceColor){.78f,.81f,.83f}, (SurfaceColor){.49f,.53f,.56f});
        color(.33f,.36f,.39f);
        for (int corner = 0; corner < 4; ++corner)
            circle(x + (corner % 2 ? 20 : -20), y + (corner / 2 ? 10 : -17), 2);
        /* Cap: raised with a drop shadow and highlight; pressed sinks into the bezel and darkens. */
        float lift = p->on ? 0 : 4;
        color(.10f, .11f, .12f);
        circle(x, y - 4, 15.5f);
        if (!p->on)
        {
            color(.06f, .07f, .08f);
            circle(x + 1.5f, y - 1.5f, 13.5f);
        }
        color(p->on ? .12f : .20f, p->on ? .13f : .22f, p->on ? .15f : .25f);
        circle(x, y - 4 - lift, p->on ? 12.5f : 13.5f);
        color(p->on ? .18f : .32f, p->on ? .20f : .35f, p->on ? .22f : .39f);
        circle(x - 2.5f, y - 6.5f - lift, p->on ? 8 : 9);
        if (!p->on)
        {
            color(.55f, .58f, .62f);
            ellipse(x - 5, y - 12 - lift, 3.5f, 2);
        }
    }
    else if (p->type == PART_SWITCH)
    {
        draw_part_switch(p);
    }
    else if (p->type == PART_CLOCK)
    {
        draw_part_clock(p);
    }
    else if (component_has_flag(p->type, COMPONENT_DIP))
    {
        draw_part_chip(p);
    }
    else if (is_gate(p->type))
    {
        draw_part_gate(p);
    }
    else if (p->type == PART_POTENTIOMETER)
    {
        color(.05f, .15f, .30f);
        circle(x, y + 5, 25);
        color(.10f, .35f, .65f);
        circle(x, y - 2, 25);
        color(.65f,.68f,.71f);
        rect(x - 28, y - 3, 7, 6);
        rect(x + 21, y - 3, 7, 6);
        rect(x - 3, y + 21, 6, 9);
        color(.65f, .69f, .73f);
        circle(x, y - 5, 13);
        color(.85f, .87f, .89f);
        circle(x - 2, y - 8, 10);
        float angle = (float)(.75 * 3.141592653589793 + p->control * 1.5 * 3.141592653589793);
        color(.12f, .14f, .17f);
        glLineWidth(2 * drawing_scale);
        line(x, y - 8, x + cosf(angle) * 9, y - 8 + sinf(angle) * 9);
        glLineWidth(1);
    }
    else if (p->type == PART_RESISTOR)
    {
        draw_part_resistor(p);
    }
    else if (p->type == PART_CAPACITOR)
    {
        draw_part_capacitor(p);
    }
    else if (p->type == PART_INDUCTOR)
    {
        draw_part_inductor(p);
    }
    else if (p->type == PART_LED)
    {
        draw_part_led(p, led, brightness);
    }
    if (selected)
    {
        color(.12f, .49f, .84f);
        glLineWidth(2);
        float half_width, half_height;
        component_body_extent(p->type, &half_width, &half_height);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x - half_width - 2, y - half_height - 2);
        glVertex2f(x + half_width + 2, y - half_height - 2);
        glVertex2f(x + half_width + 2, y + half_height + 2);
        glVertex2f(x - half_width - 2, y + half_height + 2);
        glEnd();
        glLineWidth(1);
    }
}

static float lead_corner_radius = 18;

typedef struct
{
    WirePoint previous;
    float half_width;
    int started;
} LeadStroke;

static void lead_vertex(void *context, WirePoint point, size_t slot)
{
    (void)slot;
    LeadStroke *stroke = context;
    if (stroke->started)
    {
        float dx = point.x - stroke->previous.x, dy = point.y - stroke->previous.y;
        float length = hypotf(dx, dy);
        if (length > .00001f)
        {
            float nx = -dy * stroke->half_width / length, ny = dx * stroke->half_width / length;
            color(.51f, .55f, .59f);
            glVertex2f(stroke->previous.x + nx, stroke->previous.y + ny);
            glVertex2f(point.x + nx, point.y + ny);
            color(.88f, .9f, .92f);
            glVertex2f(point.x - nx, point.y - ny);
            glVertex2f(stroke->previous.x - nx, stroke->previous.y - ny);
        }
    }
    stroke->previous = point;
    stroke->started = 1;
}

static void draw_part_leads(const Part *part)
{
    for (int pin = 0; pin < bench_pin_count(part->type); ++pin)
    {
        size_t count = bench_lead_count(part, pin);
        if (!count)
            continue;
        LeadStroke stroke = {.half_width = fmaxf(.9f * bench_part_scale(part), .5f / drawing_scale)};
        glBegin(GL_QUADS);
        bench_lead_walk(part, pin, lead_corner_radius, lead_vertex, &stroke);
        glEnd();
    }
}

static void part_draw_oriented(const Part *part, int selected, int led, float brightness)
{
    draw_part_leads(part);
    Part local = *part;
    local.x = 0;
    local.y = 0;
    glPushMatrix();
    glTranslatef(part->x, part->y, 0);
    glRotatef(part->rotation * 90.0f + part->rotation_diagonal * 45.0f, 0, 0, 1);
    float scale = bench_part_scale(part);
    glScalef(scale, scale, 1);
    drawing_mirrored = part->mirrored && !bench_is_board(part->type);
    if (drawing_mirrored)
        glScalef(-1, 1, 1);
    glLineWidth(fmaxf(1, drawing_scale * scale));
    part_draw(&local, selected, led, brightness);
    drawing_mirrored = 0;
    glLineWidth(1);
    glPopMatrix();
}

/* Scorch mark, rising smoke and a label over a burnt part. The smoke is decoration, not a physical quantity. */
static void burnt_draw(GLuint font, const Part *part, Uint64 ticks)
{
    int k;
    float x = part->x, y = part->y;
    glow_blend_begin();
    glColor4f(.10f, .07f, .05f, .55f);
    circle(x, y, 22);
    for (k = 0; k < 3; ++k)
    {
        float phase = fmodf((float)ticks * .0005f + (float)k / 3.0f, 1.0f);
        glColor4f(.38f, .38f, .40f, .6f * (1.0f - phase));
        circle(x + sinf(phase * 6.0f + (float)k * 2.0f) * 7.0f, y - 20.0f - phase * 36.0f, 5.0f + phase * 8.0f);
    }
    glow_blend_end();
    color(.86f, .12f, .08f);
    text(font, x - 36, y - 44, tr(TXT_UI_BURNED_MARK));
}

/* ---- Interface widgets: geometry comes from app_layout; colors pass through the theme mapping. ---- */
#define UI_RADIUS 8.0f
#define UI_ACCENT (dark_interface ? .48f : .12f), (dark_interface ? .70f : .38f), (dark_interface ? .98f : .74f)
#define UI_ACCENT_SOFT .86f, .92f, .99f
#define UI_INK .13f, .19f, .24f
#define UI_MUTED .40f, .46f, .51f
#define UI_SURFACE .985f, .99f, .995f
#define UI_SURFACE_ALT .93f, .95f, .965f
#define UI_BORDER .80f, .84f, .87f
#define UI_DANGER .74f, .16f, .12f
#define UI_POSITIVE .15f, .52f, .28f
static int ui_font_pixels = 15;

typedef enum
{
    UI_BUTTON_NEUTRAL,
    UI_BUTTON_PRIMARY,
    UI_BUTTON_ACTIVE,
    UI_BUTTON_DANGER,
    UI_BUTTON_DISABLED
} UiButtonStyle;

/* Ink on saturated fills must stay light in both themes, so it bypasses the theme mapping. */
static void ui_raw_white(void)
{
    glColor3f(.99f, .995f, 1);
}

static void rounded_rect(float x, float y, float w, float h, float radius)
{
    const int segments = 6;
    radius = fminf(radius, fminf(w, h) / 2);
    const float corners[4][2] = {{x + w - radius, y + radius}, {x + w - radius, y + h - radius},
                                 {x + radius, y + h - radius}, {x + radius, y + radius}};
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + w / 2, y + h / 2);
    for (int corner = 0; corner < 4; ++corner)
        for (int step = 0; step <= segments; ++step)
        {
            float angle = (corner - 1 + (float)step / segments) * 1.5707963f;
            glVertex2f(corners[corner][0] + cosf(angle) * radius, corners[corner][1] + sinf(angle) * radius);
        }
    glVertex2f(corners[0][0], y);
    glEnd();
}

static void ui_rounded(AppRect r, float radius)
{
    rounded_rect(r.x, r.y, r.w, r.h, radius);
}

static void ui_outline(AppRect r, float radius, float thickness)
{
    ui_rounded(r, radius);
    color(UI_SURFACE);
    rounded_rect(r.x + thickness, r.y + thickness, r.w - 2 * thickness, r.h - 2 * thickness,
                 fmaxf(0, radius - thickness));
}

#define UI_SHADOW_LAYERS 5
/* Raised surface shadow: offset down and blurred by translucent layers that widen outward. */
static void ui_shadow(AppRect r, float radius, float depth)
{
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float alpha = dark_interface ? .10f : .045f;
    for (int layer = UI_SHADOW_LAYERS; layer >= 1; --layer)
    {
        float grow = depth * (float)layer / UI_SHADOW_LAYERS;
        glColor4f(.04f, .08f, .12f, alpha);
        rounded_rect(r.x - grow, r.y - grow + depth * .5f, r.w + 2 * grow, r.h + 2 * grow, radius + grow);
    }
    glPopAttrib();
}

static void ui_panel(AppRect r)
{
    ui_shadow(r, UI_RADIUS + 2, 10);
    color(UI_BORDER);
    ui_rounded(r, UI_RADIUS + 2);
    color(UI_SURFACE);
    rounded_rect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, UI_RADIUS + 1);
}

static void ui_focus_outline(AppRect r)
{
    glColor3f(dark_interface ? .50f : .10f, dark_interface ? .74f : .36f, dark_interface ? 1.0f : .72f);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    glVertex2f(r.x - 2, r.y - 2);
    glVertex2f(r.x + r.w + 2, r.y - 2);
    glVertex2f(r.x + r.w + 2, r.y + r.h + 2);
    glVertex2f(r.x - 2, r.y + r.h + 2);
    glEnd();
    glLineWidth(1);
}

static float ui_baseline(AppRect r)
{
    return r.y + r.h / 2 + ui_font_pixels * .36f;
}

static int ui_text_width(GLuint font, const char *label)
{
    int width = app_text_utf8_width(label, 2048);
    return width > 0 || !font ? width : (int)strlen(label) * 8;
}

static void ui_wrapped(GLuint font, float x, float baseline, const char *source, int width, int max_lines,
                       float line_height);

/* Keep short names centered and use the card height for longer translations. */
static void palette_name(GLuint font, AppRect r, const char *name)
{
    const int text_offset = 62;
    const int right_padding = 4;
    const float line_height = ui_font_pixels + 2.0f;
    int width = (int)r.w - text_offset - right_padding;
    float baseline = ui_baseline(r);
    if (ui_text_width(font, name) > width)
        baseline -= line_height / 2;
    ui_wrapped(font, r.x + text_offset, baseline, name, width, 2, line_height);
}

static void ui_text_center(GLuint font, AppRect r, const char *label)
{
    text(font, r.x + (r.w - ui_text_width(font, label)) / 2, ui_baseline(r), label);
}

static void ui_text_right_edge(GLuint font, float right, float baseline, const char *label)
{
    text(font, right - ui_text_width(font, label), baseline, label);
}

static int ui_hovered(const AppState *app, AppRect r)
{
    return app->mouse_position_valid && app_rect_contains(r, app->mouse_screen.x, app->mouse_screen.y);
}

static void ui_button(const AppState *app, GLuint font, AppRect r, const char *label, UiButtonStyle style)
{
    int hover = style != UI_BUTTON_DISABLED && ui_hovered(app, r);
    if (style == UI_BUTTON_PRIMARY || style == UI_BUTTON_DANGER)
    {
        if (style == UI_BUTTON_PRIMARY)
            glColor3f(hover ? .12f : .16f, hover ? .39f : .45f, hover ? .74f : .82f);
        else
            glColor3f(hover ? .64f : .74f, hover ? .12f : .16f, .12f);
        ui_rounded(r, UI_RADIUS);
        ui_raw_white();
        ui_text_center(font, r, label);
        return;
    }
    if (style == UI_BUTTON_ACTIVE)
        color(UI_ACCENT_SOFT);
    else
        color(hover ? .88f : .93f, hover ? .91f : .95f, hover ? .95f : .965f);
    ui_rounded(r, UI_RADIUS);
    if (style == UI_BUTTON_ACTIVE)
        color(UI_ACCENT);
    else if (style == UI_BUTTON_DISABLED)
        color(.62f, .66f, .70f);
    else
        color(UI_INK);
    ui_text_center(font, r, label);
}

static void ui_close_button(const AppState *app, AppRect r)
{
    if (ui_hovered(app, r))
    {
        color(UI_SURFACE_ALT);
        ui_rounded(r, UI_RADIUS);
    }
    color(UI_MUTED);
    glLineWidth(2);
    float cx = app_rect_center_x(r), cy = app_rect_center_y(r), arm = 7;
    line(cx - arm, cy - arm, cx + arm, cy + arm);
    line(cx - arm, cy + arm, cx + arm, cy - arm);
    glLineWidth(1);
}

static void ui_row_hover(const AppState *app, AppRect row)
{
    if (!ui_hovered(app, row))
        return;
    color(UI_SURFACE_ALT);
    rounded_rect(row.x - 8, row.y, row.w + 16, row.h, UI_RADIUS);
}

static void ui_label(GLuint font, AppRect row, const char *label)
{
    color(UI_INK);
    text(font, row.x, ui_baseline(row), label);
}

static void ui_toggle(const AppState *app, GLuint font, AppRect row, const char *label, int on)
{
    const float track_w = 42, track_h = 22;
    ui_row_hover(app, row);
    ui_label(font, row, label);
    AppRect track = {row.x + row.w - track_w, row.y + (row.h - track_h) / 2, track_w, track_h};
    if (on)
        glColor3f(UI_ACCENT);
    else
        color(.74f, .78f, .82f);
    ui_rounded(track, track_h / 2);
    ui_raw_white();
    circle(on ? track.x + track_w - track_h / 2 : track.x + track_h / 2, track.y + track_h / 2, track_h / 2 - 3);
}

static void ui_value_box(GLuint font, AppRect box, const char *value, int editable)
{
    color(UI_BORDER);
    ui_rounded(box, 6);
    color(UI_SURFACE);
    rounded_rect(box.x + 1, box.y + 1, box.w - 2, box.h - 2, 5);
    color(editable ? .12f : .30f, editable ? .34f : .36f, editable ? .62f : .40f);
    ui_text_center(font, box, value);
}

static void ui_field(const AppState *app, GLuint font, AppRect row, const char *label, const char *value)
{
    const float box_w = 150;
    ui_row_hover(app, row);
    ui_label(font, row, label);
    AppRect box = {row.x + row.w - box_w, row.y + 3, box_w, row.h - 6};
    ui_value_box(font, box, value, 1);
}

static void ui_stepper(const AppState *app, GLuint font, AppRect row, AppRect minus, AppRect plus, const char *label,
                       const char *value)
{
    ui_label(font, row, label);
    ui_button(app, font, minus, "\xe2\x88\x92", UI_BUTTON_NEUTRAL);
    ui_button(app, font, plus, "+", UI_BUTTON_NEUTRAL);
    AppRect value_box = {minus.x + minus.w, row.y, plus.x - minus.x - minus.w, row.h};
    color(UI_INK);
    ui_text_center(font, value_box, value);
}

static void ui_section(GLuint font, AppRect r, const char *title)
{
    glColor3f(UI_ACCENT);
    text(font, r.x, r.y + r.h - 6, title);
    color(UI_BORDER);
    rect(r.x, r.y + r.h, r.w, 1);
}

static void ui_note(GLuint font, float x, float baseline, const char *note, int maximum_width)
{
    color(UI_MUTED);
    name_text(font, x, baseline, note, maximum_width);
}

static void play_button_draw(const AppState *app, GLuint font)
{
    AppRect r = app_layout_header(APP_HEADER_PLAY);
    int running = app->sim.sim_running, burnt = app->sim.burnt_count;
    const char *label = tr(TXT_UI_RUNNING);
    float fill[3] = {.84f, .94f, .87f};
    if (!running)
    {
        label = tr(TXT_UI_PAUSED);
        fill[0] = .89f, fill[1] = .91f, fill[2] = .93f;
    }
    else if (burnt)
    {
        label = tr(TXT_UI_PART_BURNED);
        fill[0] = .99f, fill[1] = .87f, fill[2] = .85f;
    }
    else if (app->sim.auto_failed)
    {
        label = tr(TXT_UI_CALCULATION_ERROR);
        fill[0] = .99f, fill[1] = .91f, fill[2] = .80f;
    }
    else if (app->dc_job.thread)
        label = tr(TXT_UI_CALCULATING);
    color(fill[0] - (ui_hovered(app, r) ? .04f : 0), fill[1] - (ui_hovered(app, r) ? .04f : 0), fill[2]);
    ui_rounded(r, r.h / 2);
    float cx = r.x + 24, cy = app_rect_center_y(r);
    glColor3f(running ? .13f : .27f, running ? .48f : .35f, running ? .24f : .42f);
    circle(cx, cy, 15);
    ui_raw_white();
    if (running)
    {
        rect(cx - 6, cy - 7, 4, 14);
        rect(cx + 2, cy - 7, 4, 14);
    }
    else
    {
        glBegin(GL_TRIANGLES);
        glVertex2f(cx - 4, cy - 8);
        glVertex2f(cx - 4, cy + 8);
        glVertex2f(cx + 8, cy);
        glEnd();
    }
    if (burnt)
        color(UI_DANGER);
    else
        color(UI_INK);
    text(font, r.x + 48, ui_baseline(r), label);
}
static void draw_thumbnail(PartType type, float x, float y)
{
    device_frame = (DeviceFrame){0};
    int previous_layer = interface_layer;
    float previous_scale = drawing_scale;
    interface_layer = 0;
    Part preview = {0};
    preview.type = type;
    preview.on = 1;
    preview.board_layout = 1;
    preview.control = .5;
    const ComponentInfo *info = component_info(type);
    preview.value = info && info->primary ? info->primary->initial : 0;
    float scale = bench_is_board(type) ? .09f : .42f;
    float half_width, half_height;
    component_body_extent(type, &half_width, &half_height);
    scale = fminf(scale, 22 / fmaxf(1, half_width + 8));
    scale = fminf(scale, 19 / fmaxf(1, half_height + 8));
    drawing_scale = scale;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1);
    glLineWidth(1);
    part_draw_oriented(&preview, 0, 0, 0);
    if (bench_is_board(type))
    {
        color(.18f, .22f, .24f);
        glPointSize(1);
        glBegin(GL_POINTS);
        for (int pin = 0; pin < bench_pin_count(type); ++pin)
        {
            float px, py;
            bench_pin_position(&preview, pin, &px, &py);
            glVertex2f(px, py);
        }
        glEnd();
    }
    else
        for (int pin = 0; pin < bench_pin_count(type); ++pin)
        {
            float px, py;
            bench_pin_position(&preview, pin, &px, &py);
            color(.2f, .24f, .26f);
            circle(px, py, 4);
        }
    glPopMatrix();
    interface_layer = previous_layer;
    drawing_scale = previous_scale;
}

static void draw_brand(GLuint font)
{
    AppRect mark = {24, 20, 28, 28};
    glColor3f(UI_ACCENT);
    ui_rounded(mark, 7);
    ui_raw_white();
    glLineWidth(2);
    line(mark.x + 5, mark.y + 14, mark.x + 9, mark.y + 14);
    line(mark.x + 9, mark.y + 14, mark.x + 12, mark.y + 8);
    line(mark.x + 12, mark.y + 8, mark.x + 16, mark.y + 20);
    line(mark.x + 16, mark.y + 20, mark.x + 19, mark.y + 14);
    line(mark.x + 19, mark.y + 14, mark.x + 23, mark.y + 14);
    glLineWidth(1);
    color(UI_INK);
    text(font, 62, ui_baseline(mark), "Bancada");
}

static void draw_editor_header(const AppState *app, GLuint font)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s", app->editor_name);
    for (const AppState *parent = app->editor_parent; parent; parent = parent->editor_parent)
    {
        char next[1024];
        const char *name = parent->editor_parent ? parent->editor_name : "Bancada";
        snprintf(next, sizeof(next), "%.127s > %.800s", name, path);
        memcpy(path, next, strlen(next) + 1);
    }
    char context[BENCH_GROUP_NAME_CAPACITY + 40];
    snprintf(context, sizeof(context), tr(TXT_UI_EDITING_CHIP), app->editor_name, app_editor_dirty(app) ? " *" : "");
    color(UI_INK);
    name_text(font, 24, 27, context, 320);
    name_text(font, 24, 51, path, 320);
    static const TextId labels[APP_EDITOR_COUNT] = {TXT_UI_EDITOR_APPLY, TXT_UI_RENAME, TXT_UI_EDITOR_SAVE, TXT_UI_BACK};
    for (int button = 0; button < APP_EDITOR_COUNT; ++button)
        ui_button(app, font, app_layout_editor_header((AppEditorButton)button), tr(labels[button]),
                  button == APP_EDITOR_APPLY ? UI_BUTTON_PRIMARY : UI_BUTTON_NEUTRAL);
}

static void draw_header(const AppState *app, GLuint font)
{
    color(UI_SURFACE);
    rect(0, 0, app_layout_canvas_width(), APP_HEADER_HEIGHT);
    color(UI_BORDER);
    rect(0, APP_HEADER_HEIGHT - 1, app_layout_canvas_width(), 1);
    if (app->editor_parent)
        draw_editor_header(app, font);
    else
    {
        draw_brand(font);
        ui_button(app, font, app_layout_header(APP_HEADER_PROJECTS), tr(TXT_UI_PROJECTS),
                  app->projects_open ? UI_BUTTON_ACTIVE : UI_BUTTON_NEUTRAL);
        ui_button(app, font, app_layout_header(APP_HEADER_EXAMPLES), tr(TXT_UI_EXAMPLES),
                  app->examples_open ? UI_BUTTON_ACTIVE : UI_BUTTON_NEUTRAL);
        ui_button(app, font, app_layout_header(APP_HEADER_SETTINGS), tr(TXT_SETTINGS_TITLE),
                  app->settings ? UI_BUTTON_ACTIVE : UI_BUTTON_NEUTRAL);
    }
    play_button_draw(app, font);
}

static void draw_palette_tabs(const AppState *app, GLuint font)
{
    const int per_row = 3;
    color(UI_SURFACE_ALT);
    for (int row = 0; row < (APP_PALETTE_TABS + per_row - 1) / per_row; ++row)
    {
        int last_tab = row * per_row + per_row - 1 < APP_PALETTE_TABS ? row * per_row + per_row - 1 : APP_PALETTE_TABS - 1;
        AppRect first = app_layout_palette_tab(row * per_row), last = app_layout_palette_tab(last_tab);
        rounded_rect(first.x, first.y, last.x + last.w - first.x, first.h, UI_RADIUS);
    }
    for (int tab = 0; tab < APP_PALETTE_TABS; ++tab)
    {
        AppRect r = app_layout_palette_tab(tab);
        if (app->palette_page == tab)
        {
            color(UI_SURFACE);
            rounded_rect(r.x + 3, r.y + 3, r.w - 6, r.h - 6, UI_RADIUS - 2);
            glColor3f(UI_ACCENT);
        }
        else
            color(UI_MUTED);
        ui_text_center(font, r, palette_page_name(tab));
    }
}

static void draw_palette_card(const AppState *app, AppRect r, int selected)
{
    if (selected)
    {
        glColor3f(UI_ACCENT);
        ui_outline(r, UI_RADIUS, 2);
        color(UI_ACCENT_SOFT);
        rounded_rect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, UI_RADIUS - 2);
        return;
    }
    color(ui_hovered(app, r) ? .90f : .95f, ui_hovered(app, r) ? .93f : .96f, ui_hovered(app, r) ? .97f : .975f);
    ui_rounded(r, UI_RADIUS);
}

static void draw_palette_scrollbar(int offset, int count)
{
    if (count <= APP_PALETTE_VISIBLE)
        return;
    AppRect track = app_layout_palette_scrollbar();
    color(UI_SURFACE_ALT);
    ui_rounded(track, track.w / 2);
    float thumb_h = track.h * APP_PALETTE_VISIBLE / count;
    float thumb_y = track.y + (track.h - thumb_h) * offset / (count - APP_PALETTE_VISIBLE);
    color(.62f, .68f, .73f);
    rounded_rect(track.x, thumb_y, track.w, thumb_h, track.w / 2);
}

static void draw_block_library(const AppState *app, GLuint font)
{
    int available = 0;
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        available += app->library_available[i];
    if (!available)
    {
        color(UI_MUTED);
        ui_wrapped(font, 20, 236, tr(TXT_UI_LIBRARY_GUIDE), 180, 5, 22);
        return;
    }
    for (int slot = 0; slot < APP_PALETTE_VISIBLE; ++slot)
    {
        if (!app->library_available[slot])
            continue;
        AppRect r = app_layout_palette_item(slot);
        draw_palette_card(app, r, 0);
        draw_thumbnail(PART_74HC00, r.x + 28, app_rect_center_y(r));
        if (app->library_available[slot] && app->library_names[slot][0])
        {
            color(UI_INK);
            name_text(font, r.x + 62, ui_baseline(r), app->library_names[slot], 92);
            AppRect remove = app_layout_palette_delete(slot);
            int armed = app->library_delete_armed == slot;
            ui_button(app, font, remove, "×", armed ? UI_BUTTON_DANGER : UI_BUTTON_NEUTRAL);
            continue;
        }
        char label[48];
        snprintf(label, sizeof(label), tr(TXT_CHIP_DEFAULT_NAME), slot + 1);
        if (app->library_available[slot])
            color(UI_INK);
        else
            color(UI_MUTED);
        text(font, r.x + 62, ui_baseline(r), app->library_available[slot] ? label : tr(TXT_UI_LIBRARY_NEW));
    }
}

static void draw_palette(const AppState *app, GLuint font)
{
    color(UI_SURFACE);
    float height = app_layout_canvas_height() - APP_HEADER_HEIGHT;
    rect(0, APP_HEADER_HEIGHT, APP_PALETTE_WIDTH, height);
    color(UI_BORDER);
    rect(APP_PALETTE_WIDTH - 1, APP_HEADER_HEIGHT, 1, height);
    draw_palette_tabs(app, font);
    AppRect search = app_layout_search();
    ui_value_box(font, search, "", 1);
    if (app->palette_search_active)
        ui_focus_outline(search);
    color(app->palette_search[0] ? .13f : .40f, app->palette_search[0] ? .19f : .46f, app->palette_search[0] ? .24f : .51f);
    name_text(font, search.x + 8, ui_baseline(search), app->palette_search[0] ? app->palette_search : tr(TXT_UI_SEARCH), 144);
    if (app->palette_search[0])
        ui_button(app, font, app_layout_search_clear(), "×", UI_BUTTON_NEUTRAL);
    if (app->palette_page == 2 && !app->palette_search[0])
    {
        draw_block_library(app, font);
        return;
    }
    int count = palette_filtered_count(app->palette_page, app->palette_search);
    if (!count)
    {
        color(UI_MUTED);
        ui_wrapped(font, 20, 236, tr(TXT_UI_SEARCH_EMPTY), 180, 4, 22);
    }
    for (int slot = 0; slot < APP_PALETTE_VISIBLE; ++slot)
    {
        int item = slot + app->palette_offset;
        if (item >= count)
            break;
        PartType type = palette_filtered_part(app->palette_page, app->palette_search, item);
        AppRect r = app_layout_palette_item(slot);
        draw_palette_card(app, r, app->placing == (int)type);
        draw_thumbnail(type, r.x + 28, app_rect_center_y(r));
        color(UI_INK);
        palette_name(font, r, part_name(type));
    }
    draw_palette_scrollbar(app->palette_offset, count);
}

static void draw_actions(const AppState *app, GLuint font)
{
    if (app->code_part >= 0)
        return;
    static const TextId labels[APP_ACTION_COUNT] = {
        TXT_UI_UNDO, TXT_UI_REDO, TXT_UI_STEP, TXT_UI_CREATE_CHIP, TXT_UI_LIVE_GRAPH, TXT_UI_FULL_CURVE};
    AppRect first = app_layout_action((AppAction)0), last = app_layout_action((AppAction)(APP_ACTION_COUNT - 1));
    AppRect bar = {first.x - 4, first.y - 4, last.x + last.w - first.x + 8, first.h + 8};
    ui_shadow(bar, UI_RADIUS + 2, 6);
    color(UI_BORDER);
    ui_rounded(bar, UI_RADIUS + 2);
    color(UI_SURFACE);
    rounded_rect(bar.x + 1, bar.y + 1, bar.w - 2, bar.h - 2, UI_RADIUS + 1);
    for (int i = 0; i < APP_ACTION_COUNT; ++i)
        ui_button(app, font, app_layout_action((AppAction)i), tr(labels[i]),
                  app_controls_action_enabled(app, (AppAction)i) ? UI_BUTTON_NEUTRAL : UI_BUTTON_DISABLED);
}

static void draw_chrome(const AppState *app, GLuint font, int width, int height)
{
    AppRect viewport = app_layout_viewport(width, height);
    glViewport((int)viewport.x, height - (int)(viewport.y + viewport.h), (int)viewport.w, (int)viewport.h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, app_layout_canvas_width(), app_layout_canvas_height(), 0, -1, 1);
    app_text_utf8_set_scale(viewport.w / app_layout_canvas_width());
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (app->dark_theme)
        glClearColor(.07f, .10f, .14f, 1);
    else
        glClearColor(.92f, .94f, .95f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_header(app, font);
    draw_palette(app, font);
}

static void draw_workspace_grid(const AppState *app)
{
    if (app->dark_theme)
        color(.17f, .22f, .28f);
    else
        color(.85f, .88f, .89f);
    glLineWidth(1);
    if (app->show_grid)
    {
        /* Cover the whole visible area at any zoom; widen the spacing when lines would crowd together. */
        float left, top, right, bottom, gx, gy;
        BenchGrid grid = bench_grid_default();
        grid.spacing = app->grid_spacing;
        float spacing = (float)bench_grid_display_spacing(&grid, app->view.zoom, 10);
        bench_view_to_world(&app->view, 220, 60, &left, &top);
        bench_view_to_world(&app->view, app_layout_canvas_width(), BENCH_VIEW_BOTTOM, &right, &bottom);
        if (spacing <= 0)
            return;
        for (gx = floorf(left / spacing) * spacing; gx <= right; gx += spacing)
            line(gx, top, gx, bottom);
        for (gy = floorf((top - (float)grid.origin_y) / spacing) * spacing + (float)grid.origin_y; gy <= bottom; gy += spacing)
            line(left, gy, right, gy);
    }
}

/* Brightness is a display curve of measured forward current, not voltage.
 * Twenty milliamperes is the visual full-scale reference, not a device rating. */
static float measured_led_brightness(const AppState *app, int part)
{
    const double visual_full_scale_amperes = SPICE_LED_VISUAL_FULL_SCALE_AMPERES;
    if (sim_state_has_result(&app->sim, SIM_RESULT_DC))
        return (float)sqrt(fmin(1.0, fmax(0.0, app->sim.dc_led_currents[part] / visual_full_scale_amperes)));
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
        return app->sim.trace.led_brightness[app->sim.trace_index][part] / 255.0f;
    return app->leds[part] == 1 ? 1 : 0;
}

static void set_device_frame(const AppState *app, int part)
{
    int measured = sim_state_has_result(&app->sim, SIM_RESULT_DC);
    device_frame.elements = measured ? app->sim.dc_elements[part] : NULL;
    device_frame.scope = app->sim.scope_history[part];
    device_frame.scope_count = app->sim.scope_count[part];
    device_frame.motor_angle = app->sim.motor_angle[part];
    device_frame.current_limited = measured && app->sim.alert[part] == PART_ALERT_CURRENT_LIMIT;
    device_frame.lcd = app->mcu ? &app->mcu->lcd[part] : NULL;
}

static void draw_workspace_boards(const AppState *app)
{
    int i;
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && bench_is_board(app->bench.parts[i].type))
        {
            int p;
            float hole_radius = 3 * fminf(1, bench_part_scale(&app->bench.parts[i]));
            set_device_frame(app, i);
            part_draw_oriented(&app->bench.parts[i], (app->selected == i || app->selection.parts[i]), app->leds[i], measured_led_brightness(app, i));
            for (p = 0; p < bench_pin_count(app->bench.parts[i].type); ++p)
            {
                float x, y;
                bench_pin_position(&app->bench.parts[i], p, &x, &y);
                color(.18f, .22f, .24f);
                circle(x, y, hole_radius);
                if (app->pending.component == i && app->pending.pin == p)
                {
                    color(1, .7f, .2f);
                    circle(x, y, hole_radius);
                }
            }
        }
}

static void draw_wire_trace_current(const AppState *app, int i, const WirePoint *path, int count)
{
    if (app->show_current && sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->sim.trace.measures_wires)
    {
        uint64_t bit = UINT64_C(1) << (i % 64);
        int forward = (app->sim.trace.wires_forward[app->sim.trace_index][i / 64] & bit) != 0,
            reverse = (app->sim.trace.wires_reverse[app->sim.trace_index][i / 64] & bit) != 0;
        if (forward || reverse)
        {
            int level = (app->sim.trace.wire_levels[app->sim.trace_index][i / 4] >> ((i % 4) * 2)) & 3;
            wire_direction_marker(path, count, reverse, level);
        }
    }
}

static void draw_wire_dc_current(const AppState *app, int i, const WirePoint *path, int count)
{
    if (!app->show_current || !sim_state_has_result(&app->sim, SIM_RESULT_DC) || !app->sim.dc_wire_valid)
        return;
    int level = spice_wire_current_level(app->sim.dc_wire_currents[i]);
    if (level > 0)
        wire_direction_marker(path, count, app->sim.dc_wire_currents[i] < 0, level);
}

/* Wire widths in screen units, so a wire looks the same at every zoom. */
#define WIRE_BODY_WIDTH 5.0f
#define WIRE_SHINE_WIDTH 2.0f
#define WIRE_SELECTED_WIDTH 10.0f
#define WIRE_SHADOW_WIDTH 7.0f
#define WIRE_SHADOW_DROP 1.0f
#define WIRE_SHADOW_ALPHA .16f

/* Polyline of a given width with round joins: no notch or spur at a knee, whatever its angle. */
static void thick_path(const WirePoint *path, int count, float width, float dx, float dy)
{
    float half = width / 2;
    for (int k = 0; k + 1 < count; ++k)
    {
        float ax = path[k].x + dx, ay = path[k].y + dy, bx = path[k + 1].x + dx, by = path[k + 1].y + dy;
        float length = hypotf(bx - ax, by - ay);
        if (length <= 0)
            continue;
        float nx = -(by - ay) / length * half, ny = (bx - ax) / length * half;
        glBegin(GL_QUADS);
        glVertex2f(ax + nx, ay + ny);
        glVertex2f(bx + nx, by + ny);
        glVertex2f(bx - nx, by - ny);
        glVertex2f(ax - nx, ay - ny);
        glEnd();
    }
    for (int k = 0; k < count; ++k)
        circle(path[k].x + dx, path[k].y + dy, half);
}

static int visible_wire_path(const AppState *app, int wire, WirePoint *path)
{
    const Wire *w = &app->bench.wires[wire];
    if (bench_group_wire_internal(&app->bench, w))
        return 0;
    WirePoint start, end;
    bench_terminal_position(&app->bench, w->a, &start.x, &start.y);
    bench_terminal_position(&app->bench, w->b, &end.x, &end.y);
    return wire_path(w, start, end, app->round_wires ? 18.0f : 0.0f, path);
}

/* Every shadow goes down before any wire, so wires laid side by side never shade each other's color. */
static void draw_wire_shadows(const AppState *app, float unit)
{
    if (app->dark_theme)
        return;
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(.10f, .14f, .18f, WIRE_SHADOW_ALPHA);
    for (int i = 0; i < app->bench.wire_count; ++i)
    {
        WirePoint path[WIRE_PATH_CAPACITY];
        int count = visible_wire_path(app, i, path);
        if (count)
            thick_path(path, count, WIRE_SHADOW_WIDTH * unit, 0, WIRE_SHADOW_DROP * unit);
    }
    glPopAttrib();
}

static void draw_workspace_wires(const AppState *app)
{
    float unit = 1 / app->view.zoom;
    draw_wire_shadows(app, unit);
    for (int i = 0; i < app->bench.wire_count; ++i)
    {
        WirePoint path[WIRE_PATH_CAPACITY];
        int count = visible_wire_path(app, i, path);
        if (!count)
            continue;
        const float *tone = palette_tone(app->bench.wires[i].color)->wire;
        if (i == app->selected_wire)
        {
            color(.15f, .49f, .9f);
            thick_path(path, count, WIRE_SELECTED_WIDTH * unit, 0, 0);
        }
        color(tone[0], tone[1], tone[2]);
        thick_path(path, count, WIRE_BODY_WIDTH * unit, 0, 0);
        color(tone[0] * .65f + .25f, tone[1] * .65f + .25f, tone[2] * .65f + .25f);
        thick_path(path, count, WIRE_SHINE_WIDTH * unit, 0, 0);
        draw_wire_trace_current(app, i, path, count);
        draw_wire_dc_current(app, i, path, count);
    }
    glLineWidth(2); /* the route preview drawn next keeps its former width */
}

static void draw_workspace_route(const AppState *app)
{
    int i;
    if (app->pending.component >= 0)
    {
        float x, y;
        int k;
        bench_terminal_position(&app->bench, app->pending, &x, &y);
        color(.9f, .55f, .15f);
        for (k = 0; k < app->route_count; ++k)
        {
            line(x, y, app->route[k].x, app->route[k].y);
            x = app->route[k].x;
            y = app->route[k].y;
        }
    }
    if (app->show_handles)
        for (i = 0; i < app->bench.wire_count; ++i)
        {
            int k;
            if (bench_group_wire_internal(&app->bench, &app->bench.wires[i]))
                continue;
            for (k = 0; k < app->bench.wires[i].knee_count; ++k)
            {
                /* Screen-sized dots: visible as edit handles without hiding the wire at any zoom. */
                WirePoint p = app->bench.wires[i].knees[k];
                float radius = (app->selected_wire == i || (app->selection.knees[i] & (1u << k)) ? 3.5f : 2.2f) /
                               app->view.zoom;
                color(.85f, .88f, .92f);
                circle(p.x, p.y, radius + .8f / app->view.zoom);
                if (app->selection.knees[i] & (1u << k))
                    color(.95f, .48f, .12f);
                else
                    color(.1f, .3f, .45f);
                circle(p.x, p.y, radius);
            }
        }
}

/* Terminal discs follow the breadboard hole in world units; picking keeps its own screen tolerance. */
#define TERMINAL_WORLD_RADIUS 3.0f
#define LABEL_MIN_ZOOM 0.6f

static int labels_visible(const AppState *app)
{
    return app->view.zoom >= LABEL_MIN_ZOOM;
}

/* Wire junction: a filled dot (schematic style), ringed when selected. */
static void draw_junction(const AppState *app, int index)
{
    const Part *part = &app->bench.parts[index];
    float radius = 4.5f;
    if (app->selected == index || app->selection.parts[index])
    {
        glColor3f(.12f, .49f, .84f);
        circle(part->x, part->y, radius * 1.8f);
    }
    color(.10f, .38f, .22f);
    circle(part->x, part->y, radius);
}

static void draw_workspace_parts(const AppState *app)
{
    int i;
    glLineWidth(1);
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && app->bench.parts[i].type == PART_JUNCTION)
            draw_junction(app, i);
        else if (!app->bench.parts[i].group && !bench_is_board(app->bench.parts[i].type) && app->bench.parts[i].type != PART_NOTE)
        {
            int p;
            set_device_frame(app, i);
            if (app->xray)
                xray_begin();
            part_draw_oriented(&app->bench.parts[i], (app->selected == i || app->selection.parts[i]), app->leds[i], measured_led_brightness(app, i));
            xray_end();
            if (app->show_handles && app->selected == i)
                for (int pin = 0; pin < bench_pin_count(app->bench.parts[i].type); ++pin)
                    for (size_t point = 0; point + 1 < bench_lead_count(&app->bench.parts[i], pin); ++point)
                    {
                        WirePoint knee = bench_lead_point(&app->bench.parts[i], pin, point);
                        color(.25f, .65f, .9f);
                        circle(knee.x, knee.y, 2.8f / app->view.zoom);
                    }
            for (p = 0; p < bench_pin_count(app->bench.parts[i].type); ++p)
            {
                float x, y;
                Pin contact;
                int snapped = bench_find_contact(&app->bench, i, p, &contact);
                if (snapped && !app->show_connection_pins &&
                    (app->pending.component != i || app->pending.pin != p))
                    continue;
                float outer_radius = TERMINAL_WORLD_RADIUS;
                float inner_radius = outer_radius * .7f;
                bench_pin_position(&app->bench.parts[i], p, &x, &y);
                color(.18f, .22f, .24f);
                circle(x, y, outer_radius);
                if (app->pending.component == i && app->pending.pin == p)
                {
                    color(1, .7f, .2f);
                    circle(x, y, inner_radius);
                }
                else if (snapped && app->show_connection_pins)
                {
                    color(.18f, .78f, .46f);
                    circle(x, y, inner_radius);
                }
                else
                {
                    color(.93f, .95f, .96f);
                    circle(x, y, inner_radius);
                }
            }
        }
}

static void draw_workspace_contacts(const AppState *app)
{
    int i;
    for (i = 0; i < app->bench.wire_count; ++i)
    {
        const Wire *wire = &app->bench.wires[i];
        if (bench_group_wire_internal(&app->bench, wire))
            continue;
        WirePoint start, end, path[WIRE_PATH_CAPACITY];
        int count, k;
        bench_terminal_position(&app->bench, wire->a, &start.x, &start.y);
        bench_terminal_position(&app->bench, wire->b, &end.x, &end.y);
        count = wire_path(wire, start, end, app->round_wires ? 18.0f : 0.0f, path);
        if (bench_is_board(app->bench.parts[wire->a.component].type))
            for (k = 1; k < count; ++k)
                if (hypotf(path[k].x - start.x, path[k].y - start.y) > .01f)
                {
                    wire_contact_tip(start, path[k], wire->color);
                    break;
                }
        if (bench_is_board(app->bench.parts[wire->b.component].type))
            for (k = count - 2; k >= 0; --k)
                if (hypotf(path[k].x - end.x, path[k].y - end.y) > .01f)
                {
                    wire_contact_tip(end, path[k], wire->color);
                    break;
                }
    }
}

static void pin_role_color(ComponentPinRole role)
{
    if (dark_interface && !interface_layer)
    {
        if (role == PIN_SUPPLY_POSITIVE || role == PIN_ANODE)
            color(1, .56f, .45f);
        else if (role == PIN_SUPPLY_RETURN || role == PIN_GROUND || role == PIN_CATHODE)
            color(.53f, .73f, 1);
        else if (role == PIN_INPUT || role == PIN_CLOCK_INPUT || role == PIN_BASE)
            color(1, .76f, .40f);
        else if (role == PIN_OUTPUT)
            color(.47f, .91f, .65f);
        else
            color(.76f, .82f, .88f);
        return;
    }
    if (role == PIN_SUPPLY_POSITIVE || role == PIN_ANODE)
        color(.78f, .22f, .15f);
    else if (role == PIN_SUPPLY_RETURN || role == PIN_GROUND || role == PIN_CATHODE)
        color(.15f, .37f, .7f);
    else if (role == PIN_INPUT || role == PIN_CLOCK_INPUT || role == PIN_BASE)
        color(.65f, .36f, .1f);
    else if (role == PIN_OUTPUT)
        color(.08f, .48f, .28f);
    else
        color(.2f, .28f, .36f);
}

static char pin_role_symbol(ComponentPinRole role)
{
    static const char symbols[] = {'B', 'I', 'C', 'O', '+', '-', 'G', 'A', 'K', '.', 'C', 'B', 'E', 'D'};
    return (unsigned)role < sizeof(symbols) / sizeof(symbols[0]) ? symbols[role] : '?';
}

/* Terminal labels are laid out in screen space: each goes outward from the body and, when it would
 * collide with a label already placed, moves one lane further out with a leader line to its pin. */
#define PIN_LABEL_MAX 16
#define PIN_LABEL_GAP 3.0f
#define PIN_LABEL_LANES 10

typedef struct
{
    float x, y, w, h;
} LabelBox;

typedef struct
{
    int pin;
    float sx, sy;
    int vertical, outward;
    char text[40];
} PinLabel;

static int label_boxes_overlap(LabelBox a, LabelBox b)
{
    return a.x < b.x + b.w + PIN_LABEL_GAP && b.x < a.x + a.w + PIN_LABEL_GAP && a.y < b.y + b.h + 1 && b.y < a.y + a.h + 1;
}

static int label_box_free(LabelBox box, const LabelBox *placed, int count)
{
    for (int i = 0; i < count; ++i)
        if (label_boxes_overlap(box, placed[i]))
            return 0;
    return 1;
}

static LabelBox place_pin_label(GLuint font, const PinLabel *label, const LabelBox *placed, int count)
{
    float w = (float)ui_text_width(font, label->text) + 6, h = (float)ui_font_pixels + 4, step = h + 1;
    LabelBox box = {0, 0, w, h};
    for (int lane = 0; lane < PIN_LABEL_LANES; ++lane)
    {
        if (label->vertical)
        {
            box.x = label->sx - w / 2;
            box.y = label->outward > 0 ? label->sy + 8 + lane * step : label->sy - 8 - h - lane * step;
        }
        else
        {
            box.x = label->outward > 0 ? label->sx + 8 : label->sx - 8 - w;
            box.y = label->sy - h - 4 + lane * step;
        }
        if (label_box_free(box, placed, count))
            break;
    }
    return box;
}

static int compare_pin_labels(const void *left, const void *right)
{
    const PinLabel *a = left, *b = right;
    if (a->vertical != b->vertical || a->outward != b->outward)
        return a->vertical * 2 + a->outward - (b->vertical * 2 + b->outward);
    float ka = a->vertical ? a->sx : a->sy, kb = b->vertical ? b->sx : b->sy;
    return (ka > kb) - (ka < kb);
}

static int collect_pin_labels(const AppState *app, const Part *part, int only_pin, PinLabel *labels)
{
    float cx, cy;
    bench_view_to_screen(&app->view, part->x, part->y, &cx, &cy);
    int count = 0;
    for (int pin = 0; pin < bench_pin_count(part->type) && count < PIN_LABEL_MAX; ++pin)
    {
        if (only_pin >= 0 && pin != only_pin)
            continue;
        const ComponentPinInfo *info = component_pin_info(part->type, pin);
        const char *marker = component_pin_marker(info->role);
        PinLabel *label = &labels[count];
        float x, y;
        bench_pin_position(part, pin, &x, &y);
        bench_view_to_screen(&app->view, x, y, &label->sx, &label->sy);
        float dx = label->sx - cx, dy = label->sy - cy;
        label->pin = pin;
        label->vertical = fabsf(dy) > fabsf(dx);
        label->outward = (label->vertical ? dy : dx) >= 0 ? 1 : -1;
        /* Contacts and passive terminals show only the name; the hover help states the function. */
        int terse = !strcmp(info->name, marker) || info->role == PIN_CONTACT || info->role == PIN_PASSIVE;
        if (terse)
            snprintf(label->text, sizeof(label->text), "%s", info->name);
        else
            snprintf(label->text, sizeof(label->text), "%s %s", info->name, marker);
        ++count;
    }
    qsort(labels, (size_t)count, sizeof(labels[0]), compare_pin_labels);
    return count;
}

static void draw_pin_leader(const Part *part, const PinLabel *label, LabelBox box)
{
    float anchor_x = fmaxf(box.x, fminf(box.x + box.w, label->sx));
    float anchor_y = label->vertical ? (label->outward > 0 ? box.y : box.y + box.h) : box.y + box.h;
    pin_role_color(component_pin_info(part->type, label->pin)->role);
    line(label->sx, label->sy, anchor_x, anchor_y);
}

static void draw_pin_label(GLuint font, const Part *part, const PinLabel *label, LabelBox box)
{
    const ComponentPinInfo *info = component_pin_info(part->type, label->pin);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (dark_interface)
        glColor4f(.07f, .10f, .14f, .82f);
    else
        glColor4f(.97f, .98f, .99f, .86f);
    rounded_rect(box.x, box.y, box.w, box.h, 4);
    glDisable(GL_BLEND);
    pin_role_color(info->role);
    text(font, box.x + 3, box.y + box.h - 4, label->text);
}

static void draw_pin_name_labels(const AppState *app, GLuint font, const Part *part, int only_pin)
{
    PinLabel labels[PIN_LABEL_MAX];
    LabelBox placed[PIN_LABEL_MAX];
    int count = collect_pin_labels(app, part, only_pin, labels);
    glPushMatrix();
    glLoadIdentity();
    for (int i = 0; i < count; ++i)
        placed[i] = place_pin_label(font, &labels[i], placed, i);
    /* Leaders first, labels on top: a leader never crosses another label text. */
    for (int i = 0; i < count; ++i)
        draw_pin_leader(part, &labels[i], placed[i]);
    for (int i = 0; i < count; ++i)
        draw_pin_label(font, part, &labels[i], placed[i]);
    glPopMatrix();
}

/* Terminal names: all of the selected part when the option is on; otherwise only the hovered terminal. */
static void draw_selected_pin_labels(const AppState *app, GLuint font)
{
    if (!app->show_pin_labels)
    {
        Pin pin = app->hovered_pin;
        if (app->hover_revision != app->bench.revision || pin.component < 0 || pin.component >= app->bench.part_count)
            return;
        const Part *hovered = &app->bench.parts[pin.component];
        if (!hovered->group && !bench_is_board(hovered->type))
            draw_pin_name_labels(app, font, hovered, pin.pin);
        return;
    }
    if (app->selected < 0)
        return;
    const Part *part = &app->bench.parts[app->selected];
    if (part->group || bench_is_board(part->type))
        return;
    if (!component_has_flag(part->type, COMPONENT_DIP))
    {
        draw_pin_name_labels(app, font, part, -1);
        return;
    }
    /* 14-pin ICs: one role symbol per pin fits the 14-unit pitch; names are in the hover help. */
    for (int pin = 0; pin < bench_pin_count(part->type); ++pin)
    {
        const ComponentPinInfo *info = component_pin_info(part->type, pin);
        char label[] = {pin_role_symbol(info->role), 0};
        float x, y;
        bench_pin_position(part, pin, &x, &y);
        pin_role_color(info->role);
        if (part->rotation % 2)
            world_text(font, x + (x < part->x ? -16 : 10) / app->view.zoom, y + 5 / app->view.zoom, label, app->view.zoom);
        else
            text(font, x - 4, y + (y < part->y ? -9 : 20), label);
    }
}

static void draw_workspace_labels(const AppState *app, GLuint font)
{
    int i;
    if (!labels_visible(app))
    {
        draw_selected_pin_labels(app, font);
        return;
    }
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && component_has_flag(app->bench.parts[i].type, COMPONENT_DIP))
        {
            color(.85f, .88f, .9f);
            const Part *part = &app->bench.parts[i];
            const char *symbol = component_symbol(part->type);
            float body_width = (bench_pin_count(part->type) / 2 - 1) * BENCH_DIP_PITCH + 14;
            float room = (part->rotation % 2 ? 26 : body_width) - 10;
            float width = (float)app_text_utf8_width(symbol, 2048);
            float scale = bench_part_scale(part) * fminf(1, room / fmaxf(1, width));
            glPushMatrix();
            glTranslatef(part->x, part->y, 0);
            glScalef(scale, scale, 1);
            text(font, -width / 2, 5, symbol);
            glPopMatrix();
        }
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && is_gate(app->bench.parts[i].type))
        {
            if (app->dark_theme)
                color(.76f, .82f, .88f);
            else
                color(.13f, .22f, .27f);
            world_text(font, app->bench.parts[i].x - 25 / app->view.zoom,
                       app->bench.parts[i].y + 35 * bench_part_scale(&app->bench.parts[i]) + 18 / app->view.zoom,
                       component_symbol(app->bench.parts[i].type), app->view.zoom);
        }
    draw_selected_pin_labels(app, font);
}

static void draw_workspace_probe(const AppState *app, GLuint font)
{
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->sim.trace_current == 2 && app->sim.trace_wire >= 0 &&
        app->sim.trace_wire < app->bench.wire_count)
    {
        const Wire *wire = &app->bench.wires[app->sim.trace_wire];
        float ax, ay, bx, by;
        bench_terminal_position(&app->bench, wire->a, &ax, &ay);
        bench_terminal_position(&app->bench, wire->b, &bx, &by);
        color(.9f, .35f, .05f);
        circle(ax, ay, 9);
        circle(bx, by, 9);
        color(.12f, .16f, .19f);
        text(font, ax + 10, ay - 5, "A");
        text(font, bx + 10, by - 5, "B");
    }
}

/* Warning marker: an amber ring on the stressed terminal (or the part centre for a supply warning). */
static void alert_draw(const AppState *app, int index)
{
    const Part *part = &app->bench.parts[index];
    float x = part->x, y = part->y;
    if (app->sim.alert[index] == PART_ALERT_OVERCURRENT || app->sim.alert[index] == PART_ALERT_FLOATING_INPUT)
        bench_pin_position(part, app->sim.alert_pin[index], &x, &y);
    float pulse = .5f + .5f * sinf((float)SDL_GetTicks() * .006f);
    glLineWidth(2.5f);
    glColor3f(.98f, .62f + .15f * pulse, .10f);
    circle_outline(x, y, 10 + 3 * pulse);
    glLineWidth(1);
}

static void draw_workspace_burns(const AppState *app, GLuint font)
{
    int i;
    for (i = 0; i < app->bench.part_count; ++i)
        if (app->sim.burnt[i] && !app->bench.parts[i].group)
            burnt_draw(font, &app->bench.parts[i], SDL_GetTicks());
        else if ((app->sim.alert[i] == PART_ALERT_OVERCURRENT || app->sim.alert[i] == PART_ALERT_OVERVOLTAGE ||
                  app->sim.alert[i] == PART_ALERT_FLOATING_INPUT) &&
                 !app->bench.parts[i].group && sim_state_has_result(&app->sim, SIM_RESULT_DC))
            alert_draw(app, i);
}

/* Displays inside a packed block are mirrored on its compact body, right-aligned, with live data. */
static void draw_group_displays(const AppState *app, int group, float width, float height)
{
    const BenchGroup *body = &app->bench.groups[group - 1];
    const float display_w = 60, display_h = 88, gap = 6, margin = 8;
    float scale = fminf(1, (height - 2 * margin) / display_h);
    float right = body->x + width * .5f - margin;
    for (int i = app->bench.part_count - 1; i >= 0; --i)
    {
        const Part *member = &app->bench.parts[i];
        if (member->group != group || (member->type != PART_SEVEN_SEGMENT && member->type != PART_RGB_LED))
            continue;
        Part view = *member; /* borrowed view: terminals are not owned here */
        view.x = 0;
        view.y = 0;
        set_device_frame(app, i);
        glPushMatrix();
        glTranslatef(right - display_w * scale * .5f, body->y, 0);
        glScalef(scale, scale, 1);
        readable_begin(0);
        draw_device(&view);
        readable_end();
        glPopMatrix();
        right -= display_w * scale + gap;
    }
    device_frame = (DeviceFrame){0};
}

/* Terminal name inside the package, next to its terminal and aligned to that edge (like an IC symbol).
 * Returns 1 when the terminal has a name, even if it is hidden by the zoom. */
static int draw_group_port_name(const AppState *app, GLuint font, const Part *part, int pin, int left_side,
                                float left, float width, float y)
{
    const float inset = 5, baseline = 4;
    char name[BENCH_PORT_NAME_CAPACITY];
    if (!bench_port_name(part, pin, name, sizeof(name)))
        return 0;
    if (!labels_visible(app))
        return 1;
    /* name_text draws in world units (it scales with the zoom), so widths need no zoom correction. */
    float text_width = fminf((float)app_text_utf8_width(name, 2048), width * .5f);
    float x = left_side ? left + inset : left + width - inset - text_width;
    glColor3f(.80f, .86f, .90f);
    readable_begin(x + text_width * .5f);
    name_text(font, x, y + baseline, name, (int)(width * .5f));
    readable_end();
    return 1;
}

static void draw_groups(const AppState *app, GLuint font)
{
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
    {
        const BenchGroup *group = &app->bench.groups[g];
        if (!group->active || group->parent)
            continue;
        float height = bench_group_height(&app->bench, g + 1);
        float width = bench_group_width(&app->bench, g + 1);
        int selected = 0, lit = 0, burnt = 0;
        for (int i = 0; i < app->bench.part_count; ++i)
            if (app->bench.parts[i].group == g + 1)
            {
                selected |= app->selection.parts[i] || app->selected == i;
                lit |= app->leds[i] == 1 && app->bench.parts[i].type == PART_LED;
                burnt |= app->sim.burnt[i];
            }
        /* Same package language as the DIP ICs: dark molded body, pin-1 notch, metal legs, small pads. */
        glPushMatrix();
        glTranslatef(group->x, group->y, 0);
        glRotatef(group->rotation * 90.0f + group->rotation_diagonal * 45.0f, 0, 0, 1);
        int previous_mirror = drawing_mirrored;
        drawing_mirrored = group->mirrored;
        if (group->mirrored)
            glScalef(-1, 1, 1);
        glTranslatef(-group->x, -group->y, 0);
        float left = group->x - width * .5f, top = group->y - height * .5f;
        glColor3f(.10f, .11f, .12f);
        rect(left + 2, top + 3, width, height);
        glColor3f(burnt ? .40f : .24f, burnt ? .16f : .27f, burnt ? .14f : .30f);
        rect(left, top, width, height);
        glColor3f(.18f, .20f, .22f);
        rect(left, top + height - 4, width, 4);
        if (selected)
        {
            glColor3f(.12f, .49f, .84f);
            glLineWidth(2);
            glBegin(GL_LINE_LOOP);
            glVertex2f(left - 3, top - 3);
            glVertex2f(left + width + 3, top - 3);
            glVertex2f(left + width + 3, top + height + 3);
            glVertex2f(left - 3, top + height + 3);
            glEnd();
            glLineWidth(1);
        }
        char label[40];
        snprintf(label, sizeof(label), tr(TXT_CHIP_DEFAULT_NAME), g + 1);
        glColor3f(.86f, .89f, .91f);
        if (lit)
        {
            color(.3f, .95f, .45f);
            circle(group->x + width * .5f - 13, group->y - height * .5f + 8, 3);
        }
        draw_group_displays(app, g + 1, width, height);
        int named = 0;
        Pin pin;
        for (int index = 0; bench_group_port(&app->bench, g + 1, index, &pin); ++index)
        {
            float x, y;
            bench_terminal_position(&app->bench, pin, &x, &y);
            WirePoint local = bench_group_to_local(group, (WirePoint){x, y});
            x = group->x + local.x;
            y = group->y + local.y;
            float edge = x < group->x ? left : left + width;
            glColor3f(.70f, .72f, .74f);
            rect(fminf(x, edge), y - 1.5f, fabsf(edge - x), 3);
            Pin contact;
            if (app->show_connection_pins &&
                bench_find_contact(&app->bench, pin.component, pin.pin, &contact))
                color(.15f, .66f, .79f);
            circle(x, y, 3.5f);
            named |= draw_group_port_name(app, font, &app->bench.parts[pin.component], pin.pin, x < group->x, left, width, y);
            if ((!selected && !app->show_pin_labels) || !labels_visible(app))
                continue;
            const ComponentPinInfo *info = component_pin_info(app->bench.parts[pin.component].type, pin.pin);
            pin_role_color(info->role);
            snprintf(label, sizeof(label), "%d", index + 1);
            float number_x = x + (x < group->x ? -14 : 6) / app->view.zoom;
            float number_width = app_text_utf8_width(label, 2048) / app->view.zoom;
            readable_begin(number_x + number_width * .5f);
            world_text(font, number_x, y - 6 / app->view.zoom, label, app->view.zoom);
            readable_end();
        }
        if (!named)
        {
            /* Pin-1 dot; named terminals already identify the pins. */
            glColor3f(.70f, .74f, .76f);
            circle(left + 9, top + 9, 2.5f);
        }
        glPopMatrix();
        drawing_mirrored = previous_mirror;
        snprintf(label, sizeof(label), tr(TXT_CHIP_DEFAULT_NAME), g + 1);
        float label_width = group->rotation % 2 ? height : width;
        const char *chip_name = group->name[0] ? group->name : label;
        float text_width = fmaxf(1, (float)app_text_utf8_width(chip_name, 2048));
        float text_scale = fminf(1, fmaxf(1, label_width - 6) / text_width);
        text_scale = fminf(text_scale, fmaxf(1, height - 6) / 16);
        glColor3f(.86f, .89f, .91f);
        glPushMatrix();
        glTranslatef(group->x, group->y, 0);
        glScalef(text_scale, text_scale, 1);
        name_text(font, -text_width * .5f, 5, chip_name, (int)ceilf(text_width) + 2);
        glPopMatrix();
    }
}

static void draw_selection_area(const AppState *app)
{
    if (!app->selecting_area)
        return;
    color(.12f, .45f, .86f);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    glVertex2f(app->area_start.x, app->area_start.y);
    glVertex2f(app->area_end.x, app->area_start.y);
    glVertex2f(app->area_end.x, app->area_end.y);
    glVertex2f(app->area_start.x, app->area_end.y);
    glEnd();
    glLineWidth(1);
}

static void draw_workspace(const AppState *app, GLuint font, int width, int height)
{
    glEnable(GL_SCISSOR_TEST);
    AppRect viewport = app_layout_viewport(width, height);
    float scale = viewport.w / app_layout_canvas_width();
    glScissor((int)(viewport.x + BENCH_VIEW_LEFT * scale), (int)(height - viewport.y - BENCH_VIEW_BOTTOM * scale),
              (int)(BENCH_VIEW_WIDTH * scale), (int)(BENCH_VIEW_HEIGHT * scale));
    glPushMatrix();
    glTranslatef(220 + app->view.pan_x, 70 + app->view.pan_y, 0);
    glScalef(app->view.zoom, app->view.zoom, 1);
    glTranslatef(-220, -70, 0);
    draw_workspace_grid(app);
    draw_workspace_boards(app);
    draw_workspace_wires(app);
    draw_workspace_route(app);
    /* Wire tips in breadboard holes belong under the parts plugged on top of them. */
    draw_workspace_contacts(app);
    draw_workspace_parts(app);
    for (int i = 0; i < app->bench.part_count; ++i)
    {
        const Part *part = &app->bench.parts[i];
        char name[BENCH_PORT_NAME_CAPACITY];
        if (!part->group && part->type == PART_JUNCTION && bench_port_name(part, 0, name, sizeof(name)))
        {
            color(UI_INK);
            name_text(font, part->x + 10, part->y - 10, name, 150);
        }
    }
    for (int i = 0; i < app->bench.part_count; ++i)
    {
        const Part *part = &app->bench.parts[i];
        if (part->type != PART_NOTE || part->group)
            continue;
        float scale = bench_part_scale(part);
        float w = 240 * scale, h = 110 * scale;
        float x = part->x - w / 2, y = part->y - h / 2;
        const float *fg = palette_tone(((part->states >> 8) + BENCH_COLOR_BLACK) % BENCH_COLOR_COUNT)->wire;
        draw_note_paper(part, w, h);
        glColor3fv(fg);
        ui_wrapped(font, x + 12, y + 36, part->code ? part->code : tr(TXT_UI_NOTE_HINT),
                   (int)fmaxf(1, w - 24), (int)fmaxf(1, (h * .90f - 42) / 18 + 1), 18);
        if (app->selected == i || app->selection.parts[i])
        {
            glColor3f(.12f, .49f, .84f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
            glEnd();
            rect(x + w - 10, y + h - 10, 8, 8);
        }
    }
    draw_groups(app, font);
    draw_workspace_labels(app, font);
    draw_workspace_probe(app, font);
    draw_workspace_burns(app, font);
    draw_selection_area(app);
    glPopMatrix();
    glDisable(GL_SCISSOR_TEST);
}

/* Compact inspector baselines (see app_layout.c for the panel geometry). */
#define INSPECTOR_LINE_1 194.0f
#define INSPECTOR_LINE_2 216.0f
#define INSPECTOR_NOTE_TOP 280.0f
#define INSPECTOR_LABEL_W 84.0f
#define INSPECTOR_NOTE_LINES 2
#define INSPECTOR_BOTTOM_PAD 12.0f
#define INSPECTOR_LED_HEIGHT 176.0f
#define INSPECTOR_ENVIRONMENT_HEIGHT 228.0f
#define INSPECTOR_GROUP_HEIGHT 190.0f
#define INSPECTOR_WIRE_NOTE 268.0f

static float inspector_text_x(void)
{
    return app_layout_inspector_value().x;
}

static int inspector_text_w(void)
{
    return (int)app_layout_inspector_value().w;
}

static float inspector_line_height(void)
{
    return ui_font_pixels + 2.0f;
}

/* Panel height that ends just below the last text line starting at baseline. */
static float inspector_height_to(float baseline, int lines)
{
    AppRect panel = app_layout_inspector(0);
    return baseline + (lines - 1) * inspector_line_height() + INSPECTOR_BOTTOM_PAD - panel.y;
}

static int inspector_note_lines(GLuint font, const char *note)
{
    return ui_text_width(font, note) > inspector_text_w() ? INSPECTOR_NOTE_LINES : 1;
}

static void inspector_note_text(GLuint font, float baseline, const char *note)
{
    color(UI_MUTED);
    ui_wrapped(font, inspector_text_x(), baseline, note, inspector_text_w(), INSPECTOR_NOTE_LINES,
               inspector_line_height());
}

static void draw_inspector_title(const AppState *app, GLuint font, const char *name, float right)
{
    AppRect minimize = app_layout_inspector_minimize();
    color(UI_INK);
    name_text(font, inspector_text_x(), ui_baseline(minimize), name, (int)(right - inspector_text_x() - 8));
    if (ui_hovered(app, minimize))
    {
        color(UI_SURFACE_ALT);
        ui_rounded(minimize, UI_RADIUS);
    }
    color(UI_MUTED);
    glLineWidth(2);
    float cx = app_rect_center_x(minimize), cy = app_rect_center_y(minimize), arm = 6;
    line(cx - arm, cy + 3, cx + arm, cy + 3);
    glLineWidth(1);
}

/* Collapsed inspector: a small tab that keeps the name visible and expands on click. */
static void draw_inspector_tab(const AppState *app, GLuint font, const char *name)
{
    AppRect tab = app_layout_inspector_tab();
    const float icon_w = 24;
    ui_panel(tab);
    if (ui_hovered(app, tab))
    {
        color(UI_SURFACE_ALT);
        rounded_rect(tab.x + 1, tab.y + 1, tab.w - 2, tab.h - 2, UI_RADIUS + 1);
    }
    color(UI_INK);
    name_text(font, tab.x + 10, ui_baseline(tab), name, (int)(tab.w - 20 - icon_w));
    color(UI_MUTED);
    glLineWidth(2);
    float cx = tab.x + tab.w - 10 - icon_w / 2, cy = app_rect_center_y(tab), arm = 5;
    line(cx - arm, cy - 2, cx, cy + 3);
    line(cx, cy + 3, cx + arm, cy - 2);
    glLineWidth(1);
}

static void draw_inspector_row(const AppState *app, GLuint font, AppRect row, const char *label, const char *value,
                               int editable)
{
    if (editable)
        ui_row_hover(app, row);
    color(UI_MUTED);
    text(font, row.x, ui_baseline(row), label);
    AppRect box = {row.x + INSPECTOR_LABEL_W, row.y + 2, row.w - INSPECTOR_LABEL_W, row.h - 4};
    if (editable)
        ui_value_box(font, box, value, 1);
    else
    {
        color(UI_MUTED);
        text(font, box.x + 10, ui_baseline(row), value);
    }
}

static void draw_group_inspector(const AppState *app, GLuint font)
{
    int group = app->bench.parts[app->selected].group;
    ui_panel(app_layout_inspector(INSPECTOR_GROUP_HEIGHT));
    draw_inspector_title(app, font, app->bench.groups[group - 1].name, app_layout_inspector_minimize().x);
    ui_button(app, font, app_layout_group_button(0), tr(TXT_UI_VIEW_INSIDE), UI_BUTTON_PRIMARY);
    char dimensions[100];
    snprintf(dimensions, sizeof(dimensions), tr(TXT_UI_CHIP_DIMENSIONS), bench_group_width(&app->bench, group),
             bench_group_height(&app->bench, group));
    AppRect button = app_layout_group_button(0);
    char size[32];
    snprintf(size, sizeof(size), "%.0f %%", bench_group_width(&app->bench, group) * 100 / 98);
    draw_inspector_row(app, font, app_layout_inspector_scale(), tr(TXT_UI_SIZE), size, 1);
    ui_note(font, inspector_text_x(), button.y + button.h + 58, dimensions, inspector_text_w());
    ui_note(font, inspector_text_x(), button.y + button.h + 81,
            tr(TXT_UI_CHIP_RESIZE_HINT), inspector_text_w());
    ui_note(font, inspector_text_x(), button.y + button.h + 102,
            tr(TXT_UI_CHIP_RESIZE_WIDTH_HINT), inspector_text_w());
}

/* Datasheet warning of the selected part, shown in place of its current reading. */
static int part_alert_reading(const AppState *app, const Part *part, char *reading, size_t capacity)
{
    int index = app->selected;
    double absolute, recommended, output, value = app->sim.alert_value[index];
    netlist_logic_ratings(part->type, &absolute, &recommended, &output);
    if (!sim_state_has_result(&app->sim, SIM_RESULT_DC))
        return 0;
    if (app->sim.alert[index] == PART_ALERT_OVERCURRENT)
    {
        const ComponentPinInfo *pin = component_pin_info(part->type, app->sim.alert_pin[index]);
        snprintf(reading, capacity, tr(TXT_UI_ALERT_CURRENT), pin && pin->name ? pin->name : "",
                 value * 1000.0, output * 1000.0);
    }
    else if (app->sim.alert[index] == PART_ALERT_OVERVOLTAGE)
        snprintf(reading, capacity, tr(TXT_UI_ALERT_VOLTAGE), value, recommended);
    else if (app->sim.alert[index] == PART_ALERT_FLOATING_INPUT)
    {
        const ComponentPinInfo *pin = component_pin_info(part->type, app->sim.alert_pin[index]);
        snprintf(reading, capacity, tr(TXT_UI_ALERT_FLOATING), pin && pin->name ? pin->name : "");
    }
    else if (app->sim.alert[index] == PART_ALERT_CURRENT_LIMIT)
        snprintf(reading, capacity, tr(TXT_UI_ALERT_LIMITING), value * 1000.0);
    else
        return 0;
    return 1;
}

static void inspector_reading(const AppState *app, const Part *part, char *reading, size_t capacity)
{
    const BurnLimits limits = burn_limits_default();
    int index = app->selected;
    if (app->sim.burnt[index])
    {
        double absolute, recommended, output;
        netlist_logic_ratings(part->type, &absolute, &recommended, &output);
        if (app->sim.burn_reason[index] == BURN_SHORT)
            snprintf(reading, capacity, "%s", tr(TXT_UI_BURNED_SHORT));
        else if (app->sim.burn_reason[index] == BURN_PIN_VOLTAGE)
            snprintf(reading, capacity, tr(TXT_UI_BURNED_PIN_VOLTAGE), app->sim.burn_value[index]);
        else if (app->sim.burn_reason[index] == BURN_OVERVOLTAGE)
            snprintf(reading, capacity, tr(TXT_UI_BURNED_VOLTAGE), app->sim.burn_value[index], absolute);
        else if (app->sim.burn_reason[index] == BURN_OVERCURRENT && !burn_is_led(part->type))
            snprintf(reading, capacity, tr(TXT_UI_BURNED_CURRENT), app->sim.burn_value[index] * 1000.0,
                     output * 1000.0);
        else if (app->sim.burn_reason[index] == BURN_OVERPOWER)
            snprintf(reading, capacity, tr(TXT_UI_BURNED_POWER), app->sim.burn_value[index],
                     limits.resistor_max_power);
        else
            snprintf(reading, capacity, tr(TXT_UI_BURNED_CURRENT), app->sim.burn_value[index] * 1000.0,
                     limits.led_max_current * 1000.0);
        return;
    }
    if (part_alert_reading(app, part, reading, capacity))
        return;
    char vector[64];
    if (!sim_state_has_result(&app->sim, SIM_RESULT_DC) ||
        !component_current_vector(part->type, index, vector, sizeof(vector)))
    {
        reading[0] = 0;
        return;
    }
    const char *direction = tr(TXT_UI_DIRECTION_TERMINALS);
    if (part->type == PART_NPN || part->type == PART_PNP || part->type == PART_AMBIENT_LIGHT)
        direction = "C → E";
    else if (part->type == PART_TEMPERATURE)
        direction = tr(TXT_UI_DIRECTION_TO_LOAD);
    TextId format = app->sim.display_pending ? TXT_UI_CURRENT_READING_UPDATING : TXT_UI_CURRENT_READING;
    snprintf(reading, capacity, tr(format), app->sim.dc_led_currents[index] * 1000.0, direction);
}

static void lit_segments_note(const double *e, char *note, size_t capacity)
{
    static const char *names[COMPONENT_SEGMENTS] = {"a", "b", "c", "d", "e", "f", "g", "dp"};
    char lit[COMPONENT_SEGMENTS * 3 + 1] = "";
    size_t used = 0;
    for (int segment = 0; segment < COMPONENT_SEGMENTS; ++segment)
    {
        if (e[segment] <= 1e-4)
            continue;
        used += (size_t)snprintf(lit + used, sizeof(lit) - used, used ? " %s" : "%s", names[segment]);
    }
    if (used)
        snprintf(note, capacity, tr(TXT_UI_SEGMENTS_LIT), lit);
    else
        snprintf(note, capacity, "%s", tr(TXT_UI_SEGMENTS_NONE));
}

static void device_note(const Part *part, const double *e, char *note, size_t capacity)
{
    const double rpm = 60 / 6.283185307179586;
    switch (part->type)
    {
    case PART_RGB_LED:
        snprintf(note, capacity, "R %.2f mA · G %.2f mA · B %.2f mA", e[0] * 1e3, e[1] * 1e3, e[2] * 1e3);
        break;
    case PART_SEVEN_SEGMENT:
        lit_segments_note(e, note, capacity);
        break;
    case PART_DIP_SWITCH:
        snprintf(note, capacity, tr(TXT_UI_DIP_CONTACTS), part->states & 1 ? "ON" : "off",
                 part->states & 2 ? "ON" : "off", part->states & 4 ? "ON" : "off", part->states & 8 ? "ON" : "off");
        break;
    case PART_RELAY:
        snprintf(note, capacity, tr(e[1] > .5 ? TXT_UI_RELAY_CLOSED : TXT_UI_RELAY_OPEN), e[0] * 1e3);
        break;
    case PART_DC_MOTOR:
    case PART_VIBRATION_MOTOR:
    case PART_ENCODER_MOTOR:
        snprintf(note, capacity, tr(TXT_UI_MOTOR_NOTE), e[0] * rpm, e[1] * 1e3);
        break;
    case PART_SERVO:
        snprintf(note, capacity, tr(TXT_UI_SERVO_NOTE), e[0]);
        break;
    case PART_PIEZO:
        snprintf(note, capacity, "%s", tr(fabs(e[0]) > 1e-5 ? TXT_UI_PIEZO_VIBRATING : TXT_UI_PIEZO_STOPPED));
        break;
    case PART_FUNCTION_GENERATOR:
        snprintf(note, capacity, tr(TXT_UI_GENERATOR_NOTE), component_mode_label(part->type, part->states));
        break;
    case PART_MULTIMETER:
        if (part->states == 1)
            snprintf(note, capacity, tr(TXT_UI_METER_CURRENT), e[1] * 1e3);
        else if (part->states == 2)
            snprintf(note, capacity, tr(TXT_UI_METER_RESISTANCE), e[0] / COMPONENT_METER_OHMS_TEST_AMPS);
        else
            snprintf(note, capacity, tr(TXT_UI_METER_VOLTAGE), e[0]);
        break;
    case PART_OSCILLOSCOPE:
        snprintf(note, capacity, tr(TXT_UI_SCOPE_NOTE), e[0]);
        break;
    default:
        note[0] = 0;
    }
}

static void inspector_note(const Part *part, char *note, size_t capacity)
{
    note[0] = 0;
    if (part->type == PART_POTENTIOMETER)
        snprintf(note, capacity, tr(TXT_UI_WIPER_POSITION), part->control * 100);
    else if (component_electrical_type(part->type) == PART_SOURCE && part->type != PART_SOURCE)
        snprintf(note, capacity, tr(TXT_UI_SOURCE_NOTE),
                 component_source_resistance(part->type));
    else if (part->type == PART_DIODE || part->type == PART_NPN || part->type == PART_PNP)
        snprintf(note, capacity, "%s", tr(TXT_UI_GENERIC_SPICE_MODEL));
}

static void draw_environment_control(GLuint font, const Part *part)
{
    const ComponentEnvironment *environment = component_environment(part->type);
    char reading[128];
    snprintf(reading, sizeof(reading), "%s: %.3g %s", environment->label, component_environment_value(part),
             environment->unit);
    color(UI_INK);
    text(font, inspector_text_x(), INSPECTOR_LINE_2, reading);
    AppRect track = app_layout_environment_track();
    color(.80f, .84f, .87f);
    ui_rounded(track, track.h / 2);
    glColor3f(UI_ACCENT);
    rounded_rect(track.x, track.y, track.w * (float)part->control, track.h, track.h / 2);
    float knob_x = track.x + track.w * (float)part->control, knob_y = app_rect_center_y(track);
    circle(knob_x, knob_y, 9);
    ui_raw_white();
    circle(knob_x, knob_y, 5);
    inspector_note_text(font, INSPECTOR_NOTE_TOP, environment->note);
}

/* Row of palette swatches; the chosen one gets an accent ring. LEDs show all but black. */
static void draw_color_swatches(AppRect (*swatch)(int), int count, int chosen)
{
    for (int index = 0; index < count; ++index)
    {
        AppRect r = swatch(index);
        if (chosen == index)
        {
            glColor3f(UI_ACCENT);
            rounded_rect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, 7);
        }
        color(UI_BORDER);
        ui_rounded(r, 5);
        const float *tone = palette_tone(index)->wire;
        glColor3f(tone[0], tone[1], tone[2]);
        rounded_rect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 4);
    }
}

static void draw_led_colors(GLuint font, const Part *part)
{
    char label[64];
    snprintf(label, sizeof(label), tr(TXT_UI_LED_COLOR), component_color_name(part->states));
    color(UI_INK);
    text(font, inspector_text_x(), INSPECTOR_LINE_2, label);
    draw_color_swatches(app_layout_color_swatch, COMPONENT_LED_COLORS, (int)part->states);
}

static void draw_part_inspector(const AppState *app, GLuint font)
{
    const Part *part = &app->bench.parts[app->selected];
    if (part->type == PART_NOTE)
    {
        ui_panel(app_layout_inspector(230));
        draw_inspector_title(app, font, tr(TXT_CAT_NOTE), app_layout_inspector_minimize().x);
        draw_inspector_row(app, font, app_layout_inspector_value(), tr(TXT_UI_NOTE_TEXT), tr(TXT_UI_NOTE_EDIT), 1);
        char size[32];
        snprintf(size, sizeof(size), "%.5g %%", bench_part_scale(part) * 100);
        draw_inspector_row(app, font, app_layout_inspector_scale(), tr(TXT_UI_SIZE), size, 1);
        color(UI_INK);
        text(font, inspector_text_x(), INSPECTOR_LINE_2, tr(TXT_UI_NOTE_BACKGROUND));
        draw_color_swatches(app_layout_color_swatch, BENCH_COLOR_COUNT, ((part->states & 255) + BENCH_COLOR_YELLOW) % BENCH_COLOR_COUNT);
        glPushMatrix();
        glTranslatef(0, 48, 0);
        color(UI_INK);
        text(font, inspector_text_x(), INSPECTOR_LINE_2, tr(TXT_UI_NOTE_TEXT));
        draw_color_swatches(app_layout_color_swatch, BENCH_COLOR_COUNT, ((part->states >> 8) + BENCH_COLOR_BLACK) % BENCH_COLOR_COUNT);
        glPopMatrix();
        return;
    }
    const ComponentInfo *info = component_info(part->type);
    char reading[160], note[160], value[64];
    inspector_reading(app, part, reading, sizeof(reading));
    inspector_note(part, note, sizeof(note));
    if (component_device(part->type) && !component_environment(part->type))
    {
        static const double none[COMPONENT_MAX_ELEMENTS];
        int measured = sim_state_has_result(&app->sim, SIM_RESULT_DC);
        device_note(part, measured ? app->sim.dc_elements[app->selected] : none, note, sizeof(note));
        if (part->type == PART_MULTIMETER && part->states == 2 &&
            !component_ohmmeter_reading_reliable(&app->bench))
            snprintf(note, sizeof(note), "%s", tr(TXT_UI_METER_RESISTANCE_POWERED));
    }
    int environment = component_environment(part->type) != NULL;
    float height = inspector_height_to(INSPECTOR_LINE_1, 1);
    if (environment)
        height = INSPECTOR_ENVIRONMENT_HEIGHT;
    else if (part->type == PART_LED)
        height = INSPECTOR_LED_HEIGHT;
    else if (note[0])
        height = inspector_height_to(INSPECTOR_LINE_2, inspector_note_lines(font, note));
    ui_panel(app_layout_inspector(height));
    float title_right = app_layout_inspector_minimize().x;
    if (!bench_is_board(part->type))
    {
        title_right = app_layout_inspector_mirror().x;
        ui_button(app, font, app_layout_inspector_rotate(), tr(TXT_UI_ROTATE), UI_BUTTON_NEUTRAL);
        ui_button(app, font, app_layout_inspector_mirror(), tr(TXT_UI_MIRROR), UI_BUTTON_NEUTRAL);
    }
    if (chip_doc(part->type) && !part->group)
    {
        title_right = app_layout_inspector_doc().x;
        ui_button(app, font, app_layout_inspector_doc(), tr(TXT_DOC_OPEN), UI_BUTTON_NEUTRAL);
    }
    int selected_count = bench_selection_count(&app->selection, &app->bench);
    char title[160];
    if (selected_count > 1 && !app_selection_same_type(app))
        snprintf(title, sizeof(title), tr(TXT_UI_SELECTED_MIXED), selected_count);
    else if (selected_count > 1)
        snprintf(title, sizeof(title), tr(TXT_UI_SELECTED_PARTS), selected_count, part_name(part->type));
    else
        snprintf(title, sizeof(title), "%s", part_name(part->type));
    draw_inspector_title(app, font, title, title_right);
    if (mcu_board(part->type))
        draw_inspector_row(app, font, app_layout_inspector_value(), tr(TXT_UI_CODE), tr(TXT_UI_EDIT_CODE), 1);
    else
    {
        int editable = app_selection_value_target(app) >= 0;
        if (info && info->primary)
            snprintf(value, sizeof(value), "%.5g %s", part->value, info->primary->unit);
        for (int i = 0; editable && i < app->bench.part_count; ++i)
            if (app->selection.parts[i] && app->bench.parts[i].value != part->value)
                snprintf(value, sizeof(value), "%s", tr(TXT_UI_MIXED_VALUES));
        draw_inspector_row(app, font, app_layout_inspector_value(), tr(TXT_UI_VALUE),
                           editable ? value : tr(TXT_UI_NO_VALUE), editable);
    }
    snprintf(value, sizeof(value), "%.5g %%", bench_part_scale(part) * 100);
    draw_inspector_row(app, font, app_layout_inspector_scale(), tr(TXT_UI_SIZE), value, app_selection_same_type(app));
    if (app->sim.burnt[app->selected] || app->sim.alert[app->selected] == PART_ALERT_OVERCURRENT ||
        app->sim.alert[app->selected] == PART_ALERT_OVERVOLTAGE ||
        app->sim.alert[app->selected] == PART_ALERT_FLOATING_INPUT)
        color(UI_DANGER);
    else
        color(UI_INK);
    name_text(font, inspector_text_x(), INSPECTOR_LINE_1, reading[0] ? reading : tr(TXT_UI_NO_CURRENT_READING),
              inspector_text_w());
    if (environment)
        draw_environment_control(font, part);
    else if (part->type == PART_LED)
        draw_led_colors(font, part);
    else if (note[0])
        inspector_note_text(font, INSPECTOR_LINE_2, note);
}

/* "Part · terminal" for one end of a wire; chip members show the chip name. */
static void wire_end_text(const AppState *app, Pin end, char *text_out, size_t capacity)
{
    const Part *part = &app->bench.parts[end.component];
    const char *name = part->group ? app->bench.groups[part->group - 1].name : part_name(part->type);
    const ComponentPinInfo *info = component_pin_info(part->type, end.pin);
    char terminal[48];
    if (info && info->name && info->name[0])
        snprintf(terminal, sizeof(terminal), "%s", info->name);
    else
        snprintf(terminal, sizeof(terminal), tr(TXT_UI_WIRE_TERMINAL), end.pin + 1);
    snprintf(text_out, capacity, tr(TXT_UI_WIRE_END), name, terminal);
}

static void wire_current_text(const AppState *app, int wire, char *text_out, size_t capacity)
{
    if (!app->show_current)
    {
        snprintf(text_out, capacity, "%s", tr(TXT_UI_WIRE_CURRENT_OFF));
        return;
    }
    if (!sim_state_has_result(&app->sim, SIM_RESULT_DC) || !app->sim.dc_wire_valid)
    {
        snprintf(text_out, capacity, "%s", tr(TXT_UI_WIRE_NO_CURRENT));
        return;
    }
    const double milliamps_per_amp = 1e3;
    snprintf(text_out, capacity, tr(TXT_UI_WIRE_CURRENT), app->sim.dc_wire_currents[wire] * milliamps_per_amp);
}

static void draw_wire_inspector(const AppState *app, GLuint font)
{
    const Wire *wire = &app->bench.wires[app->selected_wire];
    char from[160], to[160], line_text[200];
    ui_panel(app_layout_wire_inspector());
    draw_inspector_title(app, font, tr(TXT_UI_WIRE_TITLE), app_layout_inspector_minimize().x);
    wire_end_text(app, wire->a, from, sizeof(from));
    wire_end_text(app, wire->b, to, sizeof(to));
    draw_inspector_row(app, font, app_layout_inspector_value(), tr(TXT_UI_WIRE_FROM), from, 0);
    draw_inspector_row(app, font, app_layout_inspector_scale(), tr(TXT_UI_WIRE_TO), to, 0);
    wire_current_text(app, app->selected_wire, line_text, sizeof(line_text));
    color(UI_INK);
    name_text(font, inspector_text_x(), INSPECTOR_LINE_1, line_text, inspector_text_w());
    snprintf(line_text, sizeof(line_text), tr(TXT_UI_WIRE_COLOR), component_color_name((unsigned)wire->color));
    text(font, inspector_text_x(), INSPECTOR_LINE_2, line_text);
    draw_color_swatches(app_layout_color_swatch, BENCH_COLOR_COUNT, wire->color);
    inspector_note_text(font, INSPECTOR_WIRE_NOTE, tr(TXT_UI_WIRE_COLOR_NOTE));
}

static void draw_inspector(const AppState *app, GLuint font)
{
    if (app->settings || app->projects_open || app->examples_open || app->code_part >= 0)
        return;
    if (app->selected < 0 && app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count)
    {
        if (app->inspector_minimized)
            draw_inspector_tab(app, font, tr(TXT_UI_WIRE_TITLE));
        else
            draw_wire_inspector(app, font);
        return;
    }
    if (app->selected < 0)
        return;
    const Part *part = &app->bench.parts[app->selected];
    if (app->inspector_minimized)
        draw_inspector_tab(app, font, part->group ? app->bench.groups[part->group - 1].name : part_name(part->type));
    else if (part->group)
        draw_group_inspector(app, font);
    else
        draw_part_inspector(app, font);
}

#define PART_HINT_MAX 8

/* Key hints of the selected part, joined by " · " (each hint is a whole "Key: action" text). */
static void part_hint(const Part *part, char *hint, size_t capacity)
{
    const ComponentInfo *info = component_info(part->type);
    TextId hints[PART_HINT_MAX];
    int count = 0;
    if (!bench_is_board(part->type))
        hints[count++] = TXT_UI_HINT_ROTATE;
    if (info && info->primary)
        hints[count++] = TXT_UI_HINT_VALUE;
    hints[count++] = TXT_UI_HINT_SIZE;
    if (bench_lead_count(part, 0))
        hints[count++] = TXT_UI_HINT_LEGS;
    if (part->type == PART_POTENTIOMETER)
        hints[count++] = TXT_UI_HINT_WIPER;
    else if (component_environment(part->type))
        hints[count++] = TXT_UI_HINT_ENVIRONMENT;
    hints[count++] = TXT_UI_HINT_GRAPH;
    hints[count++] = TXT_UI_HINT_REPLACE;
    hints[count++] = TXT_UI_HINT_DELETE;
    size_t used = 0;
    hint[0] = 0;
    for (int index = 0; index < count && used < capacity; ++index)
        used += (size_t)snprintf(hint + used, capacity - used, index ? " · %s" : "%s", tr(hints[index]));
}

static void status_hint(const AppState *app, char *hint, size_t capacity)
{
    TextId fixed = TXT_UI_HINT_BENCH;
    if (app->selected >= 0 && app->bench.parts[app->selected].group)
        fixed = TXT_UI_HINT_CHIP;
    else if (app->selected >= 0)
    {
        part_hint(&app->bench.parts[app->selected], hint, capacity);
        return;
    }
    else if (app->selected_wire >= 0)
        fixed = TXT_UI_HINT_WIRE;
    else if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace_automatic && app->show_trace)
        fixed = TXT_UI_HINT_GRAPH_PLAYBACK;
    snprintf(hint, capacity, "%s", tr(fixed));
}

static void draw_status(const AppState *app, GLuint font)
{
    int graph = app_controls_graph_visible(app);
    WirePoint offset = app_layout_graph_offset();
    glPushMatrix();
    glTranslatef(offset.x, offset.y, 0);
    if (app->live_graph.active && graph)
        live_graph_draw(app, font);
    else if (graph)
        trace_draw(font, &app->sim.trace, app->sim.trace_index, app->sim.trace_pin, app->sim.trace_current);
    glPopMatrix();
    if (graph)
    {
        ui_close_button(app, app_layout_graph_close());
        int wire = app->live_graph.active && app->live_graph.probe.kind == SIM_LIVE_PROBE_WIRE_CURRENT;
        int current = app->live_graph.active ? app->live_graph.probe.kind == SIM_LIVE_PROBE_PART_CURRENT : app->sim.trace_current;
        TextId measure = TXT_QUANTITY_VOLTAGE;
        if (wire)
            measure = TXT_UI_GRAPH_WIRE_MEASURE;
        else if (current)
            measure = TXT_QUANTITY_CURRENT;
        ui_button(app, font, app_layout_graph_measure(), tr(measure), wire ? UI_BUTTON_DISABLED : UI_BUTTON_NEUTRAL);
        char terminal[48];
        int pin = app->live_graph.active ? app->live_graph.probe.target.pin : app->sim.trace_pin;
        if (wire || !app->live_graph.active)
            snprintf(terminal, sizeof(terminal), "%s", tr(TXT_SETTINGS_TITLE));
        else
            snprintf(terminal, sizeof(terminal), tr(TXT_TERMINAL_NUMBER), pin + 1);
        ui_button(app, font, app_layout_graph_pin(), terminal, UI_BUTTON_NEUTRAL);
    }
    float right = app_layout_canvas_width(), top = BENCH_VIEW_BOTTOM;
    color(UI_SURFACE);
    rect(APP_PALETTE_WIDTH, top, right - APP_PALETTE_WIDTH, APP_FOOTER_HEIGHT);
    color(UI_BORDER);
    rect(APP_PALETTE_WIDTH, top, right - APP_PALETTE_WIDTH, 1);
    char status[384], hint[320];
    if (app->sim.live_mode && app->sim.live_rate > 0 && app->sim.sim_running)
        snprintf(status, sizeof(status), tr(TXT_UI_LIVE_PACE_STATUS), app->sim.live_time, app->sim.live_rate,
                 app->simulation_speed, app->status);
    else if (app->sim.live_mode)
        snprintf(status, sizeof(status), "t = %.6g s  ·  %s", app->sim.live_time, app->status);
    else
        snprintf(status, sizeof(status), "%s", app->status);
    color(UI_INK);
    int text_width = (int)(right - APP_PALETTE_WIDTH - 40);
    name_text(font, 238, top + 25, status, text_width);
    status_hint(app, hint, sizeof(hint));
    color(UI_MUTED);
    name_text(font, 238, top + 49, hint, text_width);
    if (app->xray)
    {
        const char *badge = tr(TXT_UI_TRANSPARENCY_BADGE);
        float width = (float)app_text_utf8_width(badge, 2048) + 20;
        color(UI_ACCENT);
        rounded_rect(right - 12 - width, top + 8, width, 24, 12);
        glColor3f(1, 1, 1);
        text(font, right - 2 - width, top + 25, badge);
    }
}

static void draw_settings_appearance(const AppState *app, GLuint font)
{
    char value[32];
    ui_section(font, app_layout_settings_section(0, 0), tr(TXT_SETTINGS_APPEARANCE));
    ui_toggle(app, font, app_layout_setting(APP_SETTING_DARK_THEME), tr(TXT_SETTINGS_DARK_THEME), app->dark_theme);
    snprintf(value, sizeof(value), "%d%%", app->gui_font_size * 100 / 15);
    AppRect minus = app_layout_setting(APP_SETTING_FONT_SMALLER), plus = app_layout_setting(APP_SETTING_FONT_LARGER);
    ui_stepper(app, font, (AppRect){app_layout_setting(APP_SETTING_DARK_THEME).x, minus.y, 400, minus.h}, minus, plus,
               tr(TXT_SETTINGS_TEXT_SIZE), value);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_SHOW_GRID), tr(TXT_SETTINGS_SHOW_GRID), app->show_grid);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_ROUND_WIRES), tr(TXT_SETTINGS_ROUND_WIRES),
              app->round_wires);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_HANDLES), tr(TXT_SETTINGS_SHOW_BENDS), app->show_handles);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_PIN_LABELS), tr(TXT_SETTINGS_SHOW_TERMINAL_NAMES), app->show_pin_labels);
}

static void draw_settings_snapping(const AppState *app, GLuint font)
{
    char value[48];
    ui_section(font, app_layout_settings_section(0, 7), tr(TXT_SETTINGS_GRID_AND_SNAP));
    ui_toggle(app, font, app_layout_setting(APP_SETTING_SNAP_GRID), tr(TXT_SETTINGS_SNAP_GRID), app->snap_grid);
    snprintf(value, sizeof(value), "%.6g", app->grid_spacing);
    ui_field(app, font, app_layout_setting(APP_SETTING_GRID_SPACING), tr(TXT_SETTINGS_GRID_SPACING), value);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_SNAP_BOARD), tr(TXT_SETTINGS_SNAP_BREADBOARD), app->snap_board);
}

static void draw_settings_wires(const AppState *app, GLuint font)
{
    ui_section(font, app_layout_settings_section(1, 0), tr(TXT_SETTINGS_SIMULATION));
    ui_toggle(app, font, app_layout_setting(APP_SETTING_CONNECTION_PINS),
              tr(TXT_SETTINGS_SHOW_CONNECTION_PINS), app->show_connection_pins);
    AppRect current = app_layout_setting(APP_SETTING_SHOW_CURRENT);
    ui_toggle(app, font, current, tr(TXT_SETTINGS_SHOW_CURRENT), app->show_current);
    AppRect minus = app_layout_setting(APP_SETTING_SIM_SPEED_SLOWER);
    AppRect plus = app_layout_setting(APP_SETTING_SIM_SPEED_FASTER);
    char pace[32];
    snprintf(pace, sizeof(pace), "%.2gx", app->simulation_speed);
    ui_stepper(app, font, (AppRect){current.x, minus.y, 400, minus.h}, minus, plus,
               tr(TXT_SETTINGS_SIM_SPEED), pace);
}

static void draw_settings_trace(const AppState *app, GLuint font)
{
    char value[48];
    ui_section(font, app_layout_settings_section(1, 4), tr(TXT_SETTINGS_GRAPH));
    ui_field(app, font, app_layout_setting(APP_SETTING_PROBE_MEASURE), tr(TXT_SETTINGS_MEASURE),
             app->probe_current ? tr(TXT_QUANTITY_CURRENT) : tr(TXT_QUANTITY_VOLTAGE));
    snprintf(value, sizeof(value), tr(TXT_TERMINAL_NUMBER), app->probe_pin + 1);
    ui_field(app, font, app_layout_setting(APP_SETTING_PROBE_PIN), tr(TXT_SETTINGS_TERMINAL), value);
    snprintf(value, sizeof(value), "%.6g s", app->transient_step);
    ui_field(app, font, app_layout_setting(APP_SETTING_STEP), tr(TXT_SETTINGS_STEP), value);
    snprintf(value, sizeof(value), "%.6g s", app->transient_stop);
    ui_field(app, font, app_layout_setting(APP_SETTING_STOP), tr(TXT_SETTINGS_DURATION), value);
    AppRect stop = app_layout_setting(APP_SETTING_STOP);
    color(UI_MUTED);
    ui_wrapped(font, stop.x, stop.y + stop.h + 22, tr(TXT_SETTINGS_LIVE_FRAME), 400, 3, 21);
}

static void draw_languages(const AppState *app, GLuint font)
{
    static const AppSetting settings[] = {APP_SETTING_LANGUAGE_PT_BR, APP_SETTING_LANGUAGE_EN_US};
    static const TextId names[] = {TXT_UI_LANGUAGE_PT, TXT_UI_LANGUAGE_EN};
    for (int language = 0; language < UI_LANG_COUNT; ++language)
        ui_button(app, font, app_layout_setting(settings[language]), tr(names[language]),
                  language == (int)ui_text_language() ? UI_BUTTON_ACTIVE : UI_BUTTON_NEUTRAL);
}

static void draw_settings(const AppState *app, GLuint font)
{
    if (!app->settings)
        return;
    AppRect panel = app_layout_settings_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 42, tr(TXT_SETTINGS_TITLE));
    ui_close_button(app, app_layout_setting(APP_SETTING_CLOSE));
    draw_languages(app, font);
    draw_settings_appearance(app, font);
    draw_settings_snapping(app, font);
    draw_settings_wires(app, font);
    draw_settings_trace(app, font);
    AppRect reset = app_layout_setting(APP_SETTING_RESET);
    ui_note(font, panel.x + 30, ui_baseline(reset), tr(TXT_SETTINGS_AUTOSAVE), 500);
    ui_button(app, font, reset, tr(TXT_SETTINGS_RESTORE_DEFAULTS), UI_BUTTON_NEUTRAL);
}

static TextId value_editor_title(int editing)
{
    if (editing == APP_EDIT_NOTE)
        return TXT_UI_NOTE_TEXT;
    if (editing == APP_EDIT_GROUP_NAME)
        return TXT_UI_EDIT_CHIP_NAME;
    if (editing == APP_EDIT_PORT_NAME)
        return TXT_UI_EDIT_TERMINAL_NAME;
    if (editing == APP_EDIT_SCALE)
        return TXT_UI_EDIT_SIZE;
    if (editing == APP_EDIT_GRID)
        return TXT_UI_EDIT_GRID;
    if (editing == APP_EDIT_STEP)
        return TXT_UI_EDIT_STEP;
    if (editing == APP_EDIT_STOP)
        return TXT_UI_EDIT_DURATION;
    return TXT_UI_EDIT_VALUE;
}

static void draw_value_editor(const AppState *app, GLuint font)
{
    if (app->editing < 0)
        return;
    AppRect panel = app_layout_value_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 24, panel.y + 38, tr(value_editor_title(app->editing)));
    AppRect field = {panel.x + 24, panel.y + 56, panel.w - 48, 40};
    glColor3f(UI_ACCENT);
    ui_outline(field, 6, 2);
    if (app->edit_select_all)
    {
        color(UI_ACCENT_SOFT);
        rounded_rect(field.x + 6, field.y + 6, field.w - 12, field.h - 12, 4);
    }
    char reading[BENCH_GROUP_NAME_CAPACITY + 8];
    snprintf(reading, sizeof(reading), "%s|", app->edit_text);
    color(UI_INK);
    name_text(font, field.x + 12, ui_baseline(field), reading, (int)field.w - 24);
    if (app->edit_error)
    {
        color(UI_DANGER);
        ui_wrapped(font, panel.x + 24, panel.y + 116, app->status, 412, 2, 21);
    }
    else
        ui_note(font, panel.x + 24, panel.y + 132, tr(TXT_UI_EDIT_KEYS), 412);
    ui_button(app, font, app_layout_value_button(1), tr(TXT_UI_CONFIRM), UI_BUTTON_PRIMARY);
    ui_button(app, font, app_layout_value_button(0), tr(TXT_UI_CANCEL), UI_BUTTON_NEUTRAL);
}

static void draw_project_thumbnail(const AppState *app, GLuint font, int index, AppRect card)
{
    static GLuint textures[PROJECT_SLOTS_PER_PAGE];
    static int cached_slots[PROJECT_SLOTS_PER_PAGE] = {-1, -1, -1, -1, -1, -1};
    static time_t cached_times[PROJECT_SLOTS_PER_PAGE];
    static unsigned cached_revisions[PROJECT_SLOTS_PER_PAGE];
    int cell = index % PROJECT_SLOTS_PER_PAGE;
    char path[128];
    snprintf(path, sizeof(path), "%s/slot-%d.bmp", app->selftest ? app_test_directory(APP_TEST_SLOTS) : "saves", index + 1);
    struct _stat metadata;
    int exists = _stat(path, &metadata) == 0;
    if (cached_slots[cell] != index || cached_revisions[cell] != app->project_thumbnail_revision ||
        (exists && cached_times[cell] != metadata.st_mtime) || (!exists && textures[cell]))
    {
        if (textures[cell])
            glDeleteTextures(1, &textures[cell]);
        textures[cell] = 0;
        cached_slots[cell] = index;
        cached_revisions[cell] = app->project_thumbnail_revision;
        cached_times[cell] = exists ? metadata.st_mtime : 0;
        SDL_Surface *loaded = exists ? SDL_LoadBMP(path) : NULL;
        SDL_Surface *rgba = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32) : NULL;
        if (rgba)
        {
            glGenTextures(1, &textures[cell]);
            glBindTexture(GL_TEXTURE_2D, textures[cell]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
        }
        if (rgba)
            SDL_DestroySurface(rgba);
        if (loaded)
            SDL_DestroySurface(loaded);
    }
    AppRect preview = {card.x + 12, card.y + 43, card.w - 24, 112};
    color(.88f, .91f, .93f);
    ui_rounded(preview, UI_RADIUS);
    if (!textures[cell])
    {
        color(UI_MUTED);
        text(font, preview.x + preview.w / 2 - 8, preview.y + preview.h / 2 + 5,
             app->saved_slots[index] ? tr(TXT_UI_SLOT_SAVED) : "+");
        return;
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textures[cell]);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(preview.x, preview.y);
    glTexCoord2f(1, 0); glVertex2f(preview.x + preview.w, preview.y);
    glTexCoord2f(1, 1); glVertex2f(preview.x + preview.w, preview.y + preview.h);
    glTexCoord2f(0, 1); glVertex2f(preview.x, preview.y + preview.h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void draw_project_slot(const AppState *app, GLuint font, int slot)
{
    int index = app->project_page * PROJECT_SLOTS_PER_PAGE + slot;
    if (index >= app->project_slot_count)
        return;
    AppRect r = app_layout_project_slot(slot);
    color(ui_hovered(app, r) ? .90f : .94f, ui_hovered(app, r) ? .93f : .955f, ui_hovered(app, r) ? .97f : .97f);
    ui_rounded(r, UI_RADIUS);
    if (index == app->save_slot)
    {
        glColor3f(UI_ACCENT);
        ui_outline(r, UI_RADIUS, 2);
    }
    draw_project_thumbnail(app, font, index, r);
    char label[32];
    snprintf(label, sizeof(label), tr(TXT_UI_SLOT_NUMBER), index + 1);
    color(UI_INK);
    text(font, r.x + 16, r.y + 32, label);
    if (app->saved_slots[index])
        glColor3f(UI_POSITIVE);
    else
        color(UI_MUTED);
    char summary[80];
    if (app->saved_slots[index] && app->project_parts[index] >= 0)
        snprintf(summary, sizeof(summary), tr(TXT_UI_PROJECT_SUMMARY), app->project_parts[index], app->project_wires[index]);
    else
        snprintf(summary, sizeof(summary), "%s", tr(app->saved_slots[index] ? TXT_UI_SLOT_UNREADABLE : TXT_UI_SLOT_EMPTY));
    name_text(font, r.x + 16, r.y + r.h - 17, summary, (int)r.w - 32);
}

static void draw_projects(const AppState *app, GLuint font)
{
    if (!app->projects_open)
        return;
    AppRect panel = app_layout_projects_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 42, tr(TXT_UI_PROJECTS));
    char page_label[32];
    int pages = (app->project_slot_count + PROJECT_SLOTS_PER_PAGE - 1) / PROJECT_SLOTS_PER_PAGE;
    snprintf(page_label, sizeof(page_label), "%d / %d", app->project_page + 1, pages > 0 ? pages : 1);
    color(UI_MUTED);
    text(font, panel.x + panel.w - 150, panel.y + 42, page_label);
    ui_note(font, panel.x + 30, panel.y + 70, tr(TXT_UI_PROJECTS_NOTE), 820);
    ui_close_button(app, app_layout_project_close());
    for (int slot = 0; slot < APP_PROJECT_SLOTS; ++slot)
        draw_project_slot(app, font, slot);
    char label[48];
    int occupied = app->saved_slots && app->save_slot < app->project_slot_count && app->saved_slots[app->save_slot];
    snprintf(label, sizeof(label), tr(occupied ? TXT_UI_REPLACE_SLOT : TXT_UI_SAVE_TO_SLOT), app->save_slot + 1);
    ui_button(app, font, app_layout_project_save(), label, UI_BUTTON_PRIMARY);
    snprintf(label, sizeof(label), tr(TXT_UI_LOAD_SLOT), app->save_slot + 1);
    ui_button(app, font, app_layout_project_load(), label,
              occupied ? UI_BUTTON_NEUTRAL : UI_BUTTON_DISABLED);
    ui_button(app, font, app_layout_project_previous(), "<", app->project_page > 0 ? UI_BUTTON_NEUTRAL : UI_BUTTON_DISABLED);
    ui_button(app, font, app_layout_project_next(), ">",
              (app->project_page + 1) * PROJECT_SLOTS_PER_PAGE < app->project_slot_count ? UI_BUTTON_NEUTRAL : UI_BUTTON_DISABLED);
}

static void draw_pin_help(const AppState *app, GLuint font)
{
    Pin pin = app->hovered_pin;
    if (app->settings || app->projects_open || app->examples_open || app->editing >= 0 || app->selecting_area ||
        app->hover_revision != app->bench.revision || pin.component < 0 || pin.component >= app->bench.part_count)
        return;
    const Part *part = &app->bench.parts[pin.component];
    const ComponentPinInfo *info = component_pin_info(part->type, pin.pin);
    if (!info)
        return;
    float x, y;
    bench_terminal_position(&app->bench, pin, &x, &y);
    bench_view_to_screen(&app->view, x, y, &x, &y);
    glColor3f(UI_ACCENT);
    circle(x, y, 7);
    ui_raw_white();
    circle(x, y, 3.5f);
    AppRect panel = {238, y > 480 ? 80.0f : 566.0f, 520, 80};
    ui_panel(panel);
    char label[128];
    if (part->group)
        snprintf(label, sizeof(label), tr(TXT_UI_PIN_HELP_IN_CHIP), part->group, part_name(part->type),
                 pin.pin + 1, info->name);
    else
        snprintf(label, sizeof(label), tr(TXT_UI_PIN_HELP), part_name(part->type), pin.pin + 1, info->name);
    color(UI_INK);
    name_text(font, panel.x + 16, panel.y + 28, label, 490);
    pin_role_color(info->role);
    text(font, panel.x + 16, panel.y + 54, component_pin_role_label(info->role));
    ui_note(font, panel.x + 16, panel.y + 74, tr(TXT_UI_PIN_HELP_NOTE), 490);
}

static void draw_project_confirmation(const AppState *app, GLuint font)
{
    if (app->project_overwrite < 0)
        return;
    AppRect panel = app_layout_confirm_panel();
    ui_panel(panel);
    char title[64];
    snprintf(title, sizeof(title), tr(TXT_UI_REPLACE_SLOT), app->project_overwrite + 1);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 40, title);
    ui_wrapped(font, panel.x + 30, panel.y + 74, tr(TXT_UI_REPLACE_NOTE), 580, 2, 22);
    ui_button(app, font, app_layout_confirm_button(0), tr(TXT_UI_REPLACE), UI_BUTTON_DANGER);
    ui_button(app, font, app_layout_confirm_button(2), tr(TXT_UI_CANCEL), UI_BUTTON_NEUTRAL);
}

static void draw_editor_confirmation(const AppState *app, GLuint font)
{
    if (!app->editor_confirm_close)
        return;
    AppRect panel = app_layout_confirm_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 40, tr(TXT_UI_UNSAVED_CHANGES));
    name_text(font, panel.x + 30, panel.y + 68, app->editor_name, 580);
    ui_note(font, panel.x + 30, panel.y + 96, tr(TXT_UI_MAIN_BENCH_KEPT), 580);
    static const TextId labels[APP_CONFIRM_BUTTONS] = {TXT_UI_SAVE_AND_BACK, TXT_UI_DISCARD, TXT_UI_CANCEL};
    static const UiButtonStyle styles[APP_CONFIRM_BUTTONS] = {UI_BUTTON_PRIMARY, UI_BUTTON_DANGER, UI_BUTTON_NEUTRAL};
    for (int i = 0; i < APP_CONFIRM_BUTTONS; ++i)
        ui_button(app, font, app_layout_confirm_button(i), tr(labels[i]), styles[i]);
}

/* Greedy word wrap using measured widths; the last allowed line is truncated with "...". */
static void ui_wrapped(GLuint font, float x, float baseline, const char *source, int width, int max_lines,
                       float line_height)
{
    char line[256], candidate[sizeof(line) * 2];
    const char *cursor = source;
    for (int index = 0; index < max_lines && *cursor; ++index)
    {
        line[0] = 0;
        while (*cursor)
        {
            const char *word_end = strchr(cursor, ' ');
            size_t length = word_end ? (size_t)(word_end - cursor) : strlen(cursor);
            snprintf(candidate, sizeof(candidate), "%s%s%.*s", line, line[0] ? " " : "", (int)length, cursor);
            size_t used = strlen(candidate);
            if (used >= sizeof(line) || (line[0] && ui_text_width(font, candidate) > width && index < max_lines - 1))
                break;
            memcpy(line, candidate, used + 1);
            cursor += length;
            while (*cursor == ' ')
                ++cursor;
        }
        name_text(font, x, baseline + index * line_height, line, width);
    }
}

static void draw_example_card(const AppState *app, GLuint font, int index)
{
    AppRect r = app_layout_example_card(index);
    int current = index == app->example_index;
    if (current)
    {
        glColor3f(UI_ACCENT);
        ui_outline(r, UI_RADIUS, 2);
    }
    else
    {
        float shade = ui_hovered(app, r) ? .90f : .945f;
        color(shade, shade + .02f, shade + .03f);
        ui_rounded(r, UI_RADIUS);
    }
    const char *category = workbench_example_category(index);
    glColor3f(UI_ACCENT);
    ui_text_right_edge(font, r.x + r.w - 14, r.y + 24, category);
    color(UI_INK);
    name_text(font, r.x + 14, r.y + 24, workbench_example_name(index), (int)r.w - 28 - ui_text_width(font, category));
    color(UI_MUTED);
    ui_wrapped(font, r.x + 14, r.y + 44, workbench_example_description(index), (int)r.w - 28, 2,
               (float)ui_font_pixels + 3);
}

/* ---- IC sheet (info window): the package alone and zoomed, every pin explained ---- */
#define DOC_PACKAGE_MAX_SCALE 6.0f
#define DOC_PIN_ROW 19.0f

typedef struct
{
    float cx, cy, scale;
} DocView;

/* Scale that fits the body and every pin (with room for the labels) in the package area. */
static DocView doc_view(PartType type)
{
    AppRect area = app_layout_doc_package();
    Part nominal = {.type = type, .scale = 1};
    float half_w, half_h;
    component_body_extent(type, &half_w, &half_h);
    for (int pin = 0; pin < bench_pin_count(type); ++pin)
    {
        float x, y;
        bench_pin_position(&nominal, pin, &x, &y);
        half_w = fmaxf(half_w, fabsf(x));
        half_h = fmaxf(half_h, fabsf(y));
    }
    const float label_room = 70;
    float scale = fminf(area.w / (2 * half_w + 20), (area.h - label_room) / (2 * half_h));
    return (DocView){app_rect_center_x(area), app_rect_center_y(area), fminf(DOC_PACKAGE_MAX_SCALE, scale)};
}

static void doc_pin_screen(PartType type, const DocView *view, int pin, float *x, float *y)
{
    Part nominal = {.type = type, .scale = 1};
    bench_pin_position(&nominal, pin, x, y);
    *x = view->cx + *x * view->scale;
    *y = view->cy + *y * view->scale;
}

/* Pin under the pointer, on the package or on the list; -1 when none. */
static int doc_hovered_pin(const AppState *app, PartType type, const DocView *view)
{
    if (!app->mouse_position_valid)
        return -1;
    WirePoint mouse = app->mouse_screen;
    AppRect list = app_layout_doc_pin_list();
    if (app_rect_contains(list, mouse.x, mouse.y))
    {
        int row = (int)((mouse.y - list.y) / DOC_PIN_ROW);
        return row < bench_pin_count(type) ? row : -1;
    }
    for (int pin = 0; pin < bench_pin_count(type); ++pin)
    {
        float x, y;
        doc_pin_screen(type, view, pin, &x, &y);
        if (fabsf(mouse.x - x) < view->scale * 5 && fabsf(mouse.y - y) < view->scale * 9)
            return pin;
    }
    return -1;
}

static void doc_draw_package(GLuint font, PartType type, const DocView *view, int hovered)
{
    Part nominal = {.type = type, .scale = 1};
    glPushMatrix();
    glTranslatef(view->cx, view->cy, 0);
    glScalef(view->scale, view->scale, 1);
    if (!component_has_flag(type, COMPONENT_DIP))
        draw_part_leads(&nominal);
    part_draw(&nominal, 0, 0, 0);
    glPopMatrix();
    color(.85f, .88f, .9f);
    const char *symbol = component_symbol(type);
    text(font, view->cx - (float)app_text_utf8_width(symbol, 2048) * .5f, view->cy + 6, symbol);
    for (int pin = 0; pin < bench_pin_count(type); ++pin)
    {
        const ComponentPinInfo *info = component_pin_info(type, pin);
        float x, y;
        doc_pin_screen(type, view, pin, &x, &y);
        int above = y < view->cy;
        if (pin == hovered)
        {
            glColor3f(UI_ACCENT);
            circle(x, y, view->scale * 2.5f);
        }
        char number[16];
        snprintf(number, sizeof(number), "%d", pin + 1);
        color(UI_MUTED);
        text(font, x - (float)app_text_utf8_width(number, 64) * .5f, above ? y - 14 : y + 26, number);
        pin_role_color(info->role);
        glPushMatrix();
        glTranslatef(x + 5, above ? y - 32 : y + 36, 0);
        glRotatef(above ? -55.0f : 55.0f, 0, 0, 1);
        text(font, 0, 0, info->name);
        glPopMatrix();
    }
}

static void doc_draw_pin_list(GLuint font, const ChipDoc *doc, PartType type, int hovered)
{
    AppRect list = app_layout_doc_pin_list();
    for (int pin = 0; pin < bench_pin_count(type); ++pin)
    {
        float y = list.y + pin * DOC_PIN_ROW;
        if (pin == hovered)
        {
            color(UI_ACCENT_SOFT);
            rect(list.x - 6, y, list.w + 6, DOC_PIN_ROW);
        }
        char label[24];
        snprintf(label, sizeof(label), "%d  %s", pin + 1, component_pin_info(type, pin)->name);
        pin_role_color(component_pin_info(type, pin)->role);
        text(font, list.x, y + DOC_PIN_ROW - 5, label);
        color(UI_INK);
        name_text(font, list.x + 92, y + DOC_PIN_ROW - 5, tr(doc->pins[pin]), (int)list.w - 92);
    }
}

static void draw_doc_window(const AppState *app, GLuint font)
{
    const ChipDoc *doc = app->doc_type >= 0 ? chip_doc((PartType)app->doc_type) : NULL;
    if (!doc)
        return;
    PartType type = doc->type;
    AppRect panel = app_layout_doc_panel();
    glow_blend_begin();
    glColor4f(0, 0, 0, .35f);
    rect(-4000, -4000, 12000, 12000);
    glow_blend_end();
    ui_panel(panel);
    char title[96];
    snprintf(title, sizeof(title), tr(TXT_DOC_TITLE), component_symbol(type));
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 42, title);
    ui_close_button(app, app_layout_doc_close());
    ui_wrapped(font, panel.x + 30, panel.y + 72, tr(doc->summary), 520, 3, (float)ui_font_pixels + 3);
    double absolute, recommended, output;
    netlist_logic_ratings(type, &absolute, &recommended, &output);
    char ratings[160];
    if (output > 0)
        snprintf(ratings, sizeof(ratings), tr(TXT_DOC_RATINGS), LOGIC_SUPPLY_MIN, recommended, absolute,
                 output * 1000.0);
    else
        snprintf(ratings, sizeof(ratings), tr(TXT_DOC_RATINGS_SUPPLY), recommended, absolute);
    ui_note(font, panel.x + 30, panel.y + 130, ratings, 520);
    DocView view = doc_view(type);
    int hovered = doc_hovered_pin(app, type, &view);
    doc_draw_package(font, type, &view, hovered);
    doc_draw_pin_list(font, doc, type, hovered);
    AppRect package = app_layout_doc_package();
    color(UI_INK);
    if (hovered >= 0)
    {
        char heading[64];
        snprintf(heading, sizeof(heading), "%d  %s", hovered + 1, component_pin_info(type, hovered)->name);
        text(font, package.x, package.y + package.h + 60, heading);
        ui_wrapped(font, package.x, package.y + package.h + 84, tr(doc->pins[hovered]), (int)package.w, 3,
                   (float)ui_font_pixels + 3);
    }
    else
        ui_note(font, package.x, package.y + package.h + 60, tr(TXT_DOC_PIN_HINT), (int)package.w);
    ui_note(font, panel.x + 30, panel.y + panel.h - 18, tr(TXT_DOC_CLOSE_HINT), 300);
}

static void draw_examples(const AppState *app, GLuint font)
{
    if (!app->examples_open)
        return;
    AppRect panel = app_layout_examples_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 42, tr(TXT_UI_EXAMPLES));
    ui_note(font, panel.x + 30, panel.y + 66,
            tr(TXT_UI_EXAMPLES_NOTE), 800);
    ui_close_button(app, app_layout_examples_close());
    for (int index = 0; index < WORKBENCH_EXAMPLE_COUNT; ++index)
        draw_example_card(app, font, index);
}

/* ---- Microcontroller code editor (side panel) ---- */
#define CODE_SERIAL_LINES 4
#define CODE_LINE_BYTES 512

static void code_text_mono(float x, float baseline, const char *line, int maximum_width)
{
    app_text_utf8_set_monospace(1);
    app_text_utf8(x, baseline, line, maximum_width);
    app_text_utf8_set_monospace(0);
}

/* Offset of a "//" comment outside string literals, or -1. */
static int code_comment_start(const char *line)
{
    int quoted = 0;
    for (int i = 0; line[i]; ++i)
    {
        if (line[i] == '"' && (i == 0 || line[i - 1] != '\\'))
            quoted = !quoted;
        if (!quoted && line[i] == '/' && line[i + 1] == '/')
            return i;
    }
    return -1;
}

static void code_line_draw(const char *line, float x, float baseline, float advance, int maximum_width)
{
    char head[CODE_LINE_BYTES];
    int comment = code_comment_start(line);
    glColor3f(.86f, .89f, .92f);
    if (comment < 0)
    {
        code_text_mono(x, baseline, line, maximum_width);
        return;
    }
    memcpy(head, line, (size_t)comment);
    head[comment] = 0;
    if (comment)
        code_text_mono(x, baseline, head, maximum_width);
    float offset = code_column(line, comment) * advance;
    glColor3f(.45f, .70f, .48f);
    if (offset < maximum_width)
        code_text_mono(x + offset, baseline, line + comment, maximum_width - (int)offset);
}

static void code_editor_header(const AppState *app, GLuint font, AppRect panel)
{
    const McuInstance *instance = &app->mcu->mcu[app->code_part];
    char title[96], state[256];
    snprintf(title, sizeof(title), tr(TXT_MCU_EDITOR_TITLE), part_name(app->bench.parts[app->code_part].type));
    color(UI_INK);
    name_text(font, panel.x + 16, panel.y + 38, title, (int)(panel.w - 230));
    UiButtonStyle run_style = app->code_dirty ? UI_BUTTON_PRIMARY : UI_BUTTON_NEUTRAL;
    ui_button(app, font, app_layout_code_run(), tr(TXT_MCU_RUN), run_style);
    ui_close_button(app, app_layout_code_close());
    if (instance->error[0])
    {
        snprintf(state, sizeof(state), "%s", instance->error);
        color(UI_DANGER);
    }
    else if (app->code_dirty)
    {
        snprintf(state, sizeof(state), "%s", tr(TXT_MCU_CHANGES_NOT_SENT));
        color(UI_MUTED);
    }
    else
    {
        TextId state_text = app->sim.sim_running ? TXT_MCU_STATE_RUNNING : TXT_MCU_STATE_PAUSED;
        snprintf(state, sizeof(state), "%s", tr(state_text));
        color(UI_POSITIVE);
    }
    name_text(font, panel.x + 16, panel.y + 74, state, (int)(panel.w - 32));
}

static void code_line_background(const AppState *app, AppRect area, int line, float top)
{
    const McuInstance *instance = &app->mcu->mcu[app->code_part];
    int error = instance->error[0] && instance->error_line == line + 1;
    if (!error && line != code_line_index(app->code_text, app->code_cursor))
        return;
    if (error)
        glColor3f(.38f, .12f, .12f);
    else
        glColor3f(.16f, .19f, .23f);
    rect(area.x + APP_CODE_GUTTER, top, area.w - APP_CODE_GUTTER, APP_CODE_LINE_HEIGHT);
}

static void code_editor_cursor(const AppState *app, AppRect area, float text_x, float advance)
{
    int line = code_line_index(app->code_text, app->code_cursor);
    if (line < app->code_scroll || line >= app->code_scroll + app_code_visible_lines() || SDL_GetTicks() / 530 % 2)
        return;
    float x = text_x + code_column(app->code_text, app->code_cursor) * advance;
    float top = area.y + 6 + (line - app->code_scroll) * APP_CODE_LINE_HEIGHT;
    glColor3f(.95f, .80f, .30f);
    rect(x, top + 2, 2, APP_CODE_LINE_HEIGHT - 4);
}

static void code_editor_lines(const AppState *app, AppRect area, float advance)
{
    int visible = app_code_visible_lines(), total = code_line_count(app->code_text);
    float text_x = area.x + APP_CODE_GUTTER + 6;
    int text_width = (int)(area.x + area.w - text_x - 4);
    for (int row = 0; row < visible && app->code_scroll + row < total; ++row)
    {
        int line = app->code_scroll + row;
        float top = area.y + 6 + row * APP_CODE_LINE_HEIGHT, baseline = top + APP_CODE_LINE_HEIGHT - 5;
        code_line_background(app, area, line, top);
        char number[16], text_line[CODE_LINE_BYTES];
        snprintf(number, sizeof(number), "%d", line + 1);
        glColor3f(.42f, .47f, .53f);
        code_text_mono(area.x + APP_CODE_GUTTER - 6 - (float)strlen(number) * advance, baseline, number,
                       (int)APP_CODE_GUTTER);
        int start = code_line_offset(app->code_text, line), end = code_line_end(app->code_text, start);
        int length = end - start < CODE_LINE_BYTES - 1 ? end - start : CODE_LINE_BYTES - 1;
        memcpy(text_line, app->code_text + start, (size_t)length);
        text_line[length] = 0;
        if (length)
            code_line_draw(text_line, text_x, baseline, advance, text_width);
    }
    code_editor_cursor(app, area, text_x, advance);
}

/* Shows the newest serial lines, oldest on top. */
static void code_editor_serial(const AppState *app, GLuint font, AppRect area)
{
    const char *serial = app->mcu->mcu[app->code_part].serial;
    color(UI_MUTED);
    name_text(font, area.x, area.y + 14, tr(TXT_MCU_SERIAL_MONITOR), 200);
    AppRect box = {area.x, area.y + 22, area.w, area.h - 22};
    glColor3f(.08f, .09f, .11f);
    ui_rounded(box, 6);
    int starts[CODE_SERIAL_LINES], lengths[CODE_SERIAL_LINES], found = 0;
    int end = (int)strlen(serial);
    if (end > 0 && serial[end - 1] == '\n')
        --end;
    while (found < CODE_SERIAL_LINES && end > 0)
    {
        int start = end;
        while (start > 0 && serial[start - 1] != '\n')
            --start;
        starts[found] = start;
        lengths[found++] = end - start;
        end = start - 1;
    }
    glColor3f(.62f, .90f, .66f);
    for (int i = 0; i < found; ++i)
    {
        char line[CODE_LINE_BYTES];
        int length = lengths[i] < CODE_LINE_BYTES - 1 ? lengths[i] : CODE_LINE_BYTES - 1;
        memcpy(line, serial + starts[i], (size_t)length);
        line[length] = 0;
        if (length)
            code_text_mono(box.x + 8, box.y + 18 + (found - 1 - i) * APP_CODE_LINE_HEIGHT, line, (int)box.w - 16);
    }
    if (!serial[0])
    {
        glColor3f(.42f, .47f, .53f);
        code_text_mono(box.x + 8, box.y + 18, tr(TXT_MCU_SERIAL_EMPTY), (int)box.w - 16);
    }
}

static void code_editor_completions(const AppState *app)
{
    int matches[CODE_COMPLETION_MAX];
    int count = app_code_completions(app, matches, CODE_COMPLETION_MAX);
    if (!count)
        return;
    int selected = app->completion_selected < count ? app->completion_selected : 0;
    AppRect first = app_code_completion_item(app, 0);
    glColor3f(.30f, .34f, .40f);
    ui_rounded((AppRect){first.x - 1, first.y - 1, first.w + 2, first.h * (count + 2) + 2}, 5);
    for (int i = 0; i < count; ++i)
    {
        AppRect row = app_code_completion_item(app, i);
        if (i == selected)
            glColor3f(.16f, .36f, .62f);
        else
            glColor3f(.15f, .17f, .21f);
        rect(row.x, row.y, row.w, row.h);
        glColor3f(.92f, .94f, .97f);
        code_text_mono(row.x + 8, row.y + row.h - 6, mcu_completion(matches[i])->name, (int)row.w - 16);
    }
    const McuCompletion *chosen = mcu_completion(matches[selected]);
    AppRect detail = app_code_completion_item(app, count);
    glColor3f(.11f, .12f, .15f);
    rect(detail.x, detail.y, detail.w, detail.h * 2);
    glColor3f(.95f, .80f, .30f);
    code_text_mono(detail.x + 8, detail.y + 16, tr(chosen->signature), (int)detail.w - 16);
    glColor3f(.70f, .74f, .79f);
    app_text_utf8(detail.x + 8, detail.y + 37, tr(chosen->summary), (int)detail.w - 16);
}

static void draw_code_editor(const AppState *app, GLuint font)
{
    if (app->code_part < 0 || app->code_part >= app->bench.part_count || !app->mcu)
        return;
    AppRect panel = app_layout_code_panel(), area = app_layout_code_text();
    ui_panel(panel);
    code_editor_header(app, font, panel);
    glColor3f(.11f, .13f, .16f);
    ui_rounded(area, 6);
    glColor3f(.09f, .10f, .13f);
    rect(area.x, area.y + 3, APP_CODE_GUTTER, area.h - 6);
    code_editor_lines(app, area, app_code_advance());
    code_editor_serial(app, font, app_layout_code_serial());
    code_editor_completions(app);
}

void app_render_draw(const AppState *app, GLuint font, int width, int height)
{
    app_text_utf8_set_size(15);
    ui_font_pixels = 15;
    dark_interface = app->dark_theme;
    drawing_scale = app->view.zoom;
    lead_corner_radius = app->round_wires ? 18 : 0;
    flow_running = app->sim.sim_running;
    interface_layer = 1;
    draw_chrome(app, font, width, height);
    interface_layer = 0;
    draw_workspace(app, font, width, height);
    if (app->project_thumbnail_render)
        return;
    interface_layer = 1;
    draw_actions(app, font);
    draw_inspector(app, font);
    draw_code_editor(app, font);
    draw_status(app, font);
    draw_pin_help(app, font);
    draw_settings(app, font);
    draw_projects(app, font);
    draw_examples(app, font);
    draw_doc_window(app, font);
    draw_value_editor(app, font);
    draw_editor_confirmation(app, font);
    draw_project_confirmation(app, font);
    AppRect focus = app_controls_focus(app);
    if (focus.w > 0)
        ui_focus_outline(focus);
}
