#include "netlist_linear.h"
#include "netlist.h"
#include <stdarg.h>
#include <string.h>

#define LINEAR_PREFIX_CAPACITY 16
/* Smooth comparator: full swing over 1 mV keeps Newton steps continuous. */
#define COMPARATOR_GAIN 1000.0
#define OP_AMP_GAIN 1e5
#define OP_AMP_STAGE_VOLTS 100.0
#define OP_AMP_SMOOTH 1e-3
#define OP_AMP_POLE_OHMS 1000.0
#define OP_AMP_POLE_FARADS 1e-9
/* NE555 (TI SLFS022K): three 5 kOhm divider resistors, output high about 1.7 V under VCC, low 0.1 V,
 * reset active below 0.7 V, discharge on while the output is low. */
#define NE555_DIVIDER_OHMS 5000.0
#define NE555_HIGH_DROP 1.7
#define NE555_LOW_LEVEL 0.1
#define NE555_RESET_VOLTS 0.7
#define NE555_OUTPUT_OHMS 5.0
/* LM358 (TI SLOS068AB): output swings to V- and to 1.5 V under V+. */
#define LM358_HIGH_DROP 1.5
#define LM358_OUTPUT_OHMS 25.0
/* LM7805/LM340 (TI): 5 V, 2 V dropout, about 2.1 A short-circuit current; sources current only. */
#define LM7805_VOLTS 5.0
#define LM7805_DROPOUT 2.0
#define LM7805_LIMIT_AMPS 2.1
/* A softer current-limit slope converges while a bridge capacitor charges from zero. */
#define LM7805_LIMIT_GAIN 100.0
#define LM7805_QUIESCENT_OHMS 2000.0
/* ULN2003A (TI SLRS027T): 2.7 kOhm base resistor; a channel turns on between about 2 and 2.4 V. */
#define ULN2003_BASE_OHMS 2700.0
#define ULN2003_ON_START 1.6
#define ULN2003_ON_SPAN 0.8
/* L293D (TI): inputs high above 2.3 V and low below 1.5 V. */
#define L293D_INPUT_THRESHOLD 1.9

typedef struct
{
    FILE *out;
    const int *nodes;
    int vcc, gnd;
    int ok;
    char p[LINEAR_PREFIX_CAPACITY];
} LinearWriter;

static void emit(LinearWriter *w, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (w->ok && vfprintf(w->out, format, args) < 0)
        w->ok = 0;
    va_end(args);
}

/* sup = VCC - GND, alive (0 when burnt) and ok: off below the datasheet minimum supply. */
static void supply(LinearWriter *w, double minimum)
{
    const char *p = w->p;
    emit(w, "B%ssup %ssup 0 V=V(%d,%d)\n", p, p, w->vcc, w->gnd);
    emit(w, "V%salive %salive 0 1\n", p, p);
    emit(w, "B%sok %sok 0 V=V(%salive)*min(max((V(%ssup)-%g)/.1,0),1)\n", p, p, p, p, minimum - .1);
}

/* NE555: comparators into a set/reset latch; RESET over TRIG over THRES (datasheet priority). */
static void timer_555(LinearWriter *w)
{
    const char *p = w->p;
    const int *n = w->nodes;
    const int trig = n[1], out = n[2], reset = n[3], cont = n[4], thres = n[5], disch = n[6];
    supply(w, 4.5);
    emit(w, "R%sd1 %d %d %g\nR%sd2 %d %smid %g\nR%sd3 %smid %d %g\n", p, w->vcc, cont, NE555_DIVIDER_OHMS, p, cont, p,
         NE555_DIVIDER_OHMS, p, p, w->gnd, NE555_DIVIDER_OHMS);
    emit(w, "B%sct %sct 0 V=5*min(max((V(%smid)-V(%d))*%g+.5,0),1)\n", p, p, p, trig, COMPARATOR_GAIN);
    emit(w, "B%sch %sch 0 V=5*min(max((V(%d)-V(%d))*%g+.5,0),1)\n", p, p, thres, cont, COMPARATOR_GAIN);
    emit(w, "B%scr %scr 0 V=5*min(max((%g-V(%d,%d))*%g+.5,0),1)\n", p, p, NE555_RESET_VOLTS, reset, w->gnd,
         COMPARATOR_GAIN);
    emit(w, "A%sadc [%sct %sch %scr] [%sdt %sdh %sdr] lin_adc\n", p, p, p, p, p, p, p);
    emit(w, "A%sset [%sdt ~%sdr] %sset lin_and\nA%shr [%sdh ~%sdt] %shr lin_and\n", p, p, p, p, p, p, p, p);
    emit(w, "A%srst [%shr %sdr] %srst lin_or\nA%szero %szero lin_zero\n", p, p, p, p, p, p);
    emit(w, "A%sff %sq %szero %sset %srst %sq null lin_dff\n", p, p, p, p, p, p);
    emit(w, "A%sdac [%sq] [%sqa] lin_dac\n", p, p, p);
    /* Debit sourced load current from VCC; sinking current returns to GND. */
    emit(w, "V%ssense %so %sload 0\nB%sload %d %d I=max(I(V%ssense),0)\n", p, p, p, p, w->vcc, w->gnd, p);
    emit(w, "B%so %so 0 V=V(%d)+V(%sok)*(V(%sqa)*max(V(%ssup)-%g,0)+(1-V(%sqa))*%g)\nR%sout %sload %d %g\n", p, p, w->gnd,
         p, p, p, NE555_HIGH_DROP, p, NE555_LOW_LEVEL, p, p, out, NE555_OUTPUT_OHMS);
    emit(w, "B%sdc %sdc 0 V=V(%sok)*(1-V(%sqa))\nS%sdis %d %d %sdc 0 lin_disch\n", p, p, p, p, p, disch, w->gnd, p);
}

