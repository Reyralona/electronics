#include "netlist_devices.h"
#include "netlist.h"
#include "component_devices.h"
#include "component_registry.h"
#include "mcu_board.h"
#include <math.h>
#include <stdio.h>

/* Didactic parameters; the inspector and docs/DECISOES.md state that they are approximations. */
#define GENERATOR_OUTPUT_OHMS 50.0
#define METER_INPUT_OHMS 10e6
#define METER_OHMS_TEST_AMPERES COMPONENT_METER_OHMS_TEST_AMPS
#define METER_TEST_ZENER_VOLTS 2.7
#define SCOPE_INPUT_OHMS 1e6
#define RELAY_COIL_OHMS 100.0
#define RELAY_COIL_HENRY .01
#define MODULE_QUIESCENT_OHMS 5000.0
#define MODULE_OUTPUT_OHMS 100.0
#define PIEZO_FARADS 20e-9

typedef struct
{
    double resistance, inductance, constant, inertia, friction;
} MotorModel;

static const MotorModel dc_motor = {10, 1e-3, .01, 1e-8, 1e-6};
static const MotorModel vibration_motor = {30, 5e-4, .003, 1e-9, 1e-7};

int netlist_device_supported(PartType type)
{
    return component_device(type) != NULL;
}

int netlist_device_rebuilds_on_change(PartType type)
{
    return type == PART_FUNCTION_GENERATOR || type == PART_LED;
}

static int write_motor(FILE *out, int i, const int *n, const MotorModel *m)
{
    return fprintf(out,
                   "Vmot%d %d mot_a%d 0\nRmot%d mot_a%d mot_b%d %.17g\nLmot%d mot_b%d mot_c%d %.17g\n"
                   "Bmot_emf%d mot_c%d %d V=%.17g*V(mspd%d)\nBmot_trq%d 0 mspd%d I=%.17g*i(vmot%d)\n"
                   "Cmot%d mspd%d 0 %.17g IC=0\nRmot_f%d mspd%d 0 %.17g\n",
                   i, n[0], i, i, i, i, m->resistance, i, i, i, m->inductance, i, i, n[1], m->constant, i, i, i,
                   m->constant, i, i, i, m->inertia, i, i, 1 / m->friction) >= 0;
}

/* Quadrature outputs from the integrated shaft angle, 12 pulses per turn, powered by VCC-GND. */
static int write_encoder(FILE *out, int i, const int *n)
{
    const int pulses = 12;
    if (!write_motor(out, i, n, &dc_motor) ||
        fprintf(out, "Benc_th%d 0 mth%d I=V(mspd%d)\nCenc%d mth%d 0 1 IC=0\nRenc_th%d mth%d 0 1e12\n", i, i, i, i, i,
                i, i) < 0)
        return 0;
    for (int channel = 0; channel < 2; ++channel)
        if (fprintf(out, "Benc_%c%d enc_%c%d %d V=max(V(%d,%d),0)*0.5*(1+tanh(8*%s(%d*V(mth%d))))\nRenc_%c%d enc_%c%d %d 1000\n",
                    'a' + channel, i, 'a' + channel, i, n[3], n[2], n[3], channel ? "cos" : "sin", pulses, i,
                    'a' + channel, i, 'a' + channel, i, n[4 + channel]) < 0)
            return 0;
    return 1;
}

/* Position follows the average signal level relative to V+ (0..180 degrees); not pulse decoding. */
static int write_servo(FILE *out, int i, const int *n)
{
    return fprintf(out,
                   "Rsrv%d %d %d 1000\nRsrv_in%d %d savg%d 10000\nCsrv%d savg%d %d 20n IC=0\n"
                   "Bsrv_t%d starg%d 0 V=180*min(max(V(savg%d,%d)/max(V(%d,%d),0.1),0),1)*min(max((V(%d,%d)-3)/0.5,0),1)\n"
                   "Rsrv_m%d starg%d sang%d 1000\nCsrv_m%d sang%d 0 300n IC=0\n",
                   i, n[1], n[0], i, n[2], i, i, i, n[0], i, i, i, n[0], n[1], n[0], n[1], n[0], i, i, i, i, i) >= 0;
}

static int write_relay(FILE *out, int i, const int *n)
{
    return fprintf(out,
                   "Vrly%d %d rly_a%d 0\nRrly%d rly_a%d rly_b%d %.17g\nLrly%d rly_b%d %d %.17g\n"
                   "Brly%d rly_on%d 0 V=min(max((abs(i(vrly%d))-0.015)/0.01,0),1)\nBrlyn%d rly_off%d 0 V=1-V(rly_on%d)\n"
                   "Srly_no%d %d %d rly_on%d 0 relay_contact\nSrly_nc%d %d %d rly_off%d 0 relay_contact\n",
                   i, n[0], i, i, i, i, RELAY_COIL_OHMS, i, i, n[1], RELAY_COIL_HENRY, i, i, i, i, i, i, i, n[2], n[3],
                   i, i, n[2], n[4], i) >= 0;
}

