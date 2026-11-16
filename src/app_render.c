#include "workbench_grid.h"
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
#include <math.h>
#include <stdio.h>
#include <string.h>

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
static const float wire_palette[WIRE_COLOR_COUNT][3] = {{.12f, .36f, .48f}, {.75f, .12f, .12f}, {.12f, .12f, .14f},
                                                        {.12f, .52f, .25f}, {.9f, .7f, .08f},   {.84f, .86f, .87f}};
static const char *wire_color_names[WIRE_COLOR_COUNT] = {"Azul", "Vermelho", "Preto", "Verde", "Amarelo", "Branco"};
static void wire_contact_tip(WirePoint hole, WirePoint next, int wire_color)
{
    float dx = next.x - hole.x, dy = next.y - hole.y, length = hypotf(dx, dy);
    if (length < .01f)
        return;
    dx /= length;
    dy /= length;
    color(wire_palette[wire_color][0], wire_palette[wire_color][1], wire_palette[wire_color][2]);
    glLineWidth(5);
    line(hole.x + dx * 14, hole.y + dy * 14, hole.x + dx * 5, hole.y + dy * 5);
    color(.75f, .52f, .22f);
    glLineWidth(3);
    line(hole.x + dx * 6, hole.y + dy * 6, hole.x, hole.y);
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
        text(font, x, y, "[texto indisponivel]");
}

static void world_text(GLuint font, float x, float y, const char *label, float zoom)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(1 / zoom, 1 / zoom, 1);
    text(font, 0, 0, label);
    glPopMatrix();
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
    color(.62f, .68f, .72f);
    rect(846, 390, 400, 244);
    color(.98f, .98f, .99f);
    rect(840, 384, 400, 244);
    color(.14f, .22f, .28f);
    snprintf(label, sizeof(label),
             current == 2 ? "CORRENTE FIO - A -> B (A)"
             : current    ? "CORRENTE - PINO 1 -> 2 (A)"
                          : "TENSAO - TERMINAL %d (V)",
             pin + 1);
    text(font, 856, 410, label);
    snprintf(label, sizeof(label), current ? "%.4g A" : "%.4g V", high);
    text(font, 856, 433, label);
    snprintf(label, sizeof(label), current ? "%.3g A" : "%.3g V", low);
    text(font, 856, 572, label);
    color(.72f, .77f, .79f);
    line(918, 431, 918, 565);
    line(918, 565, 1220, 565);
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

static void draw_part_board(const Part *p, int selected)
{
    float x = p->x, y = p->y;
    color(.67f, .70f, .72f);
    rect(x - 224, y - 108, 454, 224);
    color(.96f, .96f, .93f);
    rect(x - 227, y - 112, 454, 224);
    color(.80f, .19f, .17f);
    rect(x - 215, y - 109, 430, 2);
    rect(x - 215, y + 68, 430, 2);
    color(.16f, .31f, .47f);
    rect(x - 215, y - 72, 430, 2);
    rect(x - 215, y + 107, 430, 2);
    color(.84f, .85f, .82f);
    rect(x - 218, y - 6, 436, 10);
    if (selected)
    {
        color(.12f, .49f, .84f);
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x - 230, y - 115);
        glVertex2f(x + 230, y - 115);
        glVertex2f(x + 230, y + 115);
        glVertex2f(x - 230, y + 115);
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
static void draw_part_source(const Part *p)
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
    seven_segment_digit(x - 29, y - 17, 13, tenths >= 100 ? tenths / 100 % 10 : -1, p->on);
    seven_segment_digit(x - 20, y - 17, 13, tenths / 10 % 10, p->on);
    glColor3f(p->on ? .40f : .10f, p->on ? .98f : .19f, p->on ? .58f : .14f);
    rect(x - 12.5f, y - 5.5f, 1.6f, 1.6f);
    seven_segment_digit(x - 9, y - 17, 13, tenths % 10, p->on);
    glColor3f(.16f, .17f, .19f);
    circle(x + 24, y - 10, 8);
    glColor3f(.58f, .60f, .62f);
    circle(x + 24, y - 10, 6.5f);
    glColor3f(.20f, .21f, .23f);
    rect(x + 23.3f, y - 16, 1.4f, 6);
    binding_post(x - 21, y + 10, .82f, .12f, .10f);
    binding_post(x - 3, y + 10, .10f, .10f, .11f);
    glColor3f(p->on ? .30f : .30f, p->on ? .92f : .33f, p->on ? .40f : .34f);
    circle(x + 24, y + 10, 2.6f);
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
    int leg;
    color(.72f, .74f, .76f);
    for (leg = 0; leg < 7; ++leg)
    {
        rect(x - 44 + leg * 14, y - 17, 4, 9);
        rect(x - 44 + leg * 14, y + 8, 4, 9);
    }
    color(.14f, .16f, .18f);
    rect(x - 49, y - 13, 98, 28);
    color(.24f, .27f, .30f);
    rect(x - 49, y - 16, 98, 26);
    color(.72f, .76f, .78f);
    circle(x - 40, y - 8, 3);
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
    glRotatef(-p->rotation * 90.0f, 0, 0, 1);
    glTranslatef(-x, -y, 0);
    color(.38f, .08f, .07f);
    ellipse(x, y + 9, 17, 5);
    rect(x - 14, y - 12, 28, 21);
    color(.55f, .09f, .08f);
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
        if (led < 0)
            color(.48f, .48f, .48f);
        else
            color(.48f + .52f * brightness, .08f + .25f * brightness, .08f + .18f * brightness);
        circle(x, y - 14, 13);
        rect(x - 13, y - 14, 26, 23);
        if (led < 0)
            color(.7f, .7f, .7f);
        else
            color(.72f + .28f * brightness, .24f + .50f * brightness, .22f + .40f * brightness);
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

static void voltage_mark(float x, float y, int digit, int decimal)
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
        voltage_mark(x + 12, y, 5, 0);
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
        /* Water-clear dome: the die and its bond wire stay visible, unlike the dark IR photodiode. */
        glColor3f(.66f, .72f, .76f);
        rounded_rect(x - 13, y + 4, 26, 6, 2);
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
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(r, g, b, .35f * mix);
        circle(x, y - 6, 22 + 10 * mix);
        glDisable(GL_BLEND);
    }
    glColor3f(.70f, .72f, .74f);
    rounded_rect(x - 17, y + 10, 34, 6, 2);
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
    for (int segment = 0; segment < COMPONENT_SEGMENTS; ++segment)
    {
        float glow = led_glow(device_element(segment));
        segment_shape(x - 15, y - 30, 58, segment, glow > .02f, glow);
    }
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
    draw_wave_icon(x - 30, y - 11, p->states);
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
    draw_number(x - 25, y - 38, 15, reading, p->states == 2 ? 0 : 2);
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
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1, .86f, .40f, .35f * glow);
        circle(x, y - 30, 22 + 14 * glow);
        glDisable(GL_BLEND);
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