/* LM358: two op-amps with gain 1e5, output clamped to V- .. V+ - 1.5 V. The clamp corners are rounded
 * (SMOOTH volts) and the gain stage saturates softly at +-100 V: hard min/max with this gain did not
 * converge from the zero start of the live session. */
static void op_amp_358(LinearWriter *w)
{
    static const int outputs[] = {0, 6}, inverting[] = {1, 5}, non_inverting[] = {2, 4};
    const char *p = w->p;
    supply(w, 3);
    for (int amp = 0; amp < 2; ++amp)
    {
        emit(w, "B%sx%d %sx%d 0 V=%g*tanh(%g*(V(%d)-V(%d))/%g)\n", p, amp, p, amp, OP_AMP_STAGE_VOLTS, OP_AMP_GAIN,
             w->nodes[non_inverting[amp]], w->nodes[inverting[amp]], OP_AMP_STAGE_VOLTS);
        /* 1 us pole in the gain stage: breaks the algebraic feedback loop for the solver's zero start and
         * is invisible at the bench's millisecond scale (no GBW or slew-rate model). */
        emit(w, "R%sxr%d %sx%d %sxf%d %g\nC%sxc%d %sxf%d 0 %g\n", p, amp, p, amp, p, amp, OP_AMP_POLE_OHMS, p, amp, p,
             amp, OP_AMP_POLE_FARADS);
        emit(w, "B%sy%d %sy%d 0 V=.5*(V(%sxf%d)+sqrt(V(%sxf%d)^2+%g))\n", p, amp, p, amp, p, amp, p, amp,
             OP_AMP_SMOOTH * OP_AMP_SMOOTH);
        emit(w, "B%sh%d %sh%d 0 V=max(V(%ssup)-%g,0)\n", p, amp, p, amp, p, LM358_HIGH_DROP);
        emit(w, "B%sa%d %sa%d 0 V=V(%d)+V(%sok)*.5*(V(%sy%d)+V(%sh%d)-sqrt((V(%sy%d)-V(%sh%d))^2+%g))\n", p, amp, p, amp,
             w->gnd, p, p, amp, p, amp, p, amp, p, amp, OP_AMP_SMOOTH * OP_AMP_SMOOTH);
        emit(w, "V%ssense%d %sa%d %sload%d 0\nB%sload%d %d %d I=max(I(V%ssense%d),0)\n",
             p, amp, p, amp, p, amp, p, amp, w->vcc, w->gnd, p, amp);
        emit(w, "R%sa%d %sload%d %d %g\n", p, amp, p, amp, w->nodes[outputs[amp]], LM358_OUTPUT_OHMS);
    }
}

/* LM393: two comparators; the open-collector output sinks when IN+ < IN-. */
static void comparator_393(LinearWriter *w)
{
    static const int outputs[] = {0, 6}, inverting[] = {1, 5}, non_inverting[] = {2, 4};
    const char *p = w->p;
    supply(w, 2);
    for (int unit = 0; unit < 2; ++unit)
        emit(w, "B%sc%d %sc%d 0 V=V(%sok)*min(max((V(%d)-V(%d))*%g+.5,0),1)\nS%sc%d %d %d %sc%d 0 lin_cmp\n", p, unit, p,
             unit, p, w->nodes[inverting[unit]], w->nodes[non_inverting[unit]], COMPARATOR_GAIN, p, unit,
             w->nodes[outputs[unit]], w->gnd, p, unit);
}

