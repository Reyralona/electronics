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
    TextId default_comment;            /* first line of the default program, as a comment */
    const char *default_body;
} McuBoard;

const McuBoard *mcu_board(PartType type);
/* IO index for an Arduino pin number, or -1. */
int mcu_board_io(const McuBoard *board, int arduino_pin);
/* Program of a new board: comment in the interface language, then the code. Same text for the same language. */
const char *mcu_board_default_code(const McuBoard *board);
/* Datasheet limits of a board's chip (consulted 20/11/2026): ATmega328P and ATtiny85 40 mA per pin,
 * pins VCC+0.5 V, ATtiny85 VCC 6 V max / 5.5 V recommended; ESP32 not 5 V tolerant (VDD+0.3 = 3.6 V) and
 * about 40 mA per pin (typical value, no absolute maximum is published). */
typedef struct
{
    double pin_amps;        /* absolute maximum DC current per I/O pin */
    double pin_over_supply; /* a pin may go this far above the supply */
    double supply_absolute_max, supply_recommended_max; /* only for boards fed through VCC */
} McuRatings;
const McuRatings *mcu_board_ratings(PartType type);
#define MCU_OUTPUT_OHMS 25.0
#define MCU_PULLUP_OHMS 30000.0
#endif