static void part_draw(const Part *p, int selected, int led, float brightness)
{
    float x = p->x, y = p->y;
    if (p->type == PART_BREADBOARD)
    {
        draw_part_board(p, selected);
        return;
    }
    if (is_gate(p->type))
    {
        Part nominal = *p;
        nominal.rotation = 0;
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
    else if (component_environment(p->type))
        draw_sensor(p);
    else if (p->type == PART_SOURCE)
    {
        draw_part_source(p);
    }
    else if (p->type == PART_BUTTON)
    {
        color(.22f, .24f, .26f);
        rect(x - 25, y - 18, 50, 40);
        gradient_rect(x - 25, y - 22, 50, 36, (SurfaceColor){.78f,.81f,.83f}, (SurfaceColor){.49f,.53f,.56f});
        color(.33f,.36f,.39f);
        for (int corner = 0; corner < 4; ++corner)
            circle(x + (corner % 2 ? 20 : -20), y + (corner / 2 ? 10 : -17), 2);
        color(.15f, .17f, .19f);
        circle(x, y - (p->on ? 1 : 6), 14);
        color(.25f, .28f, .31f);
        circle(x - 2, y - (p->on ? 3 : 8), 11);
    }
    else if (p->type == PART_SWITCH)
    {
        draw_part_switch(p);
    }
    else if (p->type == PART_CLOCK)
    {
        draw_part_clock(p);
    }
    else if (component_has_flag(p->type, COMPONENT_QUAD_GATE))
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
    else
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
    glRotatef((float)part->rotation * 90.0f, 0, 0, 1);
    float scale = bench_part_scale(part);
    glScalef(scale, scale, 1);
    glLineWidth(fmaxf(1, drawing_scale * scale));
    part_draw(&local, selected, led, brightness);
    glLineWidth(1);
    glPopMatrix();
}

/* Scorch mark, rising smoke and a label over a burnt part. The smoke is decoration, not a physical quantity. */
static void burnt_draw(GLuint font, const Part *part, Uint64 ticks)
{
    int k;
    float x = part->x, y = part->y;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(.10f, .07f, .05f, .55f);
    circle(x, y, 22);
    for (k = 0; k < 3; ++k)
    {
        float phase = fmodf((float)ticks * .0005f + (float)k / 3.0f, 1.0f);
        glColor4f(.38f, .38f, .40f, .6f * (1.0f - phase));
        circle(x + sinf(phase * 6.0f + (float)k * 2.0f) * 7.0f, y - 20.0f - phase * 36.0f, 5.0f + phase * 8.0f);
    }
    glDisable(GL_BLEND);
    color(.86f, .12f, .08f);
    text(font, x - 36, y - 44, "QUEIMADO");
}

/* ---- Interface widgets: geometry comes from app_layout; colors pass through the theme mapping. ---- */
#define UI_RADIUS 8.0f
#define UI_ACCENT .16f, .45f, .82f
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

static void ui_panel(AppRect r)
{
    color(.55f, .60f, .65f);
    rounded_rect(r.x + 2, r.y + 5, r.w, r.h, UI_RADIUS + 2);
    color(UI_BORDER);
    ui_rounded(r, UI_RADIUS + 2);
    color(UI_SURFACE);
    rounded_rect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, UI_RADIUS + 1);
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
    const char *label = "Executando";
    float fill[3] = {.84f, .94f, .87f};
    if (!running)
    {
        label = "Pausado";
        fill[0] = .89f, fill[1] = .91f, fill[2] = .93f;
    }
    else if (burnt)
    {
        label = "Componente queimado";
        fill[0] = .99f, fill[1] = .87f, fill[2] = .85f;
    }
    else if (app->sim.auto_failed)
    {
        label = "Erro no cálculo";
        fill[0] = .99f, fill[1] = .91f, fill[2] = .80f;
    }
    else if (app->dc_job.thread)
        label = "Calculando...";
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
    float scale = type == PART_BREADBOARD ? .09f : .42f;
    drawing_scale = scale;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1);
    glLineWidth(1);
    part_draw_oriented(&preview, 0, 0, 0);
    if (type == PART_BREADBOARD)
    {
        color(.18f, .22f, .24f);
        glPointSize(1);
        glBegin(GL_POINTS);
        for (int pin = 0; pin < BREADBOARD_PINS; ++pin)
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
    char context[BENCH_GROUP_NAME_CAPACITY + 40];
    snprintf(context, sizeof(context), "Editando: %s%s", app->editor_name, app_editor_dirty(app) ? " *" : "");
    color(UI_INK);
    name_text(font, 24, 42, context, 320);
    static const char *labels[APP_EDITOR_COUNT] = {"Aplicar ao chip", "Renomear", "Salvar modelo", "Voltar"};
    for (int button = 0; button < APP_EDITOR_COUNT; ++button)
        ui_button(app, font, app_layout_editor_header((AppEditorButton)button), labels[button],
                  button == APP_EDITOR_APPLY ? UI_BUTTON_PRIMARY : UI_BUTTON_NEUTRAL);
}

