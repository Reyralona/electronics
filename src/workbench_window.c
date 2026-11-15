#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "workbench.h"
#include "workbench_file.h"
#include "workbench_examples.h"
#include "spice_process.h"
#include "spice_job.h"
#include "spice_trace.h"
#include "workbench_history.h"
#include "workbench_view.h"
#include "sim_auto.h"
#include "dev_capture.h"
#include "app_state.h"
#include "app_simulation.h"
#include "dev_scenarios.h"

#define PLAY_BUTTON_X 1040.0f
#define ROTATE_BUTTON_X 1180.0f

static void rect(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}
static void color(float r, float g, float b)
{
    glColor3f(r, g, b);
}
static void line(float x, float y, float x2, float y2)
{
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x2, y2);
    glEnd();
}
static void circle(float x, float y, float radius);
static const float wire_palette[WIRE_COLOR_COUNT][3] = {{.12f, .36f, .48f}, {.75f, .12f, .12f}, {.12f, .12f, .14f},
                                                        {.12f, .52f, .25f}, {.9f, .7f, .08f},   {.84f, .86f, .87f}};
static const char *wire_color_names[WIRE_COLOR_COUNT] = {"Azul", "Vermelho", "Preto", "Verde", "Amarelo", "Branco"};
static int is_gate(PartType type)
{
    return (type >= PART_AND && type <= PART_NOT) || type == PART_DFF;
}
static PartType palette_part(int page, int item)
{
    static const PartType digital[] = {PART_AND, PART_NAND,  PART_OR,  PART_XOR,
                                       PART_NOT, PART_CLOCK, PART_DFF, PART_74HC00};
    return page ? digital[item] : (PartType)item;
}
static const char *part_name(PartType type)
{
    static const char *names[] = {"Fonte",     "Interruptor", "LED",       "Breadboard", "Resistor",
                                  "Capacitor", "Indutor",     "Porta AND", "Porta NAND", "Porta OR",
                                  "Porta XOR", "Porta NOT",   "Clock",     "Bloco DFF",  "CI 74HC00"};
    return names[type];
}
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
static void wire_current_style(int level)
{
    if (level == 1)
    {
        color(.95f, .58f, .12f);
        glLineWidth(3);
    }
    else if (level == 2)
    {
        color(1.0f, .36f, .04f);
        glLineWidth(5);
    }
    else
    {
        color(.95f, .16f, .03f);
        glLineWidth(7);
    }
}
static void wire_direction_marker(const WirePoint *path, int count, int reverse)
{
    float total = 0, position;
    int i;
    for (i = 1; i < count; ++i)
        total += hypotf(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
    position = total * .5f;
    for (i = 1; i < count; ++i)
    {
        float dx = path[i].x - path[i - 1].x, dy = path[i].y - path[i - 1].y, len = hypotf(dx, dy);
        if (len > 0 && position <= len)
        {
            float x = path[i - 1].x + dx * position / len, y = path[i - 1].y + dy * position / len, ux = dx / len,
                  uy = dy / len;
            if (reverse)
            {
                ux = -ux;
                uy = -uy;
            }
            color(1, .91f, .35f);
            glBegin(GL_TRIANGLES);
            glVertex2f(x + ux * 7, y + uy * 7);
            glVertex2f(x - ux * 5 - uy * 4, y - uy * 5 + ux * 4);
            glVertex2f(x - ux * 5 + uy * 4, y - uy * 5 - ux * 4);
            glEnd();
            return;
        }
        position -= len;
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
static GLuint make_font(void)
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
    if (!font)
        return;
    glRasterPos2f(x, y);
    glListBase(font);
    glCallLists((GLsizei)strlen(str), GL_UNSIGNED_BYTE, str);
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

static void pin_xy(const Part *p, int pin, float *x, float *y)
{
    bench_pin_position(p, pin, x, y);
}
static int hit_wire(const Workbench *bench, float x, float y, float tolerance, int rounded)
{
    int i;
    for (i = bench->wire_count - 1; i >= 0; --i)
    {
        const Wire *w = &bench->wires[i];
        WirePoint a, b, path[WIRE_PATH_CAPACITY];
        int k, n;
        pin_xy(&bench->parts[w->a.component], w->a.pin, &a.x, &a.y);
        pin_xy(&bench->parts[w->b.component], w->b.pin, &b.x, &b.y);
        n = wire_path(w, a, b, rounded ? 18.0f : 0.0f, path);
        for (k = 1; k < n; ++k)
        {
            float dx = path[k].x - path[k - 1].x, dy = path[k].y - path[k - 1].y, den = dx * dx + dy * dy, t = 0, px,
                  py;
            if (den > 0)
                t = fminf(1, fmaxf(0, ((x - path[k - 1].x) * dx + (y - path[k - 1].y) * dy) / den));
            px = path[k - 1].x + t * dx;
            py = path[k - 1].y + t * dy;
            if ((x - px) * (x - px) + (y - py) * (y - py) <= tolerance * tolerance)
                return i;
        }
    }
    return -1;
}
static void part_draw(const Part *p, int selected, int led)
{
    float x = p->x, y = p->y;
    if (p->type == PART_BREADBOARD)
    {
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
        return;
    }
    color(.68f, .71f, .73f);
    rect(x - 51, y - 3, 102, 6);
    color(.72f, .74f, .76f);
    if (p->type == PART_SOURCE || p->type == PART_SWITCH || p->type == PART_CLOCK)
        rect(x - 39, y - 23, 82, 50);
    else
        rect(x - 19, y - 11, 40, 24);
    if (p->type == PART_SOURCE)
    {
        color(.22f, .26f, .31f);
        rect(x - 40, y - 27, 80, 50);
        color(.18f, .53f, .73f);
        rect(x - 37, y - 25, 74, 45);
        color(.9f, .95f, .97f);
        line(x - 22, y, x - 8, y);
        line(x - 15, y - 7, x - 15, y + 7);
        line(x + 13, y, x + 27, y);
        color(p->on ? .25f : .55f, p->on ? .9f : .55f, .34f);
        circle(x + 25, y - 16, 4);
    }
    else if (p->type == PART_SWITCH)
    {
        color(.23f, .25f, .27f);
        rect(x - 36, y - 22, 72, 42);
        color(.77f, .79f, .79f);
        rect(x - 31, y - 17, 62, 30);
        color(.2f, .22f, .24f);
        circle(x - 17, y, 5);
        circle(x + 17, y, 5);
        glLineWidth(5);
        line(x - 17, y, x + 17, p->on ? y : y - 13);
        glLineWidth(1);
    }
    else if (p->type == PART_CLOCK)
    {
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
    else if (p->type == PART_74HC00)
    {
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
    else if (is_gate(p->type))
    {
        if (p->type != PART_NOT)
        {
            color(.68f, .71f, .73f);
            rect(x - 51, y - 17, 26, 4);
            rect(x - 51, y + 13, 26, 4);
        }
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
        else if (p->type == PART_OR || p->type == PART_XOR)
        {
            line(x - 19, y - 13, x - 13, y);
            line(x - 13, y, x - 19, y + 13);
            line(x - 19, y - 13, x + 2, y - 11);
            line(x + 2, y - 11, x + 20, y);
            line(x + 20, y, x + 2, y + 11);
            line(x + 2, y + 11, x - 19, y + 13);
            if (p->type == PART_XOR)
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
        if (p->type == PART_NAND || p->type == PART_NOT)
        {
            color(.84f, .89f, .92f);
            circle(x + 23, y, 3);
        }
    }
    else if (p->type == PART_RESISTOR)
    {
        color(.73f, .60f, .40f);
        rect(x - 18, y - 8, 36, 16);
        color(.89f, .78f, .56f);
        rect(x - 17, y - 10, 34, 15);
        color(.4f, .21f, .12f);
        rect(x - 11, y - 10, 3, 15);
        color(.12f, .12f, .12f);
        rect(x - 4, y - 10, 3, 15);
        color(.8f, .25f, .13f);
        rect(x + 3, y - 10, 3, 15);
        color(.7f, .58f, .2f);
        rect(x + 11, y - 10, 3, 15);
    }
    else if (p->type == PART_CAPACITOR)
    {
        color(.16f, .19f, .25f);
        rect(x - 18, y - 23, 36, 46);
        color(.25f, .31f, .42f);
        rect(x - 18, y - 26, 36, 44);
        color(.55f, .62f, .7f);
        rect(x + 9, y - 22, 4, 34);
    }
    else if (p->type == PART_INDUCTOR)
    {
        int coil;
        color(.25f, .2f, .16f);
        rect(x - 31, y - 13, 62, 26);
        color(.66f, .36f, .18f);
        rect(x - 29, y - 15, 58, 23);
        color(.94f, .65f, .35f);
        for (coil = 0; coil < 6; ++coil)
            rect(x - 26 + coil * 9, y - 14, 3, 21);
    }
    else
    {
        color(.75f, .75f, .72f);
        rect(x - 16, y + 5, 32, 7);
        /* led: -1 unknown, 0 off, 1 lit, 2 burnt */
        if (led == 2)
        {
            color(.22f, .14f, .10f);
            circle(x, y - 3, 13);
            color(.40f, .30f, .24f);
            circle(x - 4, y - 7, 3);
        }
        else
        {
            if (led < 0)
                color(.48f, .48f, .48f);
            else
                color(led ? 1.0f : .6f, led ? .28f : .12f, led ? .24f : .14f);
            circle(x, y - 3, 13);
            if (led < 0)
                color(.7f, .7f, .7f);
            else
                color(led ? 1.0f : .8f, led ? .63f : .28f, led ? .55f : .27f);
            circle(x - 4, y - 7, 4);
        }
    }
    if (selected)
    {
        color(.12f, .49f, .84f);
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x - 46, y - 32);
        glVertex2f(x + 46, y - 32);
        glVertex2f(x + 46, y + 32);
        glVertex2f(x - 46, y + 32);
        glEnd();
        glLineWidth(1);
    }
}

static void part_draw_oriented(const Part *part, int selected, int led)
{
    Part local = *part;
    local.x = 0;
    local.y = 0;
    glPushMatrix();
    glTranslatef(part->x, part->y, 0);
    glRotatef((float)part->rotation * 90.0f, 0, 0, 1);
    part_draw(&local, selected, led);
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

static void play_button_draw(GLuint font, int running, int busy, int failed, int burnt_count)
{
    const char *label;
    float x = PLAY_BUTTON_X, cx = x + 22, cy = 34;
    if (!running)
    {
        label = "PAUSADO";
        color(.86f, .88f, .90f);
    }
    else if (burnt_count)
    {
        label = "QUEIMOU ALGO";
        color(.98f, .86f, .84f);
    }
    else if (failed)
    {
        label = "ERRO NO CALCULO";
        color(.98f, .90f, .80f);
    }
    else if (busy)
    {
        label = "CALCULANDO...";
        color(.84f, .92f, .97f);
    }
    else
    {
        label = "EXECUTANDO";
        color(.83f, .94f, .86f);
    }
    rect(x, 12, 222, 44);
    if (running)
        color(.13f, .48f, .24f);
    else
        color(.25f, .33f, .40f);
    circle(cx, cy, 15);
    color(.98f, .99f, .99f);
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
    if (burnt_count)
        color(.72f, .12f, .08f);
    else
        color(.13f, .22f, .28f);
    text(font, x + 46, 40, label);
}

/* Topmost part under a world point; breadboards are only hit when nothing else is. */
static int part_at(const Workbench *bench, float x, float y)
{
    int pass, i;
    for (pass = 0; pass < 2; ++pass)
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            const Part *part = &bench->parts[i];
            int board = part->type == PART_BREADBOARD, vertical = part->rotation % 2, chip = part->type == PART_74HC00;
            float hw, hh;
            if (board != pass)
                continue;
            if (board)
            {
                hw = 227.0f;
                hh = 112.0f;
            }
            else if (chip)
            {
                hw = vertical ? 20.0f : 50.0f;
                hh = vertical ? 50.0f : 20.0f;
            }
            else
            {
                hw = vertical ? 30.0f : 44.0f;
                hh = vertical ? 44.0f : 30.0f;
            }
            if (fabsf(x - part->x) < hw && fabsf(y - part->y) < hh)
                return i;
        }
    return -1;
}

/* The automatic transient records the selected terminal, or the first source terminal, for the optional graph. */
/* ngspice lives next to the executable (build/spice-local); fall back to the path relative to the project root. */
static void find_ngspice(char *path, size_t capacity)
{
    const char *base = SDL_GetBasePath();
    snprintf(path, capacity, "build/spice-local/ucrt64/bin/ngspice_con.exe");
    if (base)
    {
        char candidate[1024];
        snprintf(candidate, sizeof(candidate), "%sspice-local/ucrt64/bin/ngspice_con.exe", base);
        if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
            snprintf(path, capacity, "%s", candidate);
    }
}

int main(int argc, char **argv)
{
    SDL_Window *window;
    SDL_GLContext context;
    GLuint font;
    static AppState app;
    app_state_init(&app);
    app.selftest = argc > 1 && strcmp(argv[1], "--self-test-history") == 0;
    app.capture = argc > 1 && dev_capture_requested(argv[1]);
    app.auto_capture =
        app.capture && (strcmp(argv[1], "--capture-burn") == 0 || strncmp(argv[1], "--capture-auto", 14) == 0);
    app.sim.auto_enabled = (!app.selftest && !app.capture) || app.auto_capture;
    find_ngspice(app.ngspice, sizeof(app.ngspice));

    if (dev_scenario_prepare(&app, argc, argv))
        return 1;
    if (!app.history)
    {
        fprintf(stderr, "Memoria insuficiente para historico.\n");
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        free(app.history);
        return 1;
    }
    window = SDL_CreateWindow("Bancada experimental", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        fprintf(stderr, "Janela: %s\n", SDL_GetError());
        SDL_Quit();
        free(app.history);
        return 1;
    }
    context = SDL_GL_CreateContext(window);
    if (!context)
    {
        fprintf(stderr, "OpenGL: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(app.history);
        return 1;
    }
    font = make_font();
    SDL_GL_SetSwapInterval(1);
    if (dev_scenario_analyze(&app, argv))
        return 1;
    while (app.running)
    {
        SDL_Event e;
        int width, height, i;
        if (app.selftest)
            dev_scenario_queue(&app, window);
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
                app.running = 0;
            else if (app.editing >= 0 && e.type == SDL_EVENT_TEXT_INPUT)
            {
                const char *p = e.text.text;
                size_t length = strlen(app.edit_text);
                while (*p && length + 1 < sizeof(app.edit_text))
                {
                    if ((unsigned char)*p >= 32 && (unsigned char)*p < 127)
                        app.edit_text[length++] = *p;
                    ++p;
                }
                app.edit_text[length] = '\0';
            }
            else if (app.editing >= 0 && e.type == SDL_EVENT_KEY_DOWN)
            {
                if (e.key.key == SDLK_ESCAPE)
                {
                    app.editing = -1;
                    SDL_StopTextInput(window);
                    app.status = "Edicao cancelada.";
                }
                else if (e.key.key == SDLK_BACKSPACE)
                {
                    size_t length = strlen(app.edit_text);
                    if (length)
                        app.edit_text[length - 1] = '\0';
                }
                else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER)
                {
                    double value;
                    Workbench before = app.bench;
                    if (app.editing >= WORKBENCH_MAX_COMPONENTS)
                    {
                        double step = app.transient_step, stop = app.transient_stop;
                        int parsed = bench_parse_value(app.edit_text, &value);
                        if (parsed && app.editing == WORKBENCH_MAX_COMPONENTS)
                            step = value;
                        else if (parsed)
                            stop = value;
                        if (parsed && step <= stop && stop / step <= 500.0 * (1.0 + 1e-12))
                        {
                            app.transient_step = step;
                            app.transient_stop = stop;
                            app.editing = -1;
                            SDL_StopTextInput(window);
                            app.status = "Parametro transitorio atualizado.";
                        }
                        else
                            app.status = "Use tempos positivos: passo <= duracao e duracao/passo <= 500.";
                    }
                    else if (bench_parse_value(app.edit_text, &value) &&
                             bench_set_value(&app.bench, app.editing, value))
                    {
                        if (before.parts[app.editing].value != value)
                            bench_history_record(app.history, &before);
                        app.editing = -1;
                        SDL_StopTextInput(window);
                        app.status = "Valor alterado.";
                    }
                    else
                        app.status = "Valor invalido. Use um numero positivo entre 1e-12 e 1e12.";
                }
            }
            else if (app.editing >= 0 && (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_MOTION))
            {
                continue;
            }
            else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
            {
                if (e.key.key == SDLK_F2)
                {
                    Workbench example = {0};
                    int choice = (app.example_index + 1) % WORKBENCH_EXAMPLE_COUNT, example_selected;
                    if (workbench_example_create(choice, &example, &example_selected))
                    {
                        bench_history_record(app.history, &app.bench);
                        app.bench = example;
                        app.example_index = choice;
                        app.selected = example_selected;
                        app.selected_wire = -1;
                        app.dragging = -1;
                        app.drag_wire = -1;
                        app.pending.component = -1;
                        app.route_count = 0;
                        app.placing = -1;
                        app.settings = 0;
                        app.view = (BenchView){1, 0, 0};
                        app.sim.trace_valid = 0;
                        app.sim.dc_valid = 0;
                        app.sim.dc_wire_valid = 0;
                        memset(app.leds, 0, sizeof(app.leds));
                        app.probe_pin = choice > 0 ? 2 : 0;
                        app.probe_current = 0;
                        app.transient_step = 1e-5;
                        app.transient_stop = .005;
                        app.status = workbench_example_name(choice);
                    }
                    else
                        app.status = "Falha ao preparar exemplo.";
                    continue;
                }
                if (e.key.key == SDLK_TAB)
                {
                    app.palette_page = 1 - app.palette_page;
                    app.placing = -1;
                    app.status = app.palette_page ? "Biblioteca de portas logicas." : "Biblioteca de componentes.";
                    continue;
                }
                if (e.key.key == SDLK_F1)
                {
                    app.settings = !app.settings;
                    app.dragging = -1;
                    app.drag_wire = -1;
                    continue;
                }
                if (e.key.key == SDLK_HOME)
                {
                    app.view = (BenchView){1, 0, 0};
                    app.status = "Vista restaurada.";
                    continue;
                }
                if ((e.key.mod & SDL_KMOD_CTRL) && (e.key.key == SDLK_Z || e.key.key == SDLK_Y))
                {
                    int changed = e.key.key == SDLK_Z ? bench_history_undo(app.history, &app.bench)
                                                      : bench_history_redo(app.history, &app.bench);
                    if (changed)
                    {
                        app.selected = -1;
                        app.selected_wire = -1;
                        app.dragging = -1;
                        app.drag_wire = -1;
                        app.pending.component = -1;
                        app.route_count = 0;
                        memset(app.leds, 0, sizeof(app.leds));
                        app.status = e.key.key == SDLK_Z ? "Acao desfeita." : "Acao refeita.";
                    }
                    else
                        app.status = "Nenhuma acao disponivel.";
                    continue;
                }
                if (e.key.key == SDLK_ESCAPE)
                {
                    app.pending.component = -1;
                    app.route_count = 0;
                    app.placing = -1;
                    app.dragging = -1;
                    app.drag_wire = -1;
                    app.status = "Selecao cancelada.";
                }
                else if (e.key.key == SDLK_SPACE)
                {
                    app.sim.sim_running = !app.sim.sim_running;
                    if (app.sim.trace_valid && app.sim.sim_running)
                    {
                        if (app.sim.trace_index >= app.sim.trace.count - 1)
                            app.sim.trace_index = 0;
                        app.sim.trace_play_start_index = app.sim.trace_index;
                        app.sim.trace_play_started = SDL_GetTicks();
                        app.status = "Reproducao da curva: intervalo completo em 5 s reais.";
                        if (app.sim.trace_automatic)
                            app.status = "Simulacao em execucao: o circuito e calculado automaticamente.";
                    }
                    else if (app.sim.trace_valid)
                        app.status = "Reproducao pausada; B/N percorrem as amostras.";
                    else if (app.sim.auto_enabled)
                        app.status = app.sim.sim_running
                                         ? "Simulacao em execucao: o circuito e calculado automaticamente."
                                         : "Simulacao pausada. Espaco ou o botao de play continua.";
                    else
                        app.status = app.sim.sim_running ? "Simulacao executando." : "Simulacao pausada.";
                }
                else if (e.key.key == SDLK_P && app.sim.auto_enabled)
                {
                    app.sim.auto_requested_valid = 0;
                    app.sim.bench_changed_at = 0;
                    app.sim.sim_running = 1;
                    app.status = "Recalculando o circuito...";
                }
                else if (e.key.key == SDLK_R && app.selected >= 0)
                {
                    Workbench before = app.bench;
                    if (bench_rotate_part(&app.bench, app.selected))
                    {
                        bench_snap_to_breadboard(&app.bench, app.selected);
                        bench_history_record(app.history, &before);
                        app.dragging = -1;
                        app.pending.component = -1;
                        app.route_count = 0;
                        app.status = "Componente girado 90 graus.";
                    }
                    else
                        app.status = "Esta placa ainda nao gira.";
                }
                else if (e.key.key == SDLK_G && app.sim.trace_valid)
                {
                    app.show_trace = !app.show_trace;
                    app.status = app.show_trace ? "Grafico visivel." : "Grafico oculto; pressione G para mostrar.";
                }
                else if (e.key.key == SDLK_T && app.selected_wire >= 0 && app.selected_wire < app.bench.wire_count)
                {
                    if (app.dc_job.thread)
                        app.status = "Analise em andamento.";
                    else if (spice_job_start_transient_wire(&app.dc_job, &app.bench, app.ngspice, app.selected_wire,
                                                            app.transient_step, app.transient_stop))
                    {
                        app.sim.trace_valid = 0;
                        app.sim.dc_valid = 0;
                        app.sim.dc_wire_valid = 0;
                        app.status = "Calculando corrente transitoria do fio...";
                    }
                    else
                        app.status = "Falha ao iniciar tarefa de analise.";
                }
                else if (e.key.key == SDLK_T && app.selected >= 0 &&
                         app.bench.parts[app.selected].type != PART_BREADBOARD)
                {
                    if (app.dc_job.thread)
                        app.status = "Analise em andamento.";
                    else if (!app.probe_current && app.probe_pin >= bench_pin_count(app.bench.parts[app.selected].type))
                        app.status = "Terminal indisponivel para este componente.";
                    else if (app.probe_current && (is_gate(app.bench.parts[app.selected].type) ||
                                                   app.bench.parts[app.selected].type == PART_74HC00))
                        app.status = "Corrente interna da porta nao disponivel.";
                    else if (app.probe_current && app.bench.parts[app.selected].type == PART_SWITCH)
                        app.status = "Corrente transitoria indisponivel para interruptor ideal.";
                    else if (app.show_current ? spice_job_start_transient_instrumented(
                                                    &app.dc_job, &app.bench, app.ngspice, app.probe_current ? 2 : 1,
                                                    (Pin){app.selected, app.probe_current ? 0 : app.probe_pin},
                                                    app.transient_step, app.transient_stop)
                             : app.probe_current
                                 ? spice_job_start_transient_current(&app.dc_job, &app.bench, app.ngspice, app.selected,
                                                                     app.transient_step, app.transient_stop)
                                 : spice_job_start_transient(&app.dc_job, &app.bench, app.ngspice,
                                                             (Pin){app.selected, app.probe_pin}, app.transient_step,
                                                             app.transient_stop))
                    {
                        app.sim.trace_valid = 0;
                        app.sim.dc_valid = 0;
                        app.sim.dc_wire_valid = 0;
                        app.status = "Calculando transitorio selecionado...";
                    }
                    else
                        app.status = "Falha ao iniciar tarefa de analise.";
                }
                else if ((e.key.key == SDLK_N || e.key.key == SDLK_B) && !app.sim.sim_running && app.sim.trace_valid)
                {
                    if (e.key.key == SDLK_N && app.sim.trace_index + 1 < app.sim.trace.count)
                        ++app.sim.trace_index;
                    if (e.key.key == SDLK_B && app.sim.trace_index > 0)
                        --app.sim.trace_index;
                    snprintf(app.sim.trace_status, sizeof(app.sim.trace_status),
                             app.sim.trace_current == 2 ? "Corrente fio A -> B: t=%.6g s; I=%.6g A (%d/%d)."
                             : app.sim.trace_current    ? "Corrente pino 1 -> 2: t=%.6g s; I=%.6g A (%d/%d)."
                                                        : "Tensao: t=%.6g s; V=%.6g V (%d/%d).",
                             app.sim.trace.samples[app.sim.trace_index].time,
                             app.sim.trace.samples[app.sim.trace_index].value, app.sim.trace_index + 1,
                             app.sim.trace.count);
                    app.status = app.sim.trace_status;
                }
                else if (e.key.key == SDLK_N && !app.sim.sim_running)
                {
                    if (bench_boolean_supported(&app.bench))
                    {
                        bench_simulate(&app.bench, app.leds);
                        app.status = "Um passo executado.";
                    }
                    else
                        app.status = "Calculo eletrico ainda indisponivel.";
                }
                else if (e.key.key == SDLK_P)
                {
                    if (app.dc_job.thread)
                        app.status = "Analise em andamento.";
                    else if ((app.show_current ? spice_job_start_wires : spice_job_start)(&app.dc_job, &app.bench,
                                                                                          app.ngspice))
                    {
                        app.sim.trace_valid = 0;
                        app.sim.dc_valid = 0;
                        app.sim.dc_wire_valid = 0;
                        app.status = "Analisando ponto DC...";
                    }
                    else
                        app.status = "Falha ao iniciar tarefa de analise.";
                }
                else if (e.key.key == SDLK_E && app.selected >= 0)
                {
                    PartType type = app.bench.parts[app.selected].type;
                    if (type == PART_SOURCE || type == PART_RESISTOR || type == PART_CAPACITOR ||
                        type == PART_INDUCTOR || type == PART_CLOCK)
                    {
                        if (SDL_StartTextInput(window))
                        {
                            app.editing = app.selected;
                            snprintf(app.edit_text, sizeof(app.edit_text), "%.9g", app.bench.parts[app.selected].value);
                            app.status = "Digite o valor em unidades SI; Enter confirma, Esc cancela.";
                        }
                        else
                            app.status = "Nao foi possivel iniciar edicao de texto.";
                    }
                    else
                        app.status = "Este componente nao possui valor editavel.";
                }
                else if (app.selected >= 0 &&
                         (e.key.key == SDLK_EQUALS || e.key.key == SDLK_PLUS || e.key.key == SDLK_MINUS))
                {
                    double scale = e.key.key == SDLK_MINUS ? .5 : 2.0;
                    Workbench before = app.bench;
                    if (bench_set_value(&app.bench, app.selected, app.bench.parts[app.selected].value * scale))
                    {
                        bench_history_record(app.history, &before);
                        app.status = "Valor alterado.";
                    }
                    else
                        app.status = "Valor nao editavel ou fora do limite.";
                }
                else if (e.key.key == SDLK_S)
                {
                    WorkbenchSettings saved = {app.show_grid,     app.show_handles,   app.round_wires,
                                               app.show_current,  app.wire_color,     app.probe_pin,
                                               app.probe_current, app.transient_step, app.transient_stop};
                    app.status = workbench_save_project(app.selftest ? "build/gui-settings-test.elec" : "bancada.elec",
                                                        &app.bench, &saved)
                                     ? "Salvo em bancada.elec."
                                     : "Falha ao salvar.";
                }
                else if (e.key.key == SDLK_L)
                {
                    Workbench before = app.bench;
                    WorkbenchSettings saved;
                    if (workbench_load_project(app.selftest ? "build/gui-settings-test.elec" : "bancada.elec",
                                               &app.bench, &saved))
                    {
                        bench_history_record(app.history, &before);
                        app.selected = -1;
                        app.selected_wire = -1;
                        app.dragging = -1;
                        app.drag_wire = -1;
                        app.pending.component = -1;
                        app.route_count = 0;
                        app.show_grid = saved.show_grid;
                        app.show_handles = saved.show_handles;
                        app.round_wires = saved.round_wires;
                        app.show_current = saved.show_current;
                        app.wire_color = saved.wire_color;
                        app.probe_pin = saved.probe_pin;
                        app.probe_current = saved.probe_current;
                        app.transient_step = saved.transient_step;
                        app.transient_stop = saved.transient_stop;
                        app.sim.dc_valid = 0;
                        app.sim.dc_wire_valid = 0;
                        app.sim.trace_valid = 0;
                        memset(app.leds, 0, sizeof(app.leds));
                        bench_simulate(&app.bench, app.leds);
                        app.status = "Bancada e configuracoes carregadas.";
                    }
                    else
                        app.status = "Falha ao carregar; bancada preservada.";
                }
                else if (e.key.key == SDLK_DELETE && app.selected >= 0)
                {
                    bench_history_record(app.history, &app.bench);
                    bench_remove(&app.bench, app.selected);
                    app.selected = -1;
                    app.selected_wire = -1;
                    app.pending.component = -1;
                    memset(app.leds, 0, sizeof(app.leds));
                    app.status = "Componente e seus fios removidos.";
                }
                else if (e.key.key == SDLK_DELETE && app.selected_wire >= 0 && app.selected_wire < app.bench.wire_count)
                {
                    bench_history_record(app.history, &app.bench);
                    memmove(&app.bench.wires[app.selected_wire], &app.bench.wires[app.selected_wire + 1],
                            (size_t)(app.bench.wire_count - app.selected_wire - 1) * sizeof(Wire));
                    --app.bench.wire_count;
                    app.selected_wire = -1;
                    app.status = "Fio removido.";
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_WHEEL && !app.settings && app.editing < 0)
            {
                int w, h;
                float x, y, direction = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
                SDL_GetWindowSize(window, &w, &h);
                if (w > 0 && h > 0)
                {
                    x = e.wheel.mouse_x * 1280.0f / w;
                    y = e.wheel.mouse_y * 720.0f / h;
                    if (x >= 220 && y >= 70 && y < 660 && direction != 0)
                        bench_view_zoom_at(&app.view, direction > 0 ? 1.2f : 1.0f / 1.2f, x, y);
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT && !app.settings &&
                     app.editing < 0)
            {
                /* Right click rotates the part under the cursor, the same as selecting it and pressing R. */
                int w, h, hit;
                float x, y;
                SDL_GetWindowSize(window, &w, &h);
                if (w <= 0 || h <= 0)
                    continue;
                x = e.button.x * 1280.0f / w;
                y = e.button.y * 720.0f / h;
                if (x < 220 || y < 70 || y > 660)
                    continue;
                bench_view_to_world(&app.view, x, y, &x, &y);
                hit = part_at(&app.bench, x, y);
                if (hit >= 0 && app.bench.parts[hit].type != PART_BREADBOARD)
                {
                    SDL_Event shortcut = {0};
                    app.selected = hit;
                    app.selected_wire = -1;
                    shortcut.type = SDL_EVENT_KEY_DOWN;
                    shortcut.key.key = SDLK_R;
                    SDL_PushEvent(&shortcut);
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE && !app.settings &&
                     app.editing < 0)
            {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                if (w > 0 && h > 0 && e.button.x * 1280.0f / w >= 220 && e.button.y * 720.0f / h >= 70 &&
                    e.button.y * 720.0f / h < 660)
                    app.panning = 1;
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_MIDDLE)
                app.panning = 0;
            else if (e.type == SDL_EVENT_MOUSE_MOTION && app.panning && app.editing < 0)
            {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                if (w > 0 && h > 0)
                {
                    app.view.pan_x += e.motion.xrel * 1280.0f / w;
                    app.view.pan_y += e.motion.yrel * 720.0f / h;
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
            {
                int w, h, hit = -1, hitpin = -1;
                float x, y;
                SDL_GetWindowSize(window, &w, &h);
                if (w <= 0 || h <= 0)
                    continue;
                x = e.button.x * 1280.0f / w;
                y = e.button.y * 720.0f / h;
                if (x >= 700 && x < 850 && y < 68)
                {
                    SDL_Event shortcut = {0};
                    shortcut.type = SDL_EVENT_KEY_DOWN;
                    shortcut.key.key = SDLK_F2;
                    SDL_PushEvent(&shortcut);
                    continue;
                }
                if (x >= 880 && x < 1030 && y < 68)
                {
                    app.settings = !app.settings;
                    app.dragging = -1;
                    app.drag_wire = -1;
                    continue;
                }
                if (x >= PLAY_BUTTON_X && y < 68)
                {
                    SDL_Event shortcut = {0};
                    shortcut.type = SDL_EVENT_KEY_DOWN;
                    shortcut.key.key = SDLK_SPACE;
                    SDL_PushEvent(&shortcut);
                    continue;
                }
                if (!app.settings && app.selected >= 0 && app.bench.parts[app.selected].type != PART_BREADBOARD &&
                    x >= ROTATE_BUTTON_X && x < ROTATE_BUTTON_X + 72 && y >= 86 && y < 112)
                {
                    SDL_Event shortcut = {0};
                    shortcut.type = SDL_EVENT_KEY_DOWN;
                    shortcut.key.key = SDLK_R;
                    SDL_PushEvent(&shortcut);
                    continue;
                }
                if (app.settings)
                {
                    if (x > 850 && x < 1240)
                    {
                        if (y >= 120 && y < 160)
                            app.show_grid = !app.show_grid;
                        else if (y >= 160 && y < 200)
                            app.show_handles = !app.show_handles;
                        else if (y >= 200 && y < 240)
                            app.round_wires = !app.round_wires;
                        else if (y >= 240 && y < 280)
                        {
                            app.show_current = !app.show_current;
                            if (app.show_current && app.sim.trace_valid && !app.sim.trace.measures_wires)
                                app.status = "Pressione T para medir correntes transitorias dos fios.";
                            else if (app.show_current && !app.sim.dc_wire_valid && !app.sim.trace_valid)
                                app.status = "Pressione P ou T para medir correntes dos fios.";
                        }
                        else if (y >= 354 && y < 390)
                            app.probe_current = !app.probe_current;
                        else if (y >= 390 && y < 425)
                            app.probe_pin =
                                (app.probe_pin + 1) %
                                ((app.selected >= 0 && app.bench.parts[app.selected].type == PART_74HC00) ? 14 : 3);
                        else if (y >= 430 && y < 465)
                        {
                            if (SDL_StartTextInput(window))
                            {
                                app.editing = WORKBENCH_MAX_COMPONENTS;
                                snprintf(app.edit_text, sizeof(app.edit_text), "%.9g", app.transient_step);
                            }
                        }
                        else if (y >= 470 && y < 505)
                        {
                            if (SDL_StartTextInput(window))
                            {
                                app.editing = WORKBENCH_MAX_COMPONENTS + 1;
                                snprintf(app.edit_text, sizeof(app.edit_text), "%.9g", app.transient_stop);
                            }
                        }
                        else if (y >= 320 && y < 352 && x >= 860 && x < 1160)
                        {
                            int chosen = (int)((x - 860) / 50);
                            if (chosen >= 0 && chosen < WIRE_COLOR_COUNT)
                            {
                                app.wire_color = chosen;
                                if (app.selected_wire >= 0 && app.selected_wire < app.bench.wire_count &&
                                    app.bench.wires[app.selected_wire].color != chosen)
                                {
                                    bench_history_record(app.history, &app.bench);
                                    app.bench.wires[app.selected_wire].color = chosen;
                                    app.status = "Cor do fio selecionado alterada.";
                                }
                                else
                                    app.status = "Cor dos proximos fios selecionada.";
                            }
                        }
                    }
                    continue;
                }
                if (app.sim.trace_valid && app.show_trace && x >= 840 && x <= 1240 && y >= 384 && y <= 628)
                {
                    if (x >= 918 && x <= 1220 && y >= 431 && y <= 565)
                    {
                        app.sim.trace_index = spice_trace_nearest(&app.sim.trace, (x - 918) / 302.0);
                        app.sim.sim_running = 0;
                        snprintf(app.sim.trace_status, sizeof(app.sim.trace_status),
                                 app.sim.trace_current == 2 ? "Corrente fio A -> B: t=%.6g s; I=%.6g A (%d/%d)."
                                 : app.sim.trace_current    ? "Corrente pino 1 -> 2: t=%.6g s; I=%.6g A (%d/%d)."
                                                            : "Tensao: t=%.6g s; V=%.6g V (%d/%d).",
                                 app.sim.trace.samples[app.sim.trace_index].time,
                                 app.sim.trace.samples[app.sim.trace_index].value, app.sim.trace_index + 1,
                                 app.sim.trace.count);
                        app.status = app.sim.trace_status;
                    }
                    continue;
                }
                if (x < 220 && y >= 80 && y < 125)
                {
                    app.palette_page = 1 - app.palette_page;
                    app.placing = -1;
                    continue;
                }
                if (x < 220 && y >= 140 && y < 716)
                {
                    int item = (int)((y - 140) / 72);
                    if (item < (app.palette_page ? 8 : 8))
                    {
                        app.placing = palette_part(app.palette_page, item);
                        app.pending.component = -1;
                        app.status = "Clique na bancada para posicionar.";
                    }
                    continue;
                }
                if (x < 220 || y < 70 || y > 660)
                    continue;
                bench_view_to_world(&app.view, x, y, &x, &y);
                if (app.pending.component < 0 && app.placing < 0 && app.show_handles)
                {
                    int k;
                    for (i = 0; i < app.bench.wire_count && app.drag_wire < 0; ++i)
                        for (k = 0; k < app.bench.wires[i].knee_count; ++k)
                        {
                            WirePoint point = app.bench.wires[i].knees[k];
                            if (fabsf(x - point.x) < 8 && fabsf(y - point.y) < 8)
                            {
                                app.drag_wire = i;
                                app.drag_knee = k;
                                break;
                            }
                        }
                    if (app.drag_wire >= 0)
                    {
                        if (e.button.clicks >= 2)
                        {
                            Wire *wire = &app.bench.wires[app.drag_wire];
                            bench_history_record(app.history, &app.bench);
                            memmove(&wire->knees[app.drag_knee], &wire->knees[app.drag_knee + 1],
                                    (size_t)(wire->knee_count - app.drag_knee - 1) * sizeof(WirePoint));
                            --wire->knee_count;
                            app.drag_wire = -1;
                            app.status = "Joelho removido.";
                        }
                        else
                        {
                            app.move_recorded = 0;
                            app.status = "Arraste o joelho; duplo clique remove.";
                        }
                        continue;
                    }
                }
                if (app.placing >= 0)
                {
                    Workbench before = app.bench;
                    int added = bench_add(&app.bench, (PartType)app.placing, x, y);
                    if (added >= 0)
                    {
                        int snapped = bench_snap_to_breadboard(&app.bench, added);
                        bench_history_record(app.history, &before);
                        app.selected_wire = -1;
                        app.status = snapped ? "Componente encaixado na breadboard."
                                             : "Componente adicionado. Selecione terminais para ligar.";
                        app.placing = -1;
                    }
                    else
                        app.status = "Limite de componentes atingido.";
                    continue;
                }
                {
                    int pass;
                    for (pass = 0; pass < 2 && hit < 0; ++pass)
                        for (i = app.bench.part_count - 1; i >= 0; --i)
                        {
                            int p, board = app.bench.parts[i].type == PART_BREADBOARD;
                            float radius = board ? 4.0f : app.bench.parts[i].type == PART_74HC00 ? 5.0f : 12.0f;
                            if (board != pass)
                                continue;
                            for (p = 0; p < bench_pin_count(app.bench.parts[i].type); ++p)
                            {
                                float px, py;
                                pin_xy(&app.bench.parts[i], p, &px, &py);
                                if ((x - px) * (x - px) + (y - py) * (y - py) <= radius * radius)
                                {
                                    hit = i;
                                    hitpin = p;
                                    break;
                                }
                            }
                            if (hit >= 0)
                                break;
                        }
                }
                if (hitpin >= 0)
                {
                    Pin clicked = {hit, hitpin};
                    app.selected_wire = -1;
                    if (app.pending.component < 0)
                    {
                        app.pending = clicked;
                        app.route_count = 0;
                        app.status = "Clique para criar joelhos; termine em outro terminal.";
                    }
                    else if (app.pending.component == hit && app.pending.pin == hitpin)
                    {
                        app.pending.component = -1;
                        app.route_count = 0;
                        app.status = "Conexao cancelada.";
                    }
                    else
                    {
                        Workbench before = app.bench;
                        int result = bench_toggle_wire(&app.bench, app.pending, clicked);
                        if (result == 1)
                        {
                            Wire *wire = &app.bench.wires[app.bench.wire_count - 1];
                            wire->knee_count = app.route_count;
                            wire->color = app.wire_color;
                            memcpy(wire->knees, app.route, (size_t)app.route_count * sizeof(WirePoint));
                        }
                        if (result)
                            bench_history_record(app.history, &before);
                        app.status = result == 1   ? "Fio conectado."
                                     : result == 2 ? "Fio removido."
                                                   : "Nao foi possivel conectar.";
                        app.pending.component = -1;
                        app.route_count = 0;
                    }
                    continue;
                }
                if (app.pending.component >= 0)
                {
                    if (app.route_count < WIRE_MAX_KNEES)
                    {
                        app.route[app.route_count++] = (WirePoint){x, y};
                        app.status = "Joelho adicionado; termine em um terminal.";
                    }
                    else
                        app.status = "Limite de 16 joelhos por fio.";
                    continue;
                }
                hit = part_at(&app.bench, x, y);
                app.selected = hit;
                app.selected_wire = -1;
                if (hit < 0 || app.bench.parts[hit].type == PART_BREADBOARD)
                {
                    app.selected_wire = hit_wire(&app.bench, x, y, 7.0f / app.view.zoom, app.round_wires);
                    if (app.selected_wire >= 0)
                    {
                        app.selected = -1;
                        app.wire_color = app.bench.wires[app.selected_wire].color;
                        app.status = "Fio selecionado; F1 abre as cores.";
                        continue;
                    }
                }
                if (hit >= 0)
                {
                    if (e.button.clicks >= 2 &&
                        (app.bench.parts[hit].type == PART_SOURCE || app.bench.parts[hit].type == PART_SWITCH ||
                         app.bench.parts[hit].type == PART_CLOCK))
                    {
                        bench_history_record(app.history, &app.bench);
                        app.bench.parts[hit].on = !app.bench.parts[hit].on;
                        app.status = "Estado alterado.";
                    }
                    else
                    {
                        app.dragging = hit;
                        app.move_recorded = 0;
                        app.offset_x = x - app.bench.parts[hit].x;
                        app.offset_y = y - app.bench.parts[hit].y;
                        app.status = "Arraste para mover; dois cliques alternam fonte ou chave.";
                    }
                }
                else
                {
                    app.pending.component = -1;
                    app.status = "Bancada livre.";
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
            {
                if (app.dragging >= 0 && app.move_recorded && bench_snap_to_breadboard(&app.bench, app.dragging))
                    app.status = "Componente encaixado na breadboard.";
                app.dragging = -1;
                app.drag_wire = -1;
            }
            else if (e.type == SDL_EVENT_MOUSE_MOTION && app.drag_wire >= 0)
            {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                if (w > 0 && h > 0 && app.drag_wire < app.bench.wire_count)
                {
                    float x = e.motion.x * 1280.0f / w, y = e.motion.y * 720.0f / h;
                    WirePoint *point = &app.bench.wires[app.drag_wire].knees[app.drag_knee];
                    bench_view_to_world(&app.view, x, y, &x, &y);
                    if (point->x != x || point->y != y)
                    {
                        if (!app.move_recorded)
                        {
                            bench_history_record(app.history, &app.bench);
                            app.move_recorded = 1;
                        }
                        *point = (WirePoint){x, y};
                    }
                }
            }
            else if (e.type == SDL_EVENT_MOUSE_MOTION && app.dragging >= 0)
            {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                if (w > 0 && h > 0)
                {
                    float x = e.motion.x * 1280.0f / w, y = e.motion.y * 720.0f / h;
                    bench_view_to_world(&app.view, x, y, &x, &y);
                    x -= app.offset_x;
                    y -= app.offset_y;
                    if (app.bench.parts[app.dragging].x != x || app.bench.parts[app.dragging].y != y)
                    {
                        if (!app.move_recorded)
                        {
                            bench_history_record(app.history, &app.bench);
                            app.move_recorded = 1;
                        }
                        app.bench.parts[app.dragging].x = x;
                        app.bench.parts[app.dragging].y = y;
                    }
                }
            }
        }
        if (app.selftest)
            dev_scenario_validate(&app);
        app_simulation_update(&app);
        SDL_GetWindowSizeInPixels(window, &width, &height);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 1280, 720, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(.92f, .94f, .95f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        color(.97f, .98f, .98f);
        rect(0, 0, 1280, 68);
        color(.11f, .2f, .27f);
        text(font, 24, 42, "ELECTRONICS  /  BANCADA EXPERIMENTAL");
        text(font, 700, 42, "Exemplos [F2]");
        text(font, 880, 42, "Configuracoes [F1]");
        play_button_draw(font, app.sim.sim_running, app.dc_job.thread != NULL, app.sim.auto_failed,
                         app.sim.burnt_count);
        color(.98f, .985f, .99f);
        rect(0, 68, 220, 652);
        color(.17f, .24f, .29f);
        text(font, 22, 108, app.palette_page ? "PORTAS [TAB]" : "COMPONENTES [TAB]");
        for (i = 0; i < (app.palette_page ? 8 : 8); ++i)
        {
            float y = 140 + i * 72;
            PartType type = palette_part(app.palette_page, i);
            color(app.placing == (int)type ? .79f : .92f, app.placing == (int)type ? .9f : .94f, .96f);
            rect(14, y, 192, 62);
            color(.16f, .22f, .26f);
            text(font, 76, y + 36, part_name(type));
            color(type == PART_SOURCE   ? .2f
                  : type == PART_SWITCH ? .38f
                                        : .84f,
                  type == PART_SOURCE ? .54f : .42f, type == PART_LED ? .28f : .6f);
            circle(42, y + 31, 13);
        }
        glEnable(GL_SCISSOR_TEST);
        glScissor((int)(width * 220.0 / 1280), (int)(height * 60.0 / 720), (int)(width * 1060.0 / 1280),
                  (int)(height * 590.0 / 720));
        glPushMatrix();
        glTranslatef(220 + app.view.pan_x, 70 + app.view.pan_y, 0);
        glScalef(app.view.zoom, app.view.zoom, 1);
        glTranslatef(-220, -70, 0);
        color(.85f, .88f, .89f);
        glLineWidth(1);
        if (app.show_grid)
        {
            /* Cover the whole visible area at any zoom; widen the spacing when lines would crowd together. */
            float left, top, right, bottom, spacing = 24.0f, gx, gy;
            bench_view_to_world(&app.view, 220, 60, &left, &top);
            bench_view_to_world(&app.view, 1280, 660, &right, &bottom);
            while (spacing * app.view.zoom < 10.0f)
                spacing *= 2.0f;
            for (gx = floorf(left / spacing) * spacing; gx <= right; gx += spacing)
                line(gx, top, gx, bottom);
            for (gy = floorf((top - 20.0f) / spacing) * spacing + 20.0f; gy <= bottom; gy += spacing)
                line(left, gy, right, gy);
        }
        for (i = 0; i < app.bench.part_count; ++i)
            if (app.bench.parts[i].type == PART_BREADBOARD)
            {
                int p;
                part_draw(&app.bench.parts[i], app.selected == i, app.leds[i]);
                for (p = 0; p < bench_pin_count(PART_BREADBOARD); ++p)
                {
                    float x, y;
                    pin_xy(&app.bench.parts[i], p, &x, &y);
                    color(.18f, .22f, .24f);
                    circle(x, y, 3.0f);
                    if (app.pending.component == i && app.pending.pin == p)
                    {
                        color(1, .7f, .2f);
                        circle(x, y, 3.0f);
                    }
                }
            }
        color(.25f, .45f, .57f);
        glLineWidth(3);
        for (i = 0; i < app.bench.wire_count; ++i)
        {
            WirePoint start, end, path[WIRE_PATH_CAPACITY];
            int k, count, pass;
            Wire *w = &app.bench.wires[i];
            pin_xy(&app.bench.parts[w->a.component], w->a.pin, &start.x, &start.y);
            pin_xy(&app.bench.parts[w->b.component], w->b.pin, &end.x, &end.y);
            count = wire_path(w, start, end, app.round_wires ? 18.0f : 0.0f, path);
            if (i == app.selected_wire)
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
            if (app.show_current && app.sim.trace_valid && app.sim.trace.measures_wires)
            {
                uint64_t bit = UINT64_C(1) << (i % 64);
                int forward = (app.sim.trace.wires_forward[app.sim.trace_index][i / 64] & bit) != 0,
                    reverse = (app.sim.trace.wires_reverse[app.sim.trace_index][i / 64] & bit) != 0;
                if (forward || reverse)
                {
                    int level = (app.sim.trace.wire_levels[app.sim.trace_index][i / 4] >> ((i % 4) * 2)) & 3;
                    wire_current_style(level);
                    glBegin(GL_LINE_STRIP);
                    for (k = 0; k < count; ++k)
                        glVertex2f(path[k].x, path[k].y);
                    glEnd();
                    wire_direction_marker(path, count, reverse);
                }
            }
            if (app.show_current && app.sim.dc_valid && app.sim.dc_wire_valid &&
                spice_wire_current_level(app.sim.dc_wire_currents[i]) > 0)
            {
                float total = 0, position;
                int segment;
                wire_current_style(spice_wire_current_level(app.sim.dc_wire_currents[i]));
                glBegin(GL_LINE_STRIP);
                for (k = 0; k < count; ++k)
                    glVertex2f(path[k].x, path[k].y);
                glEnd();
                for (k = 1; k < count; ++k)
                    total += hypotf(path[k].x - path[k - 1].x, path[k].y - path[k - 1].y);
                if (total > 0)
                {
                    position = app.sim.sim_running ? fmodf((float)SDL_GetTicks() * .08f, total) : 0.0f;
                    if (app.sim.dc_wire_currents[i] < 0)
                        position = total - position;
                    for (segment = 1; segment < count; ++segment)
                    {
                        float dx = path[segment].x - path[segment - 1].x, dy = path[segment].y - path[segment - 1].y,
                              length = hypotf(dx, dy);
                        if (length > 0 && position <= length)
                        {
                            color(1, .9f, .38f);
                            circle(path[segment - 1].x + dx * position / length,
                                   path[segment - 1].y + dy * position / length, 4);
                            break;
                        }
                        position -= length;
                    }
                }
            }
        }
        if (app.pending.component >= 0)
        {
            float x, y;
            int k;
            pin_xy(&app.bench.parts[app.pending.component], app.pending.pin, &x, &y);
            color(.9f, .55f, .15f);
            for (k = 0; k < app.route_count; ++k)
            {
                line(x, y, app.route[k].x, app.route[k].y);
                x = app.route[k].x;
                y = app.route[k].y;
            }
        }
        if (app.show_handles)
            for (i = 0; i < app.bench.wire_count; ++i)
            {
                int k;
                for (k = 0; k < app.bench.wires[i].knee_count; ++k)
                {
                    WirePoint p = app.bench.wires[i].knees[k];
                    color(.1f, .3f, .45f);
                    rect(p.x - 4, p.y - 4, 8, 8);
                }
            }
        glLineWidth(1);
        for (i = 0; i < app.bench.part_count; ++i)
            if (app.bench.parts[i].type != PART_BREADBOARD)
            {
                int p;
                part_draw_oriented(&app.bench.parts[i], app.selected == i, app.leds[i]);
                for (p = 0; p < bench_pin_count(app.bench.parts[i].type); ++p)
                {
                    float x, y;
                    Pin contact;
                    int snapped = bench_find_contact(&app.bench, i, p, &contact);
                    pin_xy(&app.bench.parts[i], p, &x, &y);
                    color(.18f, .22f, .24f);
                    circle(x, y, app.bench.parts[i].type == PART_74HC00 ? 4.0f : snapped ? 5.0f : 8.0f);
                    if (app.pending.component == i && app.pending.pin == p)
                    {
                        color(1, .7f, .2f);
                        circle(x, y, app.bench.parts[i].type == PART_74HC00 ? 2.5f : snapped ? 3.0f : 5.0f);
                    }
                    else if (snapped)
                    {
                        color(.18f, .78f, .46f);
                        circle(x, y, app.bench.parts[i].type == PART_74HC00 ? 2.5f : 3.0f);
                    }
                    else
                    {
                        color(.93f, .95f, .96f);
                        circle(x, y, app.bench.parts[i].type == PART_74HC00 ? 2.5f : 5.0f);
                    }
                }
            }
        for (i = 0; i < app.bench.wire_count; ++i)
        {
            const Wire *wire = &app.bench.wires[i];
            WirePoint start, end, path[WIRE_PATH_CAPACITY];
            int count, k;
            pin_xy(&app.bench.parts[wire->a.component], wire->a.pin, &start.x, &start.y);
            pin_xy(&app.bench.parts[wire->b.component], wire->b.pin, &end.x, &end.y);
            count = wire_path(wire, start, end, app.round_wires ? 18.0f : 0.0f, path);
            if (app.bench.parts[wire->a.component].type == PART_BREADBOARD)
                for (k = 1; k < count; ++k)
                    if (hypotf(path[k].x - start.x, path[k].y - start.y) > .01f)
                    {
                        wire_contact_tip(start, path[k], wire->color);
                        break;
                    }
            if (app.bench.parts[wire->b.component].type == PART_BREADBOARD)
                for (k = count - 2; k >= 0; --k)
                    if (hypotf(path[k].x - end.x, path[k].y - end.y) > .01f)
                    {
                        wire_contact_tip(end, path[k], wire->color);
                        break;
                    }
        }
        for (i = 0; i < app.bench.part_count; ++i)
            if (app.bench.parts[i].type == PART_74HC00)
            {
                color(.85f, .88f, .9f);
                text(font, app.bench.parts[i].x - 32, app.bench.parts[i].y + 5, "74HC00");
            }
        for (i = 0; i < app.bench.part_count; ++i)
            if (is_gate(app.bench.parts[i].type))
            {
                color(.13f, .22f, .27f);
                text(font, app.bench.parts[i].x - 25, app.bench.parts[i].y + 45,
                     part_name(app.bench.parts[i].type) + 6);
            }
        if (app.selected >= 0 &&
            (is_gate(app.bench.parts[app.selected].type) || app.bench.parts[app.selected].type == PART_CLOCK))
        {
            int terminal;
            color(.10f, .25f, .36f);
            for (terminal = 0; terminal < bench_pin_count(app.bench.parts[app.selected].type); ++terminal)
            {
                float x, y;
                char label[2] = {
                    app.bench.parts[app.selected].type == PART_DFF ? "DCQ"[terminal] : (char)('1' + terminal), 0};
                pin_xy(&app.bench.parts[app.selected], terminal, &x, &y);
                text(font, x + 8, y - 8, label);
            }
        }
        if (app.selected >= 0 && app.bench.parts[app.selected].type == PART_74HC00)
        {
            static const int labels[] = {0, 6, 7, 13};
            int n;
            color(.1f, .25f, .36f);
            for (n = 0; n < 4; ++n)
            {
                float x, y;
                char label[16];
                pin_xy(&app.bench.parts[app.selected], labels[n], &x, &y);
                snprintf(label, sizeof(label), "%d", labels[n] + 1);
                text(font, x - 5, y + (labels[n] < 7 ? -7 : 18), label);
            }
        }
        if (app.sim.trace_valid && app.sim.trace_current == 2 && app.sim.trace_wire >= 0 &&
            app.sim.trace_wire < app.bench.wire_count)
        {
            Wire *wire = &app.bench.wires[app.sim.trace_wire];
            float ax, ay, bx, by;
            pin_xy(&app.bench.parts[wire->a.component], wire->a.pin, &ax, &ay);
            pin_xy(&app.bench.parts[wire->b.component], wire->b.pin, &bx, &by);
            color(.9f, .35f, .05f);
            circle(ax, ay, 9);
            circle(bx, by, 9);
            color(.12f, .16f, .19f);
            text(font, ax + 10, ay - 5, "A");
            text(font, bx + 10, by - 5, "B");
        }
        for (i = 0; i < app.bench.part_count; ++i)
            if (app.sim.burnt[i])
                burnt_draw(font, &app.bench.parts[i], SDL_GetTicks());
        glPopMatrix();
        glDisable(GL_SCISSOR_TEST);
        if (app.selected >= 0)
        {
            Part *part = &app.bench.parts[app.selected];
            char reading[120];
            const char *unit = part->type == PART_SOURCE      ? "V"
                               : part->type == PART_RESISTOR  ? "ohm"
                               : part->type == PART_CAPACITOR ? "F"
                               : part->type == PART_INDUCTOR  ? "H"
                               : part->type == PART_CLOCK     ? "Hz"
                                                              : NULL;
            int shows_current = app.sim.dc_valid && !app.sim.burnt[app.selected] &&
                                (part->type == PART_LED || part->type == PART_RESISTOR);
            color(.97f, .98f, .98f);
            rect(840, 80, 420, (shows_current || app.sim.burnt[app.selected]) ? 70 : 38);
            color(.14f, .22f, .28f);
            if (unit)
            {
                snprintf(reading, sizeof(reading), "Valor: %.5g %s  [E / + / -]", part->value, unit);
                text(font, 850, 105, reading);
            }
            else
                text(font, 850, 105,
                     part->type == PART_LED                               ? "LED generico"
                     : (is_gate(part->type) || part->type == PART_74HC00) ? part_name(part->type)
                                                                          : "Componente selecionado");
            if (part->type != PART_BREADBOARD)
            {
                color(.86f, .91f, .95f);
                rect(ROTATE_BUTTON_X, 86, 72, 26);
                color(.12f, .34f, .52f);
                text(font, ROTATE_BUTTON_X + 8, 105, "Girar R");
            }
            if (app.sim.burnt[app.selected])
            {
                color(.72f, .12f, .08f);
                if (app.sim.burn_reason[app.selected] == BURN_SHORT)
                    snprintf(reading, sizeof(reading), "QUEIMADO: curto-circuito nos terminais");
                else if (app.sim.burn_reason[app.selected] == BURN_OVERPOWER)
                    snprintf(reading, sizeof(reading), "QUEIMADO: %.3g W (limite %.3g W)",
                             app.sim.burn_value[app.selected], burn_limits_default().resistor_max_power);
                else
                    snprintf(reading, sizeof(reading), "QUEIMADO: %.4g mA (limite %.3g mA)",
                             app.sim.burn_value[app.selected] * 1000.0, burn_limits_default().led_max_current * 1000.0);
                text(font, 850, 137, reading);
            }
            else if (shows_current)
            {
                snprintf(reading, sizeof(reading), "Corrente: %.5g mA (pino 1 -> 2)",
                         app.sim.dc_led_currents[app.selected] * 1000.0);
                text(font, 850, 137, reading);
            }
        }
        if (app.sim.trace_valid && app.show_trace && !app.settings)
            trace_draw(font, &app.sim.trace, app.sim.trace_index, app.sim.trace_pin, app.sim.trace_current);
        color(.97f, .98f, .98f);
        rect(220, 660, 1060, 60);
        color(.2f, .26f, .3f);
        text(font, 238, 685, app.status);
        text(font, 238, 709,
             app.sim.auto_enabled ? "Espaco: executar/pausar | R ou botao direito: girar | Duplo clique: liga/desliga "
                                    "| E: valor | T: grafico | Delete: excluir"
                                  : "Terminais: fios | Duplo clique: alternar | P: DC | T: transitorio | Espaco/N: "
                                    "pausa/passo | Delete: excluir");
        if (app.settings)
        {
            int swatch;
            color(.62f, .68f, .72f);
            rect(846, 90, 400, 558);
            color(.98f, .98f, .99f);
            rect(840, 84, 400, 558);
            color(.14f, .22f, .28f);
            text(font, 860, 112, "CONFIGURACOES DE VISUALIZACAO");
            text(font, 860, 148, app.show_grid ? "[x] Grade" : "[ ] Grade");
            text(font, 860, 188,
                 app.show_handles ? "[x] Pontos de controle dos fios" : "[ ] Pontos de controle dos fios");
            text(font, 860, 228, app.round_wires ? "[x] Curvas arredondadas" : "[ ] Curvas arredondadas");
            text(font, 860, 268,
                 app.show_current ? "[x] Visualizar corrente nos fios" : "[ ] Visualizar corrente nos fios");
            text(font, 860, 309, app.selected_wire >= 0 ? "COR DO FIO SELECIONADO" : "COR DO PROXIMO FIO");
            for (swatch = 0; swatch < WIRE_COLOR_COUNT; ++swatch)
            {
                float sx = 860 + swatch * 50;
                color(.2f, .25f, .29f);
                rect(sx - 2, 318, 36, 30);
                color(wire_palette[swatch][0], wire_palette[swatch][1], wire_palette[swatch][2]);
                rect(sx, 320, 32, 26);
                if (swatch == app.wire_color)
                {
                    color(1, .8f, .15f);
                    glLineWidth(2);
                    glBegin(GL_LINE_LOOP);
                    glVertex2f(sx - 3, 317);
                    glVertex2f(sx + 35, 317);
                    glVertex2f(sx + 35, 349);
                    glVertex2f(sx - 3, 349);
                    glEnd();
                    glLineWidth(1);
                }
            }
            color(.14f, .22f, .28f);
            text(font, 1170, 340, wire_color_names[app.wire_color]);
            {
                char setting[100];
                text(font, 860, 377, app.probe_current ? "Medida: CORRENTE (clique)" : "Medida: TENSAO (clique)");
                snprintf(setting, sizeof(setting), "Terminal de sondagem: %d (clique)", app.probe_pin + 1);
                text(font, 860, 415, setting);
                snprintf(setting, sizeof(setting), "Passo solicitado: %.6g s (clique)", app.transient_step);
                text(font, 860, 455, setting);
                snprintf(setting, sizeof(setting), "Duracao: %.6g s (clique)", app.transient_stop);
                text(font, 860, 495, setting);
            }
            text(font, 860, 535, "T: transitorio | Espaco: reproduzir | B/N: amostras");
            text(font, 860, 565, "1-100 uA | 100 uA-10 mA | >=10 mA");
            text(font, 860, 590, "Largura e cor indicam faixas de corrente");
            text(font, 860, 625, "F1 fecha este painel");
        }
        if (app.editing >= 0)
        {
            char reading[110];
            color(.62f, .68f, .72f);
            rect(446, 268, 400, 146);
            color(.98f, .98f, .99f);
            rect(440, 262, 400, 146);
            color(.14f, .22f, .28f);
            text(font, 460, 294,
                 app.editing >= WORKBENCH_MAX_COMPONENTS ? "TEMPO EM SEGUNDOS" : "VALOR EM UNIDADES SI");
            snprintf(reading, sizeof(reading), "> %s_", app.edit_text);
            text(font, 460, 336, reading);
            text(font, 460, 380, "Enter confirma | Esc cancela");
        }
        SDL_GL_SwapWindow(window);
        if (app.capture)
        {
            if (!dev_capture_save("build/bancada-capture.bmp", width, height))
            {
                fprintf(stderr, "Captura: %s\n", SDL_GetError());
                app.exit_code = 1;
            }
            app.running = 0;
        }
    }
    spice_job_join(&app.dc_job);
    if (app.selftest)
        remove("build/gui-settings-test.elec");
    free(app.history);
    if (font)
        glDeleteLists(font, 128);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return app.exit_code;
}
