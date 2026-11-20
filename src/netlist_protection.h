#ifndef NETLIST_PROTECTION_H
#define NETLIST_PROTECTION_H
#include <stdio.h>
/* Datasheet limits of logic outputs and supplies. The netlist writer lists every push-pull output (current
 * through the on switch of ~LOGIC_OUTPUT_OHMS) and every supply; the live session reads their nodes in
 * batches, a few outputs per step, so the solver itself carries no extra equations. Pure data. */
#define LOGIC_OUTPUT_OHMS 50.0
/* An output switch counts as on only above its turn-on control (SW Vt .5 + Vh .1): an unknown logic
 * level drives both controls to .5, and both switches then keep their previous state. */
#define LOGIC_SWITCH_ON_CONTROL .6
/* 74HC family absolute maximum (TI SN74HC04, consulted 20/11/2026): VCC 7 V, 25 mA per output;
 * recommended VCC up to 6 V. Reverse supply beyond -0.5 V is also outside the ratings. */
#define HC_SUPPLY_ABSOLUTE_MAX 7.0
#define HC_SUPPLY_RECOMMENDED_MAX 6.0
#define HC_OUTPUT_ABSOLUTE_MAX_AMPS .025
#define LOGIC_REVERSE_ABSOLUTE_MAX .5
/* Chosen policy (20/11/2026): above the absolute maximum the part warns; near twice it (a hard short or bus
 * contention) it burns. 1.8 and not 2: two opposed outputs at 5 V pass exactly 2x (5 V / 100 ohm) and must
 * burn. A supply above its absolute maximum burns at once. */
#define PROTECTION_BURN_CURRENT_RATIO 1.8
#define PROTECTION_NAME_CAPACITY 24

typedef struct
{
    int part, pin;
    char vcc[PROTECTION_NAME_CAPACITY], gnd[PROTECTION_NAME_CAPACITY], node[PROTECTION_NAME_CAPACITY]; /* SPICE nodes */
    char high[PROTECTION_NAME_CAPACITY];                         /* control of the switch to VCC, 0..1 */
    char low_plus[PROTECTION_NAME_CAPACITY], low_minus[PROTECTION_NAME_CAPACITY]; /* switch to GND */
    double limit_amps;
    double measured_amps; /* last reading in the live session */
} ProtectionCurrent;

typedef struct
{
    int part; /* -1: the shared supply of the loose gate symbols */
    char supply[PROTECTION_NAME_CAPACITY];
    double absolute_max, recommended_max;
    double measured_volts; /* last reading in the live session */
} ProtectionVoltage;

typedef struct
{
    ProtectionCurrent *current;
    ProtectionVoltage *voltage;
    int current_count, current_capacity, voltage_count, voltage_capacity;
    int failed;
} NetlistProtection;

void netlist_protection_reset(NetlistProtection *protection);
void netlist_protection_free(NetlistProtection *protection);
void netlist_protection_add_current(NetlistProtection *protection, ProtectionCurrent term);
/* One push-pull output; nodes are SPICE node numbers (0 = ground). */
void netlist_protection_output(NetlistProtection *protection, int part, int pin, int vcc, int gnd, int node,
                               const char *high, const char *low_plus, const char *low_minus, double limit_amps);
void netlist_protection_supply(NetlistProtection *protection, int part, const char *supply, double absolute_max,
                               double recommended_max);
void netlist_protection_add_voltage(NetlistProtection *protection, ProtectionVoltage term);
#endif