static void draw_header(const AppState *app, GLuint font)
{
    color(UI_SURFACE);
    rect(0, 0, 1280, APP_HEADER_HEIGHT);
    color(UI_BORDER);
    rect(0, APP_HEADER_HEIGHT - 1, 1280, 1);
    if (app->editor_parent)
        draw_editor_header(app, font);
    else
    {
        draw_brand(font);
        ui_button(app, font, app_layout_header(APP_HEADER_PROJECTS), "Projetos",
                  app->projects_open ? UI_BUTTON_ACTIVE : UI_BUTTON_NEUTRAL);
        ui_button(app, font, app_layout_header(APP_HEADER_EXAMPLES), "Exemplos",
                  app->examples_open ? UI_BUTTON_ACTIVE : UI_BUTTON_NEUTRAL);
        ui_button(app, font, app_layout_header(APP_HEADER_SETTINGS), "Configurações",
                  app->settings ? UI_BUTTON_ACTIVE : UI_BUTTON_NEUTRAL);
    }
    play_button_draw(app, font);
}

static void draw_palette_tabs(const AppState *app, GLuint font)
{
    const int per_row = 3;
    color(UI_SURFACE_ALT);
    for (int row = 0; row < APP_PALETTE_TABS / per_row; ++row)
    {
        AppRect first = app_layout_palette_tab(row * per_row), last = app_layout_palette_tab(row * per_row + per_row - 1);
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
    int empty_shown = 0;
    for (int slot = 0; slot < APP_PALETTE_VISIBLE; ++slot)
    {
        if (!app->library_available[slot] && empty_shown++)
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
        snprintf(label, sizeof(label), "Bloco %d", slot + 1);
        if (app->library_available[slot])
            color(UI_INK);
        else
            color(UI_MUTED);
        text(font, r.x + 62, ui_baseline(r), app->library_available[slot] ? label : "Novo: Ctrl+G");
    }
}

static void draw_palette(const AppState *app, GLuint font)
{
    color(UI_SURFACE);
    rect(0, APP_HEADER_HEIGHT, APP_PALETTE_WIDTH, 720 - APP_HEADER_HEIGHT);
    color(UI_BORDER);
    rect(APP_PALETTE_WIDTH - 1, APP_HEADER_HEIGHT, 1, 720 - APP_HEADER_HEIGHT);
    draw_palette_tabs(app, font);
    if (app->palette_page == 2)
    {
        draw_block_library(app, font);
        return;
    }
    int count = palette_count(app->palette_page);
    for (int slot = 0; slot < APP_PALETTE_VISIBLE; ++slot)
    {
        int item = slot + app->palette_offset;
        if (item >= count)
            break;
        PartType type = palette_part(app->palette_page, item);
        AppRect r = app_layout_palette_item(slot);
        draw_palette_card(app, r, app->placing == (int)type);
        draw_thumbnail(type, r.x + 28, app_rect_center_y(r));
        color(UI_INK);
        name_text(font, r.x + 62, ui_baseline(r), part_name(type), 120);
    }
    draw_palette_scrollbar(app->palette_offset, count);
}

static void draw_chrome(const AppState *app, GLuint font, int width, int height)
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1280, 720, 0, -1, 1);
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
        bench_view_to_world(&app->view, 1280, 660, &right, &bottom);
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
}

static void draw_workspace_boards(const AppState *app)
{
    int i;
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && app->bench.parts[i].type == PART_BREADBOARD)
        {
            int p;
            float hole_radius = 3 * fminf(1, bench_part_scale(&app->bench.parts[i]));
            set_device_frame(app, i);
            part_draw_oriented(&app->bench.parts[i], (app->selected == i || app->selection.parts[i]), app->leds[i], measured_led_brightness(app, i));
            for (p = 0; p < bench_pin_count(PART_BREADBOARD); ++p)
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

static void draw_workspace_wires(const AppState *app)
{
    int i;
    color(.25f, .45f, .57f);
    glLineWidth(3);
    for (i = 0; i < app->bench.wire_count; ++i)
    {
        WirePoint start, end, path[WIRE_PATH_CAPACITY];
        int k, count, pass;
        const Wire *w = &app->bench.wires[i];
        if (bench_group_wire_internal(&app->bench, w))
            continue;
        bench_terminal_position(&app->bench, w->a, &start.x, &start.y);
        bench_terminal_position(&app->bench, w->b, &end.x, &end.y);
        count = wire_path(w, start, end, app->round_wires ? 18.0f : 0.0f, path);
        if (i == app->selected_wire)
        {
            color(.15f, .49f, .9f);
            glLineWidth(10);
            glBegin(GL_LINE_STRIP);
            for (k = 0; k < count; ++k)
                glVertex2f(path[k].x, path[k].y);
            glEnd();
        }
        for (pass = 0; pass < 3; ++pass)
        {
            if (pass == 0 && app->dark_theme)
                continue;
            float offset = pass == 0 ? 3.0f : 0.0f;
            if (pass == 0)
            {
                color(.72f, .76f, .78f);
                glLineWidth(6);
            }
            else if (pass == 1)
            {
                color(wire_palette[w->color][0], wire_palette[w->color][1], wire_palette[w->color][2]);
                glLineWidth(5);
            }
            else
            {
                color(wire_palette[w->color][0] * .65f + .25f, wire_palette[w->color][1] * .65f + .25f,
                      wire_palette[w->color][2] * .65f + .25f);
                glLineWidth(2);
            }
            glBegin(GL_LINE_STRIP);
            for (k = 0; k < count; ++k)
                glVertex2f(path[k].x + offset, path[k].y + offset);
            glEnd();
        }
        draw_wire_trace_current(app, i, path, count);
        draw_wire_dc_current(app, i, path, count);
    }
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
                WirePoint p = app->bench.wires[i].knees[k];
                color(.1f, .3f, .45f);
                rect(p.x - 4, p.y - 4, 8, 8);
            }
        }
}