/* LM7805: OUT follows min(5 V, IN - 2 V) through a current sense; a series drop (the bench supply's CV/CC
 * form) holds the current between 0 and 2.1 A, so it never sinks. IN supplies the same current. */
static void regulator_7805(LinearWriter *w)
{
    const char *p = w->p;
    const int in = w->nodes[0], out = w->nodes[2];
    supply(w, 0);
    emit(w, "R%sq %d %d %g\n", p, in, w->gnd, LM7805_QUIESCENT_OHMS);
    emit(w, "B%st %st 0 V=V(%d)+max(min(%g,V(%d,%d)-%g),0)\nV%ss %st %ss 0\n", p, p, w->gnd, LM7805_VOLTS, in, w->gnd,
         LM7805_DROPOUT, p, p, p);
    emit(w, "B%sd %d %ss V=-%g*.5*((I(V%ss)-%g*V(%salive))+sqrt((I(V%ss)-%g*V(%salive))^2+%g)+(-I(V%ss))+sqrt(I(V%ss)^2+%g))\n",
         p, out, p, LM7805_LIMIT_GAIN, p, LM7805_LIMIT_AMPS, p, p, LM7805_LIMIT_AMPS, p,
         NETLIST_SOURCE_LIMIT_SMOOTH * NETLIST_SOURCE_LIMIT_SMOOTH, p, p, NETLIST_SOURCE_LIMIT_SMOOTH * NETLIST_SOURCE_LIMIT_SMOOTH);
    emit(w, "F%sin %d %d V%ss 1\n", p, in, w->gnd, p);
}

/* ULN2003A: per channel a 2.7 kOhm base resistor and base diodes, a saturating sink to E and a clamp
 * diode from the collector to COM. */
static void darlington_2003(LinearWriter *w)
{
    const char *p = w->p;
    const int *n = w->nodes;
    const int emitter = n[7], common = n[8];
    emit(w, "V%salive %salive 0 1\n", p, p);
    for (int k = 0; k < 7; ++k)
    {
        int base = n[k], collector = n[15 - k];
        emit(w, "R%sb%d %d %sbb%d %g\nD%sbe%d %sbb%d %d lin_be\n", p, k, base, p, k, ULN2003_BASE_OHMS, p, k, p, k,
             emitter);
        emit(w, "B%sk%d %sk%d 0 V=V(%salive)*min(max((V(%d,%d)-%g)/%g,0),1)\n", p, k, p, k, p, base, emitter,
             ULN2003_ON_START, ULN2003_ON_SPAN);
        emit(w, "S%ss%d %d %sx%d %sk%d 0 lin_sink\nD%ssat%d %sx%d %d lin_sat\nD%sfw%d %d %d lin_clamp\n", p, k,
             collector, p, k, p, k, p, k, p, k, emitter, p, k, collector, common);
    }
}

/* L293D: four half-H drivers from VCC2 (about 1.4 V drop high, 1.2 V low), off when their enable is low,
 * with clamp diodes to VCC2 and GND. */
static void driver_293(LinearWriter *w)
{
    static const int inputs[] = {1, 6, 9, 14}, outputs[] = {2, 5, 10, 13}, enables[] = {0, 0, 8, 8};
    const char *p = w->p;
    const int motor = w->nodes[7];
    supply(w, 4.5);
    for (int k = 0; k < 4; ++k)
    {
        int a = w->nodes[inputs[k]], y = w->nodes[outputs[k]], en = w->nodes[enables[k]];
        emit(w, "B%se%d %se%d 0 V=V(%sok)*min(max((V(%d,%d)-%g)*20+.5,0),1)\n", p, k, p, k, p, en, w->gnd,
             L293D_INPUT_THRESHOLD);
        emit(w, "B%si%d %si%d 0 V=min(max((V(%d,%d)-%g)*20+.5,0),1)\n", p, k, p, k, a, w->gnd, L293D_INPUT_THRESHOLD);
        emit(w, "B%sh%d %sh%d 0 V=V(%se%d)*V(%si%d)\nB%sl%d %sl%d 0 V=V(%se%d)*(1-V(%si%d))\n", p, k, p, k, p, k, p, k,
             p, k, p, k, p, k, p, k);
        emit(w, "S%sph%d %d %sxh%d %sh%d 0 lin_drive\nD%sdh%d %sxh%d %d lin_high_drop\n", p, k, motor, p, k, p, k, p, k,
             p, k, y);
        emit(w, "S%spl%d %sxl%d %d %sl%d 0 lin_drive\nD%sdl%d %d %sxl%d lin_low_drop\n", p, k, p, k, w->gnd, p, k, p, k,
             y, p, k);
        emit(w, "D%skh%d %d %d lin_clamp\nD%skl%d %d %d lin_clamp\n", p, k, y, motor, p, k, w->gnd, y);
    }
}