static int write_generator(FILE *out, const Part *part, int i, const int *n)
{
    double period = 1 / part->value, edge = period * 1e-3;
    char wave[160];
    if (part->states == 1)
        snprintf(wave, sizeof(wave), "PULSE(0 5 0 %.17g %.17g %.17g %.17g)", edge, edge, period / 2 - edge, period);
    else if (part->states == 2)
        snprintf(wave, sizeof(wave), "PULSE(0 5 0 %.17g %.17g %.17g %.17g)", period / 2, period / 2, edge, period);
    else
        snprintf(wave, sizeof(wave), "SIN(2.5 2.5 %.17g)", part->value);
    return fprintf(out, "Vgen%d gen%d %d %s\nRgen%d gen%d %d %.17g\n", i, i, n[1], wave, i, i, n[0],
                   part->on ? GENERATOR_OUTPUT_OHMS : NETLIST_LIVE_OPEN_OHMS) >= 0;
}

static const char *module_transfer(PartType type)
{
    switch (type)
    {
    case PART_PIR:
        return "min(max((V(env%d)-45)/10,0),1)";
    case PART_GAS:
        return "(0.1+0.8*min(max(V(env%d)/1000,0),1))";
    case PART_IR_OBSTACLE:
        return "min(max((V(env%d)-9)/2,0),1)";
    default:
        return "min(max(27/max(V(env%d),1)/5,0.08),0.62)";
    }
}

/* Powered module: output = (V+ - GND) * transfer(stimulus) through 100 ohm; 5 kOhm quiescent load. */
static int write_module(FILE *out, const Part *part, int i, const int *n)
{
    char transfer[96];
    snprintf(transfer, sizeof(transfer), module_transfer(part->type), i);
    return fprintf(out, "Venv%d env%d 0 %.17g\nRsq%d %d %d %.17g\nBsen%d sen%d %d V=max(V(%d,%d),0)*%s\nRso%d sen%d %d %.17g\n",
                   i, i, component_environment_value(part), i, n[0], n[2], MODULE_QUIESCENT_OHMS, i, i, n[2], n[0],
                   n[2], transfer, i, i, n[1], MODULE_OUTPUT_OHMS) >= 0;
}

static int write_seven_segment(FILE *out, int i, const int *n)
{
    static const int segment_pins[COMPONENT_SEGMENTS] = {0, 1, 2, 3, 5, 6, 7, 8};
    const int common = 4;
    for (int segment = 0; segment < COMPONENT_SEGMENTS; ++segment)
        if (fprintf(out, "D%d_s%d %d sg%d_%d LED_RED\nRguard%d_s%d sg%d_%d %d %.17g\n", i, segment,
                    n[segment_pins[segment]], i, segment, i, segment, i, segment, n[common],
                    NETLIST_LIVE_CLOSED_OHMS) < 0)
            return 0;
    return 1;
}

/* Each color behind its own guard resistor, which a burn opens (like the single LED's Rguard). */
static int write_rgb(FILE *out, int i, const int *n)
{
    static const char colors[] = "rgb";
    static const char *const models[] = {"LED_RED", "LED_GREEN", "LED_BLUE"};
    static const int anodes[] = {0, 2, 3};
    const int common = 1;
    for (int color = 0; color < 3; ++color)
        if (fprintf(out, "D%d_%c %d rg%d_%c %s\nRguard%d_%c rg%d_%c %d %.17g\n", i, colors[color], n[anodes[color]], i,
                    colors[color], models[color], i, colors[color], i, colors[color], n[common],
                    NETLIST_LIVE_CLOSED_OHMS) < 0)
            return 0;
    return 1;
}

static int write_contacts(FILE *out, const Part *part, int i, const int *n)
{
    char name[48];
    double value;
    for (int k = 0; k < netlist_device_control_count(part); ++k)
    {
        netlist_device_control(part, i, k, name, sizeof(name), &value);
        int a = part->type == PART_DIP_SWITCH ? n[k] : n[2 * k], b = part->type == PART_DIP_SWITCH ? n[k + 4] : n[2 * k + 1];
        if (fprintf(out, "%s %d %d %.17g\n", name, a, b, value) < 0)
            return 0;
    }
    return 1;
}