static void draw_workspace_parts(const AppState *app)
{
    int i;
    glLineWidth(1);
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && app->bench.parts[i].type != PART_BREADBOARD)
        {
            int p;
            set_device_frame(app, i);
            part_draw_oriented(&app->bench.parts[i], (app->selected == i || app->selection.parts[i]), app->leds[i], measured_led_brightness(app, i));
            if (app->show_handles && app->selected == i)
                for (int pin = 0; pin < bench_pin_count(app->bench.parts[i].type); ++pin)
                    for (size_t point = 0; point + 1 < bench_lead_count(&app->bench.parts[i], pin); ++point)
                    {
                        WirePoint knee = bench_lead_point(&app->bench.parts[i], pin, point);
                        color(.25f, .65f, .9f);
                        float radius = 4 / app->view.zoom;
                        rect(knee.x - radius, knee.y - radius, 2 * radius, 2 * radius);
                    }
            for (p = 0; p < bench_pin_count(app->bench.parts[i].type); ++p)
            {
                float x, y;
                Pin contact;
                int snapped = bench_find_contact(&app->bench, i, p, &contact);
                float outer_radius = snapped ? 5.0f : 8.0f;
                float inner_radius = snapped ? 3.0f : 5.0f;
                if (!app->bench.parts[i].group && component_has_flag(app->bench.parts[i].type, COMPONENT_QUAD_GATE))
                {
                    outer_radius = 4.0f;
                    inner_radius = 2.5f;
                }
                float terminal_scale = fminf(1, bench_part_scale(&app->bench.parts[i]));
                outer_radius = fmaxf(3, outer_radius * terminal_scale) / app->view.zoom;
                inner_radius = fmaxf(1.5f, inner_radius * terminal_scale) / app->view.zoom;
                bench_pin_position(&app->bench.parts[i], p, &x, &y);
                color(.18f, .22f, .24f);
                circle(x, y, outer_radius);
                if (app->pending.component == i && app->pending.pin == p)
                {
                    color(1, .7f, .2f);
                    circle(x, y, inner_radius);
                }
                else if (snapped)
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
        if (app->bench.parts[wire->a.component].type == PART_BREADBOARD)
            for (k = 1; k < count; ++k)
                if (hypotf(path[k].x - start.x, path[k].y - start.y) > .01f)
                {
                    wire_contact_tip(start, path[k], wire->color);
                    break;
                }
        if (app->bench.parts[wire->b.component].type == PART_BREADBOARD)
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
    static const char symbols[] = {'B', 'I', 'C', 'O', '+', '-', 'G', 'A', 'K', '.', 'C', 'B', 'E'};
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
        if (!hovered->group && hovered->type != PART_BREADBOARD)
            draw_pin_name_labels(app, font, hovered, pin.pin);
        return;
    }
    if (app->selected < 0)
        return;
    const Part *part = &app->bench.parts[app->selected];
    if (part->group || part->type == PART_BREADBOARD)
        return;
    if (!component_has_flag(part->type, COMPONENT_QUAD_GATE))
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
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && component_has_flag(app->bench.parts[i].type, COMPONENT_QUAD_GATE))
        {
            color(.85f, .88f, .9f);
            text(font, app->bench.parts[i].x - 32, app->bench.parts[i].y + 5, component_info(app->bench.parts[i].type)->display_name + 3);
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
                       part_name(app->bench.parts[i].type) + 6, app->view.zoom);
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

static void draw_workspace_burns(const AppState *app, GLuint font)
{
    int i;
    for (i = 0; i < app->bench.part_count; ++i)
        if (app->sim.burnt[i] && !app->bench.parts[i].group)
            burnt_draw(font, &app->bench.parts[i], SDL_GetTicks());
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
        draw_device(&view);
        glPopMatrix();
        right -= display_w * scale + gap;
    }
    device_frame = (DeviceFrame){0};
}

