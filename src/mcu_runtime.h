#ifndef MCU_RUNTIME_H
#define MCU_RUNTIME_H
#include "workbench.h"
#include "component_devices.h"
#include "mcu_lang.h"
/* Runs the programs of microcontroller parts against measured pin voltages. Pure: no SDL/OpenGL.
 * The UI advances programs on the accepted solver clock. Output drive is written to Part.io_*
 * (runtime fields, not versioned). */
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
    int runs_default; /* running the board's default program (Part.code is NULL) */
    uint64_t identity;
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
    uint64_t next_identity;
    int *connection_nodes;
    const Workbench *connection_bench;
    uint64_t connection_revision;
} McuRuntime;

McuRuntime *mcu_runtime_create(void);
void mcu_runtime_destroy(McuRuntime *runtime);
/* Restarts every program from setup() and clears serial output and screens. */
void mcu_runtime_reset(McuRuntime *runtime);
/* elements may be NULL before the first measurement (inputs read 0 V). */
void mcu_runtime_step(McuRuntime *runtime, Workbench *bench,
                      const double (*elements)[COMPONENT_MAX_ELEMENTS], double elapsed_ms);
/* Advance to an absolute simulated time; rewinding restarts firmware. */
void mcu_runtime_at(McuRuntime *runtime, Workbench *bench,
                    const double (*elements)[COMPONENT_MAX_ELEMENTS], double seconds);
/* Absolute simulated wake time of the next delay/delayMicroseconds, or INFINITY. */
double mcu_runtime_next_event(const McuRuntime *runtime);
const char *mcu_program_text(const Part *part);
#endif
