#ifndef MCU_RUNTIME_H
#define MCU_RUNTIME_H
#include "workbench.h"
#include "component_devices.h"
#include "mcu_lang.h"
/* Runs the programs of microcontroller parts against measured pin voltages. Pure: no SDL/OpenGL.
 * Programs run in real (wall-clock) time: millis() follows the user's clock, not the analog
 * simulation time scale. Output drive is written to Part.io_* (runtime fields, not versioned). */
#define MCU_SERIAL_CAPACITY 4096
#define LCD_COLUMNS 16
#define LCD_ROWS 2

typedef struct
{
    char *source;
    McuProgram *program;
    McuVm *vm;
    PartType type;
    double started_ms;
    char serial[MCU_SERIAL_CAPACITY];
    char error[200];
    int error_line;
} McuInstance;

typedef struct
{
    char text[LCD_ROWS][LCD_COLUMNS + 1];
    int column, row, backlight, used;
} LcdScreen;

typedef struct
{
    McuInstance mcu[WORKBENCH_MAX_COMPONENTS];
    LcdScreen lcd[WORKBENCH_MAX_COMPONENTS];
    double clock_ms;
} McuRuntime;

McuRuntime *mcu_runtime_create(void);
void mcu_runtime_destroy(McuRuntime *runtime);
/* Restarts every program from setup() and clears serial output and screens. */
void mcu_runtime_reset(McuRuntime *runtime);
/* elements may be NULL before the first measurement (inputs read 0 V). */
void mcu_runtime_step(McuRuntime *runtime, Workbench *bench,
                      const double (*elements)[COMPONENT_MAX_ELEMENTS], double elapsed_ms);
const char *mcu_program_text(const Part *part);
#endif