static void draw_groups(const AppState *app, GLuint font)
{
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
    {
        const BenchGroup *group = &app->bench.groups[g];
        if (!group->active)
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
        color(.65f, .69f, .72f);
        rect(group->x - width * .5f + 3, group->y - height * .5f + 4, width, height);
        color(burnt ? .45f : .16f, .2f, .25f);
        rect(group->x - width * .5f, group->y - height * .5f, width, height);
        color(selected ? .22f : .55f, .7f, .9f);
        glLineWidth(selected ? 3 : 1);
        glBegin(GL_LINE_LOOP);
        glVertex2f(group->x - width * .5f - 1, group->y - height * .5f);
        glVertex2f(group->x + width * .5f + 1, group->y - height * .5f);
        glVertex2f(group->x + width * .5f + 1, group->y + height * .5f);
        glVertex2f(group->x - width * .5f - 1, group->y + height * .5f);
        glEnd();
        glLineWidth(1);
        char label[40];
        snprintf(label, sizeof(label), "Bloco %d", g + 1);
        color(.9f, .94f, .98f);
        name_text(font, group->x - width * .5f + 8, group->y + 5, group->name[0] ? group->name : label, (int)width - 16);
        if (lit)
        {
            color(.3f, .95f, .45f);
            circle(group->x + width * .5f - 13, group->y - height * .5f + 8, 3);
        }
        draw_group_displays(app, g + 1, width, height);
        Pin pin;
        for (int index = 0; bench_group_port(&app->bench, g + 1, index, &pin); ++index)
        {
            float x, y;
            bench_terminal_position(&app->bench, pin, &x, &y);
            color(.45f, .52f, .58f);
            line(x, y, x < group->x ? x + 10 : x - 10, y);
            color(.18f, .23f, .28f);
            circle(x, y, 8);
            color(.94f, .96f, .98f);
            circle(x, y, 5);
            const ComponentPinInfo *info = component_pin_info(app->bench.parts[pin.component].type, pin.pin);
            pin_role_color(info->role);
            snprintf(label, sizeof(label), "%d %s", index + 1, component_pin_marker(info->role));
            text(font, x < group->x ? x - 56 : x + 12, y - 8, label);
        }
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
    glScissor((int)(width * 220.0 / 1280), (int)(height * 60.0 / 720), (int)(width * 1060.0 / 1280),
              (int)(height * 590.0 / 720));
    glPushMatrix();
    glTranslatef(220 + app->view.pan_x, 70 + app->view.pan_y, 0);
    glScalef(app->view.zoom, app->view.zoom, 1);
    glTranslatef(-220, -70, 0);
    draw_workspace_grid(app);
    draw_workspace_boards(app);
    draw_workspace_wires(app);
    draw_workspace_route(app);
    draw_workspace_parts(app);
    draw_groups(app, font);
    draw_workspace_contacts(app);
    draw_workspace_labels(app, font);
    draw_workspace_probe(app, font);
    draw_workspace_burns(app, font);
    draw_selection_area(app);
    glPopMatrix();
    glDisable(GL_SCISSOR_TEST);
}

#define INSPECTOR_LINE_1 224.0f
#define INSPECTOR_LINE_2 248.0f
#define INSPECTOR_NOTE_WIDTH 390

static void draw_inspector_row(const AppState *app, GLuint font, AppRect row, const char *label, const char *value,
                               int editable)
{
    if (editable)
        ui_row_hover(app, row);
    color(UI_MUTED);
    text(font, row.x, ui_baseline(row), label);
    AppRect box = {row.x + 110, row.y + 2, row.w - 110, row.h - 4};
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
    ui_panel(app_layout_inspector(150));
    color(UI_INK);
    name_text(font, 852, 112, app->bench.groups[group - 1].name, 396);
    ui_button(app, font, app_layout_group_button(0), "Ver interior", UI_BUTTON_PRIMARY);
    ui_button(app, font, app_layout_group_button(1), "Renomear", UI_BUTTON_NEUTRAL);
    char dimensions[100];
    snprintf(dimensions, sizeof(dimensions), "Largura %.0f  ·  Altura %.0f", bench_group_width(&app->bench, group),
             bench_group_height(&app->bench, group));
    ui_note(font, 852, 196, dimensions, 396);
}

static void inspector_reading(const AppState *app, const Part *part, char *reading, size_t capacity)
{
    const BurnLimits limits = burn_limits_default();
    int index = app->selected;
    if (app->sim.burnt[index])
    {
        if (app->sim.burn_reason[index] == BURN_SHORT)
            snprintf(reading, capacity, "Queimado: curto-circuito nos terminais");
        else if (app->sim.burn_reason[index] == BURN_OVERPOWER)
            snprintf(reading, capacity, "Queimado: %.3g W (limite %.3g W)", app->sim.burn_value[index],
                     limits.resistor_max_power);
        else
            snprintf(reading, capacity, "Queimado: %.4g mA (limite %.3g mA)", app->sim.burn_value[index] * 1000.0,
                     limits.led_max_current * 1000.0);
        return;
    }
    char vector[64];
    if (!sim_state_has_result(&app->sim, SIM_RESULT_DC) ||
        !component_current_vector(part->type, index, vector, sizeof(vector)))
    {
        reading[0] = 0;
        return;
    }
    const char *direction = "terminal 1 → 2";
    if (part->type == PART_NPN || part->type == PART_PNP || part->type == PART_AMBIENT_LIGHT)
        direction = "C → E";
    else if (part->type == PART_TEMPERATURE)
        direction = "OUT → carga";
    snprintf(reading, capacity, "Corrente: %.4g mA (%s)%s", app->sim.dc_led_currents[index] * 1000.0, direction,
             app->sim.display_pending ? " · atualizando" : "");
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
    {
        static const char *names[COMPONENT_SEGMENTS] = {"a", "b", "c", "d", "e", "f", "g", "dp"};
        size_t used = (size_t)snprintf(note, capacity, "Segmentos acesos:");
        for (int segment = 0; segment < COMPONENT_SEGMENTS && used < capacity; ++segment)
            if (e[segment] > 1e-4)
                used += (size_t)snprintf(note + used, capacity - used, " %s", names[segment]);
        break;
    }
    case PART_DIP_SWITCH:
        snprintf(note, capacity, "Contatos 1-4: %s %s %s %s · duplo clique na alavanca", part->states & 1 ? "ON" : "off",
                 part->states & 2 ? "ON" : "off", part->states & 4 ? "ON" : "off", part->states & 8 ? "ON" : "off");
        break;
    case PART_RELAY:
        snprintf(note, capacity, "Bobina %.1f mA · contato NA %s", e[0] * 1e3, e[1] > .5 ? "fechado" : "aberto");
        break;
    case PART_DC_MOTOR:
    case PART_VIBRATION_MOTOR:
    case PART_ENCODER_MOTOR:
        snprintf(note, capacity, "%.0f rpm · %.1f mA · modelo R-L com força contraeletromotriz", e[0] * rpm, e[1] * 1e3);
        break;
    case PART_SERVO:
        snprintf(note, capacity, "Ângulo %.0f° · segue o nível médio do sinal (aprox.)", e[0]);
        break;
    case PART_PIEZO:
        snprintf(note, capacity, "%s · piezo passivo: soa com sinal alternado", fabs(e[0]) > 1e-5 ? "Vibrando" : "Parado");
        break;
    case PART_FUNCTION_GENERATOR:
        snprintf(note, capacity, "%s 0–5 V · duplo clique troca a forma de onda", component_mode_label(part->type, part->states));
        break;
    case PART_MULTIMETER:
        if (part->states == 1)
            snprintf(note, capacity, "Corrente: %.4g mA (entre A e COM)", e[1] * 1e3);
        else if (part->states == 2)
            snprintf(note, capacity, "Resistência: %.4g Ω (circuito sem alimentação)", e[0] / 1e-3);
        else
            snprintf(note, capacity, "Tensão: %.4g V (entre VΩ e COM)", e[0]);
        break;
    case PART_OSCILLOSCOPE:
        snprintf(note, capacity, "CH1: %.3g V · escala ±5 V; histórico das últimas amostras", e[0]);
        break;
    default:
        note[0] = 0;
    }
}