/* Board: optional on-board regulator(s); each IO is a level source scaled by the supply, a 25 ohm
 * output resistance (open when input), an optional 30 kOhm pull-up and a voltage sense. */
static int write_regulator(FILE *out, int i, const char *tag, int terminal_node, int ground, double volts)
{
    return fprintf(out, "Vmcu%d_%s mcu%d_%s %d DC %.17g\nRmcu%d_%s mcu%d_%s %d 0.05\n", i, tag, i, tag, ground, volts, i,
                   tag, i, tag, terminal_node) >= 0;
}

static int write_board(FILE *out, const Part *part, int i, const int *n)
{
    const McuBoard *board = mcu_board(part->type);
    int ground = n[board->ground_terminal], supply = n[board->supply_terminal];
    if (board->supply_volts > 0 && !write_regulator(out, i, "reg", supply, ground, board->supply_volts))
        return 0;
    /* Sense of the supply a VCC-fed chip receives, read by the datasheet check. */
    if (board->supply_volts <= 0 && fprintf(out, "Bmcu%d_vcc mcu%d_vcc 0 V=V(%d,%d)\n", i, i, supply, ground) < 0)
        return 0;
    if (board->auxiliary_terminal >= 0 &&
        !write_regulator(out, i, "aux", n[board->auxiliary_terminal], ground, board->auxiliary_volts))
        return 0;
    for (int k = 0; k < board->io_count; ++k)
    {
        int pin = n[board->io_terminal[k]];
        char name[48];
        double values[3];
        for (int control = 0; control < 3; ++control)
            netlist_device_control(part, i, k * 3 + control, name, sizeof(name), &values[control]);
        if (fprintf(out,
                    "Vmcu%d_l%d mcu%d_l%d 0 DC %.17g\nBmcu%d_d%d mcu%d_o%d %d V=V(mcu%d_l%d)*max(V(%d,%d),0)\n"
                    "Rmcu%d_o%d mcu%d_o%d %d %.17g\nRmcu%d_u%d %d %d %.17g\nBmcu%d_s%d mcu%d_in%d 0 V=V(%d)-V(%d)\n",
                    i, k, i, k, values[0], i, k, i, k, ground, i, k, supply, ground, i, k, i, k, pin, values[1], i, k,
                    supply, pin, values[2], i, k, i, k, pin, ground) < 0)
            return 0;
    }
    return 1;
}

static int write_lcd(FILE *out, int i, const int *n)
{
    const int gnd = 0, vcc = 1, sda = 2, scl = 3;
    return fprintf(out, "Rlcd%d %d %d 1000\nBlcd%d lcd%d_pwr 0 V=V(%d)-V(%d)\nRlcd_sda%d %d %d 10000\nRlcd_scl%d %d %d 10000\n",
                   i, n[vcc], n[gnd], i, i, n[vcc], n[gnd], i, n[sda], n[vcc], i, n[scl], n[vcc]) >= 0;
}

int netlist_device_write(FILE *out, const Part *part, int i, const int *n)
{
    switch (part->type)
    {
    case PART_RGB_LED:
        return write_rgb(out, i, n);
    case PART_SEVEN_SEGMENT:
        return write_seven_segment(out, i, n);
    case PART_DIP_SWITCH:
    case PART_DPST:
        return write_contacts(out, part, i, n);
    case PART_RELAY:
        return write_relay(out, i, n);
    case PART_DC_MOTOR:
        return write_motor(out, i, n, &dc_motor);
    case PART_VIBRATION_MOTOR:
        return write_motor(out, i, n, &vibration_motor);
    case PART_ENCODER_MOTOR:
        return write_encoder(out, i, n);
    case PART_SERVO:
        return write_servo(out, i, n);
    case PART_PIEZO:
        return fprintf(out, "Vpz%d %d pz%d 0\nCpz%d pz%d %d %.17g IC=0\nRpz%d pz%d %d 1e6\n", i, n[0], i, i, i, n[1],
                       PIEZO_FARADS, i, i, n[1]) >= 0;
    case PART_FUNCTION_GENERATOR:
        return write_generator(out, part, i, n);
    case PART_MULTIMETER:
        return fprintf(out, "Rmm%d %d %d %.17g\nBmm%d mmv%d 0 V=V(%d)-V(%d)\nVamm%d %d %d 0\n"
                            "Imm%d %d ohm_test%d DC %.17g\nDohm%d ohm_test%d %d METER_TEST_DIODE\n"
                            "Dzohm%d %d ohm_test%d METER_TEST_ZENER\n",
                       i, n[1], n[0], METER_INPUT_OHMS, i, i, n[1], n[0], i, n[2], n[0],
                       i, n[0], i, part->states == 2 ? METER_OHMS_TEST_AMPERES : 0.0,
                       i, i, n[1], i, n[0], i) >= 0;
    case PART_OSCILLOSCOPE:
        return fprintf(out, "Rosc%d %d %d %.17g\nBosc%d oscv%d 0 V=V(%d)-V(%d)\n", i, n[0], n[1], SCOPE_INPUT_OHMS, i, i,
                       n[0], n[1]) >= 0;
    case PART_ARDUINO_UNO:
    case PART_ESP32:
    case PART_ATTINY85:
        return write_board(out, part, i, n);
    case PART_LCD_I2C:
        return write_lcd(out, i, n);
    case PART_PIR:
    case PART_GAS:
    case PART_IR_OBSTACLE:
    case PART_DISTANCE:
        return write_module(out, part, i, n);
    default:
        return 1;
    }
}

