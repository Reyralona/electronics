#ifndef WORKBENCH_COMPUTER_H
#define WORKBENCH_COMPUTER_H
#include "workbench.h"
/* Example 8-bit computer in the style of the SAP-1 teaching design (Malvino), built from 74HC chips and
 * three AT28C16 EEPROMs: program memory and two microcode memories. Instructions (high nibble):
 * 1 LDA addr, 2 ADD addr, 3 SUB addr, E OUT, F HLT. The program computes 28 + 14 - 7 and shows 35 on the
 * eight output LEDs, least significant bit first, then halts. */
#define COMPUTER_PROGRAM_RESULT 35
#define COMPUTER_CLOCK_HZ 10.0
#define COMPUTER_OUTPUT_BITS 8
int workbench_build_computer(Workbench *bench, int *selected);
/* Index of the LED that shows output bit `bit` in a bench built above, or -1. */
int workbench_computer_output_led(const Workbench *bench, int bit);
#endif