static void inspector_note(const Part *part, char *note, size_t capacity)
{
    note[0] = 0;
    if (part->type == PART_POTENTIOMETER)
        snprintf(note, capacity, "Cursor: %.0f%%", part->control * 100);
    else if (component_electrical_type(part->type) == PART_SOURCE && part->type != PART_SOURCE)
        snprintf(note, capacity, "Resistência interna %.3g Ω · não descarrega por capacidade",
                 component_source_resistance(part->type));
    else if (part->type == PART_DIODE || part->type == PART_NPN || part->type == PART_PNP)
        snprintf(note, capacity, "Modelo SPICE genérico");
}

static void draw_environment_control(GLuint font, const Part *part)
{
    const ComponentEnvironment *environment = component_environment(part->type);
    char reading[128];
    snprintf(reading, sizeof(reading), "%s: %.3g %s", environment->label, component_environment_value(part),
             environment->unit);
    color(UI_INK);
    text(font, 852, INSPECTOR_LINE_2 + 8, reading);
    AppRect track = app_layout_environment_track();
    color(.80f, .84f, .87f);
    ui_rounded(track, track.h / 2);
    glColor3f(UI_ACCENT);
    rounded_rect(track.x, track.y, track.w * (float)part->control, track.h, track.h / 2);
    float knob_x = track.x + track.w * (float)part->control, knob_y = app_rect_center_y(track);
    circle(knob_x, knob_y, 9);
    ui_raw_white();
    circle(knob_x, knob_y, 5);
    ui_note(font, 852, 326, environment->note, INSPECTOR_NOTE_WIDTH);
}

static void draw_part_inspector(const AppState *app, GLuint font)
{
    const Part *part = &app->bench.parts[app->selected];
    const ComponentInfo *info = component_info(part->type);
    char reading[160], note[160], value[64];
    inspector_reading(app, part, reading, sizeof(reading));
    inspector_note(part, note, sizeof(note));
    if (component_device(part->type) && !component_environment(part->type))
    {
        static const double none[COMPONENT_MAX_ELEMENTS];
        int measured = sim_state_has_result(&app->sim, SIM_RESULT_DC);
        device_note(part, measured ? app->sim.dc_elements[app->selected] : none, note, sizeof(note));
    }
    int environment = component_environment(part->type) != NULL;
    float height = environment ? 268 : note[0] ? 186 : 162;
    ui_panel(app_layout_inspector(height));
    color(UI_INK);
    name_text(font, 852, 112, part_name(part->type), 300);
    if (part->type != PART_BREADBOARD)
        ui_button(app, font, app_layout_inspector_rotate(), "Girar", UI_BUTTON_NEUTRAL);
    if (info && info->primary)
        snprintf(value, sizeof(value), "%.5g %s", part->value, info->primary->unit);
    draw_inspector_row(app, font, app_layout_inspector_value(), "Valor",
                       info && info->primary ? value : "Sem valor ajustável", info && info->primary);
    snprintf(value, sizeof(value), "%.5g %%", bench_part_scale(part) * 100);
    draw_inspector_row(app, font, app_layout_inspector_scale(), "Tamanho", value, 1);
    if (app->sim.burnt[app->selected])
        color(UI_DANGER);
    else
        color(UI_INK);
    text(font, 852, INSPECTOR_LINE_1, reading[0] ? reading : "Sem leitura de corrente");
    if (environment)
        draw_environment_control(font, part);
    else if (note[0])
        ui_note(font, 852, INSPECTOR_LINE_2, note, INSPECTOR_NOTE_WIDTH);
}

static void draw_inspector(const AppState *app, GLuint font)
{
    if (app->selected < 0 || app->settings || app->projects_open || app->examples_open)
        return;
    if (app->bench.parts[app->selected].group)
        draw_group_inspector(app, font);
    else
        draw_part_inspector(app, font);
}

static void status_hint(const AppState *app, char *hint, size_t capacity)
{
    if (app->selected >= 0 && app->bench.parts[app->selected].group)
    {
        snprintf(hint, capacity, "Enter: ver interior · F4: renomear · Ctrl+scroll: largura · "
                                 "Ctrl+Shift+scroll: altura · Delete: excluir");
        return;
    }
    if (app->selected >= 0)
    {
        const Part *part = &app->bench.parts[app->selected];
        const ComponentInfo *info = component_info(part->type);
        snprintf(hint, capacity, "%s%sCtrl+scroll: tamanho%s%s · T: gráfico · Delete: excluir",
                 part->type == PART_BREADBOARD ? "" : "R: girar · ", info && info->primary ? "E: valor · " : "",
                 bench_lead_count(part, 0) ? " · Alt+arraste: terminais" : "",
                 part->type == PART_POTENTIOMETER ? " · Shift+scroll: cursor"
                 : component_environment(part->type) ? " · Shift+scroll: ambiente"
                                                      : "");
        return;
    }
    if (app->selected_wire >= 0)
    {
        snprintf(hint, capacity, "Duplo clique: adicionar/remover joelho · T: gráfico da corrente · Delete: excluir");
        return;
    }
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace_automatic && app->show_trace)
    {
        snprintf(hint, capacity, "Gráfico desde o estado inicial · Espaço: reproduzir · B/N: amostra anterior/próxima · "
                                 "P: voltar ao cálculo ao vivo");
        return;
    }
    snprintf(hint, capacity, "Espaço: pausar · Ctrl+Espaço: reiniciar · Shift+arraste: selecionar área · "
                             "Ctrl+C/V: copiar/colar · Ctrl+G: criar bloco · Ctrl+Z/Y: desfazer/refazer");
}