int netlist_device_models(FILE *out)
{
    /* Same diode law as LED_GENERIC, with saturation currents placing 10 mA near 1.9/2.2/2.9 V. */
    return fprintf(out, ".model LED_RED D(IS=1e-18 N=2 RS=2)\n.model LED_GREEN D(IS=3.6e-21 N=2 RS=2)\n"
                        ".model LED_YELLOW D(IS=6.4e-20 N=2 RS=2)\n"
                        ".model LED_BLUE D(IS=4.7e-27 N=2 RS=2)\n"
                        ".model METER_TEST_DIODE D(IS=1e-14 N=1.5)\n"
                        ".model METER_TEST_ZENER D(IS=1e-14 N=1.5 BV=%.17g IBV=%.17g)\n"
                        ".model relay_contact SW(Ron=.05 Roff=1e10 Vt=.5 Vh=.2)\n",
                   METER_TEST_ZENER_VOLTS, METER_OHMS_TEST_AMPERES) >= 0;
}

int netlist_device_control_count(const Part *part)
{
    const McuBoard *board = mcu_board(part->type);
    if (board)
        return board->io_count * 3;
    switch (part->type)
    {
    case PART_DIP_SWITCH:
        return COMPONENT_DIP_POSITIONS;
    case PART_DPST:
        return 2;
    case PART_FUNCTION_GENERATOR:
    case PART_MULTIMETER:
    case PART_PIR:
    case PART_GAS:
    case PART_IR_OBSTACLE:
    case PART_DISTANCE:
        return 1;
    default:
        return 0;
    }
}

int netlist_device_control(const Part *part, int index, int control, char *name, size_t capacity, double *value)
{
    if (control < 0 || control >= netlist_device_control_count(part))
        return 0;
    if (mcu_board(part->type))
    {
        int io = control / 3, kind = control % 3, mode = part->io_mode[io];
        static const char *tags[] = {"Vmcu%d_l%d", "Rmcu%d_o%d", "Rmcu%d_u%d"};
        snprintf(name, capacity, tags[kind], index, io);
        if (kind == 0)
            *value = mode == MCU_OUTPUT ? fmin(1, fmax(0, part->io_level[io])) : 0;
        else if (kind == 1)
            *value = mode == MCU_OUTPUT ? MCU_OUTPUT_OHMS : NETLIST_LIVE_OPEN_OHMS;
        else
            *value = mode == MCU_INPUT_PULLUP ? MCU_PULLUP_OHMS : NETLIST_LIVE_OPEN_OHMS;
        return 1;
    }
    switch (part->type)
    {
    case PART_DIP_SWITCH:
        snprintf(name, capacity, "Rdip%d_%d", index, control);
        *value = (part->states >> control & 1u) ? NETLIST_LIVE_CLOSED_OHMS : NETLIST_LIVE_OPEN_OHMS;
        return 1;
    case PART_DPST:
        snprintf(name, capacity, "Rdpst%d_%c", index, 'a' + control);
        *value = part->on ? NETLIST_LIVE_CLOSED_OHMS : NETLIST_LIVE_OPEN_OHMS;
        return 1;
    case PART_FUNCTION_GENERATOR:
        snprintf(name, capacity, "Rgen%d", index);
        *value = part->on ? GENERATOR_OUTPUT_OHMS : NETLIST_LIVE_OPEN_OHMS;
        return 1;
    case PART_MULTIMETER:
        snprintf(name, capacity, "Imm%d", index);
        *value = part->states == 2 ? METER_OHMS_TEST_AMPERES : 0.0;
        return 1;
    default:
        snprintf(name, capacity, "Venv%d", index);
        *value = component_environment_value(part);
        return 1;
    }
}
