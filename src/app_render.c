#include <windows.h>
#include "app_render.h"
#include "app_layout.h"
#include "app_palette.h"
#include "component_registry.h"
#include "workbench_pack.h"
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

static void draw_part_source(const Part *p)
{
    float x = p->x, y = p->y;
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

static void draw_part_switch(const Part *p)
{
    float x = p->x, y = p->y;
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

static void draw_part_resistor(const Part *p)
{
    float x = p->x, y = p->y;
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

static void draw_part_capacitor(const Part *p)
{
    float x = p->x, y = p->y;
    color(.16f, .19f, .25f);
    rect(x - 18, y - 23, 36, 46);
    color(.25f, .31f, .42f);
    rect(x - 18, y - 26, 36, 44);
    color(.55f, .62f, .7f);
    rect(x + 9, y - 22, 4, 34);
}

static void draw_part_inductor(const Part *p)
{
    float x = p->x, y = p->y;
    int coil;
    color(.25f, .2f, .16f);
    rect(x - 31, y - 13, 62, 26);
    color(.66f, .36f, .18f);
    rect(x - 29, y - 15, 58, 23);
    color(.94f, .65f, .35f);
    for (coil = 0; coil < 6; ++coil)
        rect(x - 26 + coil * 9, y - 14, 3, 21);
}

static void draw_part_led(const Part *p, int led)
{
    float x = p->x, y = p->y;
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

static void part_draw(const Part *p, int selected, int led)
{
    float x = p->x, y = p->y;
    if (p->type == PART_BREADBOARD)
    {
        draw_part_board(p, selected);
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
        draw_part_source(p);
    }
    else if (p->type == PART_SWITCH)
    {
        draw_part_switch(p);
    }
    else if (p->type == PART_CLOCK)
    {
        draw_part_clock(p);
    }
    else if (p->type == PART_74HC00)
    {
        draw_part_chip(p);
    }
    else if (is_gate(p->type))
    {
        draw_part_gate(p);
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
        draw_part_led(p, led);
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

static void draw_thumbnail(PartType type, float x, float y)
{
    Part preview = {0};
    preview.type = type;
    preview.on = 1;
    preview.board_layout = 1;
    float scale = type == PART_BREADBOARD ? .09f : .42f;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1);
    glLineWidth(1);
    part_draw(&preview, 0, 0);
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
}

static void draw_chrome(const AppState *app, GLuint font, int width, int height)
{
    int i, empty_shown = 0;
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
    text(font, 510, 42, "Projetos [F3]");
    text(font, 700, 42, "Exemplos [F2]");
    text(font, 880, 42, "Configuracoes [F1]");
    play_button_draw(font, app->sim.sim_running, app->dc_job.thread != NULL, app->sim.auto_failed,
                     app->sim.burnt_count);
    color(.98f, .985f, .99f);
    rect(0, 68, 220, 652);
    color(.17f, .24f, .29f);
    static const char *tabs[] = {"Basicos", "Digital", "Blocos"};
    for (int tab = 0; tab < 3; ++tab)
    {
        float x = 12 + tab * 70;
        color(app->palette_page == tab ? .78f : .92f, .9f, .96f);
        rect(x, 80, 62, 44);
        color(.14f, .23f, .28f);
        text(font, x + 4, 108, tabs[tab]);
    }
    for (i = 0; i < (app->palette_page ? 8 : 8); ++i)
    {
        float y = 140 + i * 72;
        if (app->palette_page == 2)
        {
            if (!app->library_available[i] && empty_shown++)
                continue;
            color(.9f, .94f, .97f);
            rect(14, y, 192, 62);
            draw_thumbnail(PART_74HC00, 42, y + 31);
            char label[48];
            snprintf(label, sizeof(label), app->library_available[i] ? "Bloco %d" : "Criar: Ctrl+G", i + 1);
            color(.16f, .22f, .26f);
            text(font, 76, y + 36, label);
            continue;
        }
        PartType type = palette_part(app->palette_page, i);
        color(app->placing == (int)type ? .79f : .92f, app->placing == (int)type ? .9f : .94f, .96f);
        rect(14, y, 192, 62);
        color(.16f, .22f, .26f);
        text(font, 76, y + 36, part_name(type));
        draw_thumbnail(type, 42, y + 31);
    }
}

static void draw_workspace_grid(const AppState *app)
{
    color(.85f, .88f, .89f);
    glLineWidth(1);
    if (app->show_grid)
    {
        /* Cover the whole visible area at any zoom; widen the spacing when lines would crowd together. */
        float left, top, right, bottom, spacing = 24.0f, gx, gy;
        bench_view_to_world(&app->view, 220, 60, &left, &top);
        bench_view_to_world(&app->view, 1280, 660, &right, &bottom);
        while (spacing * app->view.zoom < 10.0f)
            spacing *= 2.0f;
        for (gx = floorf(left / spacing) * spacing; gx <= right; gx += spacing)
            line(gx, top, gx, bottom);
        for (gy = floorf((top - 20.0f) / spacing) * spacing + 20.0f; gy <= bottom; gy += spacing)
            line(left, gy, right, gy);
    }
}

static void draw_workspace_boards(const AppState *app)
{
    int i;
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && app->bench.parts[i].type == PART_BREADBOARD)
        {
            int p;
            part_draw(&app->bench.parts[i], (app->selected == i || app->selection.parts[i]), app->leds[i]);
            for (p = 0; p < bench_pin_count(PART_BREADBOARD); ++p)
            {
                float x, y;
                bench_pin_position(&app->bench.parts[i], p, &x, &y);
                color(.18f, .22f, .24f);
                circle(x, y, 3.0f);
                if (app->pending.component == i && app->pending.pin == p)
                {
                    color(1, .7f, .2f);
                    circle(x, y, 3.0f);
                }
            }
        }
}

static void draw_wire_trace_current(const AppState *app, int i, const WirePoint *path, int count)
{
    int k;
    if (app->show_current && sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->sim.trace.measures_wires)
    {
        uint64_t bit = UINT64_C(1) << (i % 64);
        int forward = (app->sim.trace.wires_forward[app->sim.trace_index][i / 64] & bit) != 0,
            reverse = (app->sim.trace.wires_reverse[app->sim.trace_index][i / 64] & bit) != 0;
        if (forward || reverse)
        {
            int level = (app->sim.trace.wire_levels[app->sim.trace_index][i / 4] >> ((i % 4) * 2)) & 3;
            wire_current_style(level);
            glBegin(GL_LINE_STRIP);
            for (k = 0; k < count; ++k)
                glVertex2f(path[k].x, path[k].y);
            glEnd();
            wire_direction_marker(path, count, reverse);
        }
    }
}

static void draw_wire_dc_current(const AppState *app, int i, const WirePoint *path, int count)
{
    int k;
    if (app->show_current && sim_state_has_result(&app->sim, SIM_RESULT_DC) && app->sim.dc_wire_valid &&
        spice_wire_current_level(app->sim.dc_wire_currents[i]) > 0)
    {
        float total = 0, position;
        int segment;
        wire_current_style(spice_wire_current_level(app->sim.dc_wire_currents[i]));
        glBegin(GL_LINE_STRIP);
        for (k = 0; k < count; ++k)
            glVertex2f(path[k].x, path[k].y);
        glEnd();
        for (k = 1; k < count; ++k)
            total += hypotf(path[k].x - path[k - 1].x, path[k].y - path[k - 1].y);
        if (total > 0)
        {
            position = app->sim.sim_running ? fmodf((float)SDL_GetTicks() * .08f, total) : 0.0f;
            if (app->sim.dc_wire_currents[i] < 0)
                position = total - position;
            for (segment = 1; segment < count; ++segment)
            {
                float dx = path[segment].x - path[segment - 1].x, dy = path[segment].y - path[segment - 1].y,
                      length = hypotf(dx, dy);
                if (length > 0 && position <= length)
                {
                    color(1, .9f, .38f);
                    circle(path[segment - 1].x + dx * position / length, path[segment - 1].y + dy * position / length,
                           4);
                    break;
                }
                position -= length;
            }
        }
    }
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
            part_draw_oriented(&app->bench.parts[i], (app->selected == i || app->selection.parts[i]), app->leds[i]);
            for (p = 0; p < bench_pin_count(app->bench.parts[i].type); ++p)
            {
                float x, y;
                Pin contact;
                int snapped = bench_find_contact(&app->bench, i, p, &contact);
                float outer_radius = snapped ? 5.0f : 8.0f;
                float inner_radius = snapped ? 3.0f : 5.0f;
                if (!app->bench.parts[i].group && app->bench.parts[i].type == PART_74HC00)
                {
                    outer_radius = 4.0f;
                    inner_radius = 2.5f;
                }
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
                    circle(x, y, app->bench.parts[i].type == PART_74HC00 ? 2.5f : 3.0f);
                }
                else
                {
                    color(.93f, .95f, .96f);
                    circle(x, y, app->bench.parts[i].type == PART_74HC00 ? 2.5f : 5.0f);
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
    if (role == PIN_SUPPLY_POSITIVE || role == PIN_ANODE)
        color(.78f, .22f, .15f);
    else if (role == PIN_SUPPLY_RETURN || role == PIN_GROUND || role == PIN_CATHODE)
        color(.15f, .37f, .7f);
    else if (role == PIN_INPUT || role == PIN_CLOCK_INPUT)
        color(.65f, .36f, .1f);
    else if (role == PIN_OUTPUT)
        color(.08f, .48f, .28f);
    else
        color(.2f, .28f, .36f);
}

static char pin_role_symbol(ComponentPinRole role)
{
    static const char symbols[] = {'B', 'I', 'C', 'O', '+', '-', 'G', 'A', 'K', '.'};
    return (unsigned)role < sizeof(symbols) / sizeof(symbols[0]) ? symbols[role] : '?';
}

static void draw_selected_pin_labels(const AppState *app, GLuint font)
{
    if (app->selected < 0)
        return;
    const Part *part = &app->bench.parts[app->selected];
    if (part->group || part->type == PART_BREADBOARD)
        return;
    for (int pin = 0; pin < bench_pin_count(part->type); ++pin)
    {
        const ComponentPinInfo *info = component_pin_info(part->type, pin);
        float x, y;
        bench_pin_position(part, pin, &x, &y);
        pin_role_color(info->role);
        if (part->type == PART_74HC00)
        {
            char label[] = {pin_role_symbol(info->role), 0};
            if (part->rotation % 2)
                text(font, x + (x < part->x ? -16 : 10), y + 5, label);
            else
                text(font, x - 4, y + (y < part->y ? -9 : 20), label);
        }
        else
        {
            char label[40];
            const char *marker = component_pin_marker(info->role);
            if (!strcmp(info->name, marker))
                snprintf(label, sizeof(label), "%s", info->name);
            else
                snprintf(label, sizeof(label), "%s %s", info->name, marker);
            text(font, x + 8, y - 10, label);
        }
    }
}

static void draw_workspace_labels(const AppState *app, GLuint font)
{
    int i;
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && app->bench.parts[i].type == PART_74HC00)
        {
            color(.85f, .88f, .9f);
            text(font, app->bench.parts[i].x - 32, app->bench.parts[i].y + 5, "74HC00");
        }
    for (i = 0; i < app->bench.part_count; ++i)
        if (!app->bench.parts[i].group && is_gate(app->bench.parts[i].type))
        {
            color(.13f, .22f, .27f);
            text(font, app->bench.parts[i].x - 25, app->bench.parts[i].y + 45, part_name(app->bench.parts[i].type) + 6);
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

static void draw_groups(const AppState *app, GLuint font)
{
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
    {
        const BenchGroup *group = &app->bench.groups[g];
        if (!group->active)
            continue;
        float height = bench_group_height(&app->bench, g + 1);
        int selected = 0, lit = 0, burnt = 0;
        for (int i = 0; i < app->bench.part_count; ++i)
            if (app->bench.parts[i].group == g + 1)
            {
                selected |= app->selection.parts[i] || app->selected == i;
                lit |= app->leds[i] == 1 && app->bench.parts[i].type == PART_LED;
                burnt |= app->sim.burnt[i];
            }
        color(.65f, .69f, .72f);
        rect(group->x - 46, group->y - height * .5f + 4, 98, height);
        color(burnt ? .45f : .16f, .2f, .25f);
        rect(group->x - 49, group->y - height * .5f, 98, height);
        color(selected ? .22f : .55f, .7f, .9f);
        glLineWidth(selected ? 3 : 1);
        glBegin(GL_LINE_LOOP);
        glVertex2f(group->x - 50, group->y - height * .5f);
        glVertex2f(group->x + 50, group->y - height * .5f);
        glVertex2f(group->x + 50, group->y + height * .5f);
        glVertex2f(group->x - 50, group->y + height * .5f);
        glEnd();
        glLineWidth(1);
        char label[40];
        snprintf(label, sizeof(label), "Bloco %d", g + 1);
        color(.9f, .94f, .98f);
        text(font, group->x - 30, group->y + 5, label);
        if (lit)
        {
            color(.3f, .95f, .45f);
            circle(group->x + 36, group->y - height * .5f + 8, 3);
        }
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

static void draw_inspector(const AppState *app, GLuint font)
{
    if (app->selected >= 0 && app->bench.parts[app->selected].group)
    {
        int group = app->bench.parts[app->selected].group;
        char label[100];
        color(.97f, .98f, .99f);
        rect(840, 80, 420, 70);
        color(.12f, .23f, .3f);
        snprintf(label, sizeof(label), "BLOCO %d  /  %d terminais eletricos", group,
                 bench_group_ports(&app->bench, group));
        text(font, 850, 105, label);
        text(font, 850, 135, "Ctrl+Shift+G: desempacotar | Ctrl+C/V: copiar");
        return;
    }
    if (app->selected >= 0)
    {
        const Part *part = &app->bench.parts[app->selected];
        char reading[120];
        const ComponentInfo *info = component_info(part->type);
        const char *unit = info && info->primary ? info->primary->unit : NULL;
        int shows_current = sim_state_has_result(&app->sim, SIM_RESULT_DC) && !app->sim.burnt[app->selected] &&
                            (part->type == PART_LED || part->type == PART_RESISTOR);
        color(.97f, .98f, .98f);
        rect(840, 80, 420, (shows_current || app->sim.burnt[app->selected]) ? 70 : 38);
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
        if (app->sim.burnt[app->selected])
        {
            color(.72f, .12f, .08f);
            if (app->sim.burn_reason[app->selected] == BURN_SHORT)
                snprintf(reading, sizeof(reading), "QUEIMADO: curto-circuito nos terminais");
            else if (app->sim.burn_reason[app->selected] == BURN_OVERPOWER)
                snprintf(reading, sizeof(reading), "QUEIMADO: %.3g W (limite %.3g W)",
                         app->sim.burn_value[app->selected], burn_limits_default().resistor_max_power);
            else
                snprintf(reading, sizeof(reading), "QUEIMADO: %.4g mA (limite %.3g mA)",
                         app->sim.burn_value[app->selected] * 1000.0, burn_limits_default().led_max_current * 1000.0);
            text(font, 850, 137, reading);
        }
        else if (shows_current)
        {
            snprintf(reading, sizeof(reading), "Corrente: %.5g mA (pino 1 -> 2)",
                     app->sim.dc_led_currents[app->selected] * 1000.0);
            text(font, 850, 137, reading);
        }
    }
}

static void draw_status(const AppState *app, GLuint font)
{
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->show_trace && !app->settings)
        trace_draw(font, &app->sim.trace, app->sim.trace_index, app->sim.trace_pin, app->sim.trace_current);
    color(.97f, .98f, .98f);
    rect(220, 660, 1060, 60);
    color(.2f, .26f, .3f);
    text(font, 238, 685, app->status);
    text(font, 238, 709,
         "Shift+arraste: area | Ctrl+C/X/V: copiar/recortar/colar | Delete: excluir | Ctrl+G: bloco | Ctrl+Z/Y: "
         "historico");
}

static void draw_settings(const AppState *app, GLuint font)
{
    if (app->settings)
    {
        int swatch;
        color(.62f, .68f, .72f);
        rect(846, 90, 400, 558);
        color(.98f, .98f, .99f);
        rect(840, 84, 400, 558);
        color(.14f, .22f, .28f);
        text(font, 860, 112, "CONFIGURACOES DE VISUALIZACAO");
        text(font, 860, 148, app->show_grid ? "[x] Grade" : "[ ] Grade");
        text(font, 860, 188, app->show_handles ? "[x] Pontos de controle dos fios" : "[ ] Pontos de controle dos fios");
        text(font, 860, 228, app->round_wires ? "[x] Curvas arredondadas" : "[ ] Curvas arredondadas");
        text(font, 860, 268,
             app->show_current ? "[x] Visualizar corrente nos fios" : "[ ] Visualizar corrente nos fios");
        text(font, 860, 309, app->selected_wire >= 0 ? "COR DO FIO SELECIONADO" : "COR DO PROXIMO FIO");
        for (swatch = 0; swatch < WIRE_COLOR_COUNT; ++swatch)
        {
            float sx = 860 + swatch * 50;
            color(.2f, .25f, .29f);
            rect(sx - 2, 318, 36, 30);
            color(wire_palette[swatch][0], wire_palette[swatch][1], wire_palette[swatch][2]);
            rect(sx, 320, 32, 26);
            if (swatch == app->wire_color)
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
        text(font, 1170, 340, wire_color_names[app->wire_color]);
        {
            char setting[100];
            text(font, 860, 377, app->probe_current ? "Medida: CORRENTE (clique)" : "Medida: TENSAO (clique)");
            snprintf(setting, sizeof(setting), "Terminal de sondagem: %d (clique)", app->probe_pin + 1);
            text(font, 860, 415, setting);
            snprintf(setting, sizeof(setting), "Passo solicitado: %.6g s (clique)", app->transient_step);
            text(font, 860, 455, setting);
            snprintf(setting, sizeof(setting), "Duracao: %.6g s (clique)", app->transient_stop);
            text(font, 860, 495, setting);
        }
        text(font, 860, 535, "T: transitorio | Espaco: reproduzir | B/N: amostras");
        text(font, 860, 565, "1-100 uA | 100 uA-10 mA | >=10 mA");
        text(font, 860, 590, "Largura e cor indicam faixas de corrente");
        text(font, 860, 625, "F1 fecha este painel");
    }
}

static void draw_value_editor(const AppState *app, GLuint font)
{
    if (app->editing >= 0)
    {
        char reading[110];
        color(.62f, .68f, .72f);
        rect(446, 268, 400, 146);
        color(.98f, .98f, .99f);
        rect(440, 262, 400, 146);
        color(.14f, .22f, .28f);
        text(font, 460, 294, app->editing >= WORKBENCH_MAX_COMPONENTS ? "TEMPO EM SEGUNDOS" : "VALOR EM UNIDADES SI");
        snprintf(reading, sizeof(reading), "> %s_", app->edit_text);
        text(font, 460, 336, reading);
        text(font, 460, 380, "Enter confirma | Esc cancela");
    }
}

static void draw_projects(const AppState *app, GLuint font)
{
    if (!app->projects_open)
        return;
    color(.35f, .4f, .44f);
    rect(306, 136, 680, 438);
    color(.97f, .98f, .99f);
    rect(300, 130, 680, 438);
    color(.12f, .22f, .29f);
    text(font, 330, 165, "SAVE STATES  /  MONTAGEM E CONFIGURACOES");
    text(font, 330, 188, "Escolha um slot. F3 ou Esc fecha.");
    for (int i = 0; i < PROJECT_SLOT_COUNT; ++i)
    {
        float x = 330 + (i % 3) * 200, y = 200 + (i / 3) * 90;
        color(i == app->save_slot ? .76f : .9f, .9f, .96f);
        rect(x, y, 180, 74);
        color(.13f, .23f, .29f);
        char label[64];
        snprintf(label, sizeof(label), "Slot %d  %s", i + 1, app->saved_slots[i] ? "Gravado" : "Vazio");
        text(font, x + 12, y + 30, label);
        text(font, x + 12, y + 56, i == app->save_slot ? "SELECIONADO" : "Clique para escolher");
    }
    color(.76f, .9f, .82f);
    rect(340, 500, 250, 45);
    color(.78f, .88f, .97f);
    rect(640, 500, 250, 45);
    color(.12f, .22f, .29f);
    text(font, 360, 529, "Salvar slot [Ctrl+S]");
    text(font, 660, 529, "Carregar [Ctrl+L]");
}

static void draw_pin_help(const AppState *app, GLuint font)
{
    Pin pin = app->hovered_pin;
    if (app->settings || app->projects_open || app->editing >= 0 || app->selecting_area ||
        app->hover_revision != app->bench.revision || pin.component < 0 || pin.component >= app->bench.part_count)
        return;
    const Part *part = &app->bench.parts[pin.component];
    const ComponentPinInfo *info = component_pin_info(part->type, pin.pin);
    if (!info)
        return;
    float x, y;
    bench_terminal_position(&app->bench, pin, &x, &y);
    bench_view_to_screen(&app->view, x, y, &x, &y);
    float panel_y = y > 480 ? 80 : 560;
    color(.14f, .42f, .8f);
    circle(x, y, fmaxf(9 * app->view.zoom, 6));
    color(.96f, .98f, 1);
    circle(x, y, fmaxf(5 * app->view.zoom, 3));
    color(.78f, .85f, .91f);
    rect(240, panel_y, 588, 84);
    color(.98f, .99f, 1);
    rect(238, panel_y - 2, 588, 84);
    char label[128];
    if (part->group)
        snprintf(label, sizeof(label), "Bloco %d > %s / pino %d (%s)", part->group, part_name(part->type), pin.pin + 1,
                 info->name);
    else
        snprintf(label, sizeof(label), "%s / pino %d (%s)", part_name(part->type), pin.pin + 1, info->name);
    color(.14f, .22f, .28f);
    text(font, 250, panel_y + 21, label);
    pin_role_color(info->role);
    text(font, 250, panel_y + 44, component_pin_role_label(info->role));
    color(.24f, .3f, .35f);
    text(font, 250, panel_y + 68, "Funcao do terminal; nao e uma medicao de tensao.");
}

void app_render_draw(const AppState *app, GLuint font, int width, int height)
{
    draw_chrome(app, font, width, height);
    draw_workspace(app, font, width, height);
    draw_inspector(app, font);
    draw_status(app, font);
    draw_pin_help(app, font);
    draw_settings(app, font);
    draw_value_editor(app, font);
    draw_projects(app, font);
}