static void draw_status(const AppState *app, GLuint font)
{
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->show_trace && !app->settings)
        trace_draw(font, &app->sim.trace, app->sim.trace_index, app->sim.trace_pin, app->sim.trace_current);
    color(UI_SURFACE);
    rect(APP_PALETTE_WIDTH, 660, 1280 - APP_PALETTE_WIDTH, 60);
    color(UI_BORDER);
    rect(APP_PALETTE_WIDTH, 660, 1280 - APP_PALETTE_WIDTH, 1);
    char status[384], hint[320];
    if (app->sim.live_mode)
        snprintf(status, sizeof(status), "t = %.6g s  ·  %s", app->sim.live_time, app->status);
    else
        snprintf(status, sizeof(status), "%s", app->status);
    color(UI_INK);
    name_text(font, 238, 685, status, 1020);
    status_hint(app, hint, sizeof(hint));
    color(UI_MUTED);
    name_text(font, 238, 709, hint, 1020);
}

static void draw_settings_appearance(const AppState *app, GLuint font)
{
    char value[32];
    ui_section(font, app_layout_settings_section(0, 0), "Aparência");
    ui_toggle(app, font, app_layout_setting(APP_SETTING_DARK_THEME), "Tema escuro", app->dark_theme);
    snprintf(value, sizeof(value), "%d px", app->gui_font_size);
    AppRect minus = app_layout_setting(APP_SETTING_FONT_SMALLER), plus = app_layout_setting(APP_SETTING_FONT_LARGER);
    ui_stepper(app, font, (AppRect){app_layout_setting(APP_SETTING_DARK_THEME).x, minus.y, 400, minus.h}, minus, plus,
               "Tamanho do texto", value);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_SHOW_GRID), "Mostrar grade", app->show_grid);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_ROUND_WIRES), "Cantos arredondados nos fios",
              app->round_wires);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_HANDLES), "Mostrar joelhos dos fios", app->show_handles);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_PIN_LABELS), "Mostrar nomes dos terminais", app->show_pin_labels);
}

static void draw_settings_snapping(const AppState *app, GLuint font)
{
    char value[48];
    ui_section(font, app_layout_settings_section(0, 7), "Grade e encaixe");
    ui_toggle(app, font, app_layout_setting(APP_SETTING_SNAP_GRID), "Encaixar na grade", app->snap_grid);
    snprintf(value, sizeof(value), "%.6g", app->grid_spacing);
    ui_field(app, font, app_layout_setting(APP_SETTING_GRID_SPACING), "Espaçamento da grade", value);
    ui_toggle(app, font, app_layout_setting(APP_SETTING_SNAP_BOARD), "Encaixar na breadboard", app->snap_board);
}

static void draw_settings_wires(const AppState *app, GLuint font)
{
    ui_section(font, app_layout_settings_section(1, 0), "Fios");
    AppRect first = app_layout_wire_swatch(0);
    color(UI_INK);
    text(font, first.x, first.y - 8, app->selected_wire >= 0 ? "Cor do fio selecionado" : "Cor dos próximos fios");
    for (int swatch = 0; swatch < WIRE_COLOR_COUNT; ++swatch)
    {
        AppRect r = app_layout_wire_swatch(swatch);
        if (swatch == app->wire_color)
        {
            glColor3f(UI_ACCENT);
            rounded_rect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, 8);
        }
        color(UI_BORDER);
        ui_rounded(r, 6);
        glColor3f(wire_palette[swatch][0], wire_palette[swatch][1], wire_palette[swatch][2]);
        rounded_rect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 5);
    }
    color(UI_MUTED);
    AppRect last = app_layout_wire_swatch(WIRE_COLOR_COUNT - 1);
    text(font, last.x + last.w + 12, ui_baseline(last), wire_color_names[app->wire_color]);
    AppRect current = app_layout_setting(APP_SETTING_SHOW_CURRENT);
    ui_toggle(app, font, current, "Mostrar corrente nos fios", app->show_current);
    ui_note(font, current.x, current.y + current.h + 22, "Velocidade das bolinhas: 1–100 µA · até 10 mA · ≥ 10 mA", 400);
}

static void draw_settings_trace(const AppState *app, GLuint font)
{
    char value[48];
    ui_section(font, app_layout_settings_section(1, 5), "Gráfico transitório (T)");
    ui_field(app, font, app_layout_setting(APP_SETTING_PROBE_MEASURE), "Grandeza medida",
             app->probe_current ? "Corrente" : "Tensão");
    snprintf(value, sizeof(value), "Terminal %d", app->probe_pin + 1);
    ui_field(app, font, app_layout_setting(APP_SETTING_PROBE_PIN), "Terminal medido", value);
    snprintf(value, sizeof(value), "%.6g s", app->transient_step);
    ui_field(app, font, app_layout_setting(APP_SETTING_STEP), "Passo", value);
    snprintf(value, sizeof(value), "%.6g s", app->transient_stop);
    ui_field(app, font, app_layout_setting(APP_SETTING_STOP), "Duração", value);
    AppRect stop = app_layout_setting(APP_SETTING_STOP);
    ui_note(font, stop.x, stop.y + stop.h + 22, "Ao vivo, cada quadro avança duração ÷ 300.", 400);
}

static void draw_settings(const AppState *app, GLuint font)
{
    if (!app->settings)
        return;
    AppRect panel = app_layout_settings_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 42, "Configurações");
    ui_close_button(app, app_layout_setting(APP_SETTING_CLOSE));
    draw_settings_appearance(app, font);
    draw_settings_snapping(app, font);
    draw_settings_wires(app, font);
    draw_settings_trace(app, font);
    AppRect reset = app_layout_setting(APP_SETTING_RESET);
    ui_note(font, panel.x + 30, ui_baseline(reset), "Alterações são salvas automaticamente.", 500);
    ui_button(app, font, reset, "Restaurar padrões", UI_BUTTON_NEUTRAL);
}

