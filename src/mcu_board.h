#ifndef MCU_BOARD_H
#define MCU_BOARD_H
#include "workbench.h"
#include "mcu_lang.h"
/* Simulated microcontroller boards. Pure data: no SDL/OpenGL. Each IO k has an Arduino pin number
 * and a device terminal; electrical drive and sensing are emitted by netlist_devices.c. */
typedef struct
{
    PartType type;
    const char *name;
    int io_count;
    int io_terminal[WORKBENCH_MAX_IO]; /* device terminal of IO k */
    int arduino_pin[WORKBENCH_MAX_IO]; /* number used by pinMode/digitalWrite for IO k */
    int supply_terminal, ground_terminal;
    double supply_volts;               /* > 0: on-board regulator (USB powered); 0: fed through VCC */
    int auxiliary_terminal;            /* 3V3 on the Uno, -1 when absent */
    double auxiliary_volts;
    int adc_max;
    int sda_pin, scl_pin, led_builtin; /* Arduino numbers */
    const McuConstant *constants;
    int constant_count;
    const char *default_code;
} McuBoard;

const McuBoard *mcu_board(PartType type);
/* IO index for an Arduino pin number, or -1. */
int mcu_board_io(const McuBoard *board, int arduino_pin);
#define MCU_OUTPUT_OHMS 25.0
#define MCU_PULLUP_OHMS 30000.0
#endif
