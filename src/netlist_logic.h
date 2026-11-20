#ifndef NETLIST_LOGIC_H
#define NETLIST_LOGIC_H
#include <stdio.h>
#include "workbench.h"
/* SPICE text of medium-scale logic ICs (counters, registers, adder, bus transceiver, EEPROM).
 * The inside of each chip is XSPICE digital primitives; only its pins cross into the analog circuit:
 * inputs through adc bridges, outputs through two switches to the chip's own VCC/GND pins, opened
 * together when a 3-state output is disabled. Pure text generation. */
int netlist_logic_supported(PartType type);
/* An EEPROM whose text cannot be read is invalid (see rom_image.h); the other chips always are. */
int netlist_logic_valid(const Part *part);
/* nodes: SPICE node of each terminal (0 = ground). Returns 0 on I/O failure or an unreadable EEPROM text. */
int netlist_logic_write(FILE *out, const Part *part, int index, const int *nodes);
int netlist_logic_models(FILE *out);
#endif