static const char *value_editor_title(int editing)
{
    if (editing == APP_EDIT_GROUP_NAME)
        return "Nome do chip";
    if (editing == APP_EDIT_SCALE)
        return "Tamanho em porcentagem (1 a 10000)";
    if (editing == APP_EDIT_GRID)
        return "Espaçamento da grade (1 a 10000)";
    if (editing == APP_EDIT_STEP)
        return "Passo do gráfico, em segundos";
    if (editing == APP_EDIT_STOP)
        return "Duração do gráfico, em segundos";
    return "Valor (aceita 4k7, 10u, 2.2e-3...)";
}

static void draw_value_editor(const AppState *app, GLuint font)
{
    if (app->editing < 0)
        return;
    AppRect panel = {420, 250, 440, 160};
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 24, panel.y + 38, value_editor_title(app->editing));
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
    ui_note(font, panel.x + 24, panel.y + 132, "Enter confirma  ·  Esc cancela", 380);
}

static void draw_project_slot(const AppState *app, GLuint font, int slot)
{
    AppRect r = app_layout_project_slot(slot);
    if (slot == app->save_slot)
    {
        glColor3f(UI_ACCENT);
        ui_outline(r, UI_RADIUS, 2);
    }
    else
    {
        color(ui_hovered(app, r) ? .90f : .94f, ui_hovered(app, r) ? .93f : .955f, ui_hovered(app, r) ? .97f : .97f);
        ui_rounded(r, UI_RADIUS);
    }
    char label[32];
    snprintf(label, sizeof(label), "Slot %d", slot + 1);
    color(UI_INK);
    text(font, r.x + 16, r.y + 32, label);
    if (app->saved_slots[slot])
        glColor3f(UI_POSITIVE);
    else
        color(UI_MUTED);
    text(font, r.x + 16, r.y + 58, app->saved_slots[slot] ? "Salvo" : "Vazio");
}

static void draw_projects(const AppState *app, GLuint font)
{
    if (!app->projects_open)
        return;
    AppRect panel = app_layout_projects_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 42, "Projetos");
    ui_note(font, panel.x + 30, panel.y + 70, "Cada slot guarda a montagem e as configurações do projeto.", 560);
    ui_close_button(app, app_layout_project_close());
    for (int slot = 0; slot < APP_PROJECT_SLOTS; ++slot)
        draw_project_slot(app, font, slot);
    char label[48];
    snprintf(label, sizeof(label), "Salvar no slot %d", app->save_slot + 1);
    ui_button(app, font, app_layout_project_save(), label, UI_BUTTON_PRIMARY);
    snprintf(label, sizeof(label), "Carregar slot %d", app->save_slot + 1);
    ui_button(app, font, app_layout_project_load(), label,
              app->saved_slots[app->save_slot] ? UI_BUTTON_NEUTRAL : UI_BUTTON_DISABLED);
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
    circle(x, y, fmaxf(9 * app->view.zoom, 6));
    ui_raw_white();
    circle(x, y, fmaxf(5 * app->view.zoom, 3));
    AppRect panel = {238, y > 480 ? 80.0f : 566.0f, 520, 80};
    ui_panel(panel);
    char label[128];
    if (part->group)
        snprintf(label, sizeof(label), "Bloco %d › %s · terminal %d (%s)", part->group, part_name(part->type),
                 pin.pin + 1, info->name);
    else
        snprintf(label, sizeof(label), "%s · terminal %d (%s)", part_name(part->type), pin.pin + 1, info->name);
    color(UI_INK);
    name_text(font, panel.x + 16, panel.y + 28, label, 490);
    pin_role_color(info->role);
    text(font, panel.x + 16, panel.y + 54, component_pin_role_label(info->role));
    ui_note(font, panel.x + 16, panel.y + 74, "Função do terminal; não é uma medição de tensão.", 490);
}

static void draw_editor_confirmation(const AppState *app, GLuint font)
{
    if (!app->editor_confirm_close)
        return;
    AppRect panel = app_layout_confirm_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 40, "Alterações não salvas no editor");
    name_text(font, panel.x + 30, panel.y + 68, app->editor_name, 580);
    ui_note(font, panel.x + 30, panel.y + 96, "A bancada externa continua preservada.", 580);
    static const char *labels[APP_CONFIRM_BUTTONS] = {"Salvar e voltar", "Descartar", "Cancelar"};
    static const UiButtonStyle styles[APP_CONFIRM_BUTTONS] = {UI_BUTTON_PRIMARY, UI_BUTTON_DANGER, UI_BUTTON_NEUTRAL};
    for (int i = 0; i < APP_CONFIRM_BUTTONS; ++i)
        ui_button(app, font, app_layout_confirm_button(i), labels[i], styles[i]);
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

static void draw_examples(const AppState *app, GLuint font)
{
    if (!app->examples_open)
        return;
    AppRect panel = app_layout_examples_panel();
    ui_panel(panel);
    color(UI_INK);
    text(font, panel.x + 30, panel.y + 42, "Exemplos");
    ui_note(font, panel.x + 30, panel.y + 66,
            "Escolha um circuito pronto. Ele substitui a bancada atual; Ctrl+Z desfaz.", 800);
    ui_close_button(app, app_layout_examples_close());
    for (int index = 0; index < WORKBENCH_EXAMPLE_COUNT; ++index)
        draw_example_card(app, font, index);
}

void app_render_draw(const AppState *app, GLuint font, int width, int height)
{
    app_text_utf8_set_size(app->gui_font_size);
    ui_font_pixels = app->gui_font_size;
    dark_interface = app->dark_theme;
    drawing_scale = app->view.zoom;
    lead_corner_radius = app->round_wires ? 18 : 0;
    flow_running = app->sim.sim_running;
    interface_layer = 1;
    draw_chrome(app, font, width, height);
    interface_layer = 0;
    draw_workspace(app, font, width, height);
    interface_layer = 1;
    draw_inspector(app, font);
    draw_status(app, font);
    draw_pin_help(app, font);
    draw_settings(app, font);
    draw_projects(app, font);
    draw_examples(app, font);
    draw_value_editor(app, font);
    draw_editor_confirmation(app, font);
}
