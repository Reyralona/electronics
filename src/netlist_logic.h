#ifndef NETLIST_LOGIC_H
#define NETLIST_LOGIC_H
#include <stdio.h>
#include "workbench.h"
#include "netlist_protection.h"
/* SPICE text of medium-scale logic ICs (counters, registers, adder, bus transceiver, EEPROM).
 * The inside of each chip is XSPICE digital primitives; only its pins cross into the analog circuit:
 * inputs through adc bridges, outputs through two switches to the chip's own VCC/GND pins, opened
 * together when a 3-state output is disabled. Pure text generation. */
int netlist_logic_supported(PartType type);
/* An EEPROM whose text cannot be read is invalid (see rom_image.h); the other chips always are. */
int netlist_logic_valid(const Part *part);
/* nodes: SPICE node of each terminal (0 = ground). Returns 0 on I/O failure or an unreadable EEPROM text. */
/* protection (may be NULL) receives the datasheet stress terms of the chip's outputs and supply. */
int netlist_logic_write(FILE *out, const Part *part, int index, const int *nodes, NetlistProtection *protection);
int netlist_logic_models(FILE *out);
/* Datasheet ratings of any logic part (loose gate symbols and quad chips use the 74HC family). */
void netlist_logic_ratings(PartType type, double *supply_absolute_max, double *supply_recommended_max,
                           double *output_limit_amps);
int netlist_eeprom_core(FILE *out, const Workbench *bench);
#endif