typedef struct
{
    PartType type;
    int vcc, gnd; /* supply pins (0-based); vcc -1: no supply pin */
    double absolute_max, recommended_max;
    void (*write)(LinearWriter *w);
} LinearChip;

/* Supply ratings from the TI datasheets (consulted 20/11/2026). */
static const LinearChip chips[] = {
    {PART_NE555, 7, 0, 18, 16, timer_555},          {PART_LM358, 7, 3, 32, 30, op_amp_358},
    {PART_LM393, 7, 3, 36, 30, comparator_393},     {PART_LM7805, 0, 1, 35, 35, regulator_7805},
    {PART_ULN2003, -1, 7, 0, 0, darlington_2003},   {PART_L293D, 15, 3, 36, 7, driver_293},
};

static const LinearChip *linear_chip(PartType type)
{
    for (size_t i = 0; i < sizeof(chips) / sizeof(chips[0]); ++i)
        if (chips[i].type == type)
            return &chips[i];
    return NULL;
}

int netlist_linear_supported(PartType type)
{
    return linear_chip(type) != NULL;
}

int netlist_linear_ratings(PartType type, double *supply_absolute_max, double *supply_recommended_max)
{
    const LinearChip *chip = linear_chip(type);
    if (!chip)
        return 0;
    *supply_absolute_max = chip->absolute_max;
    *supply_recommended_max = chip->recommended_max;
    return 1;
}

int netlist_linear_write(FILE *out, const Part *part, int index, const int *nodes, NetlistProtection *protection)
{
    const LinearChip *chip = part ? linear_chip(part->type) : NULL;
    if (!out || !chip || !nodes)
        return 0;
    LinearWriter w = {out, nodes, chip->vcc >= 0 ? nodes[chip->vcc] : 0, nodes[chip->gnd], 1, {0}};
    snprintf(w.p, sizeof(w.p), "l%d_", index);
    if (chip->vcc >= 0)
    {
        char supply_node[PROTECTION_NAME_CAPACITY];
        snprintf(supply_node, sizeof(supply_node), "%ssup", w.p);
        netlist_protection_supply(protection, index, supply_node, chip->absolute_max, chip->recommended_max);
    }
    chip->write(&w);
    return w.ok;
}

int netlist_linear_models(FILE *out)
{
    return fputs(".model lin_adc adc_bridge(in_low=1.5 in_high=3.5)\n"
                 ".model lin_dac dac_bridge(out_low=0 out_high=1 out_undef=.5 t_rise=1n t_fall=1n)\n"
                 ".model lin_and d_and(rise_delay=1n fall_delay=1n)\n"
                 ".model lin_or d_or(rise_delay=1n fall_delay=1n)\n"
                 ".model lin_dff d_dff(ic=0 clk_delay=1n set_delay=1n reset_delay=1n rise_delay=1n fall_delay=1n)\n"
                 ".model lin_zero d_pulldown\n"
                 ".model lin_disch SW(Ron=15 Roff=1e10 Vt=.5 Vh=.1)\n"
                 ".model lin_cmp SW(Ron=50 Roff=1e10 Vt=.5 Vh=.1)\n"
                 ".model lin_sink SW(Ron=.1 Roff=1e10 Vt=.5 Vh=.1)\n"
                 ".model lin_drive SW(Ron=.1 Roff=1e10 Vt=.5 Vh=.1)\n"
                 ".model lin_be D(IS=1e-14 N=2)\n"
                 ".model lin_sat D(IS=1e-14 N=1.2 RS=1)\n"
                 ".model lin_high_drop D(IS=1e-12 N=2 RS=.1)\n"
                 ".model lin_low_drop D(IS=1e-12 N=1.7 RS=.1)\n"
                 ".model lin_clamp D(IS=1e-14 N=1.5 RS=.5)\n",
                 out) != EOF;
}
