#ifndef MCU_LANG_H
#define MCU_LANG_H
#include "ui_text.h"
#include <stddef.h>
/* Arduino-style language subset for the simulated microcontrollers. Pure C: no SDL/OpenGL.
 * Supported: int/long/byte/bool/float/double (and unsigned/const/uint8_t...), 1-D arrays with
 * initializers, functions with parameters and return values, if/else, while, do/while, for,
 * break/continue, C operators (arithmetic, comparison, logical, bitwise, compound assignment,
 * ++/--, ternary), #define of constants, #include (ignored), setup()/loop(), Serial and
 * LiquidCrystal_I2C objects. Strings exist only as literals passed to print functions. */

typedef struct McuProgram McuProgram;
typedef struct McuVm McuVm;

typedef struct
{
    const char *name;
    double value;
} McuConstant;

typedef enum
{
    MCU_LCD_INIT,
    MCU_LCD_BACKLIGHT,
    MCU_LCD_NO_BACKLIGHT,
    MCU_LCD_CLEAR,
    MCU_LCD_SET_CURSOR,
    MCU_LCD_PRINT,
    MCU_LCD_HOME
} McuLcdOperation;

typedef struct
{
    void *context;
    void (*pin_mode)(void *context, int pin, int mode);
    void (*digital_write)(void *context, int pin, int level);
    int (*digital_read)(void *context, int pin);
    int (*analog_read)(void *context, int pin);
    void (*analog_write)(void *context, int pin, int duty);
    void (*serial_write)(void *context, const char *text);
    void (*lcd)(void *context, int object, McuLcdOperation operation, int a, int b, const char *text);
} McuHost;

#define MCU_TIME_TOLERANCE_MS 1e-9
#define MCU_INPUT 0
#define MCU_OUTPUT 1
#define MCU_INPUT_PULLUP 2

/* Compiles source; on failure returns NULL and fills error with "Line N: reason" in the interface language. */
McuProgram *mcu_compile(const char *source, const McuConstant *constants, int constant_count, char *error,
                        size_t capacity, int *error_line);
void mcu_program_free(McuProgram *program);

McuVm *mcu_vm_create(const McuProgram *program);
void mcu_vm_free(McuVm *vm);
/* Runs until delay() waits past now_ms or the instruction budget is spent. Returns 0 on a runtime
 * error (message in error); the VM then stays stopped. */
int mcu_vm_run(McuVm *vm, const McuHost *host, double now_ms, char *error, size_t capacity);
double mcu_vm_wake_ms(const McuVm *vm);

/* Completion catalog for the editor: names, signatures and summaries (texts in the interface language). */
typedef struct
{
    const char *name;
    TextId signature;
    TextId summary;
} McuCompletion;
int mcu_completion_count(void);
const McuCompletion *mcu_completion(int index);
#endif
