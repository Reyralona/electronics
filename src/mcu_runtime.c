#include "mcu_runtime.h"
#include "mcu_board.h"
#include "netlist.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    McuRuntime *runtime;
    Workbench *bench;
    int index;
    const McuBoard *board;
    const double *elements;
} HostContext;

McuRuntime *mcu_runtime_create(void)
{
    return calloc(1, sizeof(McuRuntime));
}

static void instance_clear(McuInstance *instance)
{
    mcu_vm_free(instance->vm);
    mcu_program_free(instance->program);
    free(instance->source);
    memset(instance, 0, sizeof(*instance));
}

void mcu_runtime_destroy(McuRuntime *runtime)
{
    if (!runtime)
        return;
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
        instance_clear(&runtime->mcu[i]);
    free(runtime);
}

void mcu_runtime_reset(McuRuntime *runtime)
{
    if (!runtime)
        return;
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
        instance_clear(&runtime->mcu[i]);
    memset(runtime->lcd, 0, sizeof(runtime->lcd));
}

const char *mcu_program_text(const Part *part)
{
    const McuBoard *board = mcu_board(part->type);
    return part->code ? part->code : board ? board->default_code : "";
}

static Part *host_part(HostContext *host)
{
    return &host->bench->parts[host->index];
}

static double supply_volts(const HostContext *host)
{
    return host->board->supply_volts > 0 ? host->board->supply_volts : 5.0;
}

static void host_pin_mode(void *context, int pin, int mode)
{
    HostContext *host = context;
    int io = mcu_board_io(host->board, pin);
    if (io < 0 || mode < MCU_INPUT || mode > MCU_INPUT_PULLUP)
        return;
    host_part(host)->io_mode[io] = (unsigned char)mode;
    if (mode != MCU_OUTPUT)
        host_part(host)->io_level[io] = 0;
}

static void host_digital_write(void *context, int pin, int level)
{
    HostContext *host = context;
    int io = mcu_board_io(host->board, pin);
    if (io < 0)
        return;
    Part *part = host_part(host);
    if (part->io_mode[io] == MCU_OUTPUT)
        part->io_level[io] = level ? 1.0f : 0.0f;
    else /* Arduino: writing HIGH to an input enables its pull-up */
        part->io_mode[io] = level ? MCU_INPUT_PULLUP : MCU_INPUT;
}

static double pin_volts(const HostContext *host, int pin)
{
    int io = mcu_board_io(host->board, pin);
    return io >= 0 && host->elements ? host->elements[io] : 0;
}

static int host_digital_read(void *context, int pin)
{
    const HostContext *host = context;
    return pin_volts(host, pin) > supply_volts(host) * .5;
}

static int host_analog_read(void *context, int pin)
{
    const HostContext *host = context;
    double ratio = fmax(0, fmin(1, pin_volts(host, pin) / supply_volts(host)));
    return (int)lround(ratio * host->board->adc_max);
}

/* PWM is represented by its average level; the circuit sees duty * supply. */
static void host_analog_write(void *context, int pin, int duty)
{
    HostContext *host = context;
    int io = mcu_board_io(host->board, pin);
    if (io < 0)
        return;
    host_part(host)->io_mode[io] = MCU_OUTPUT;
    host_part(host)->io_level[io] = (float)duty / 255.0f;
}

static void host_serial(void *context, const char *text)
{
    HostContext *host = context;
    char *serial = host->runtime->mcu[host->index].serial;
    size_t used = strlen(serial), length = strlen(text);
    if (length >= MCU_SERIAL_CAPACITY)
        return;
    if (used + length >= MCU_SERIAL_CAPACITY)
    {
        size_t drop = used + length - MCU_SERIAL_CAPACITY + 1024;
        drop = drop > used ? used : drop;
        memmove(serial, serial + drop, used - drop + 1);
        used -= drop;
    }
    memcpy(serial + used, text, length + 1);
}

static int lcd_connected(const HostContext *host, int lcd)
{
    const int sda_terminal = 2, scl_terminal = 3;
    int sda = mcu_board_io(host->board, host->board->sda_pin), scl = mcu_board_io(host->board, host->board->scl_pin);
    if (sda < 0 || scl < 0)
        return 0;
    Pin board_sda = {host->index, host->board->io_terminal[sda]}, board_scl = {host->index, host->board->io_terminal[scl]};
    return netlist_same_net(host->bench, (Pin){lcd, sda_terminal}, board_sda) &&
           netlist_same_net(host->bench, (Pin){lcd, scl_terminal}, board_scl);
}

static void lcd_apply(LcdScreen *screen, McuLcdOperation operation, int a, int b, const char *text)
{
    screen->used = 1;
    if (operation == MCU_LCD_INIT || operation == MCU_LCD_CLEAR || operation == MCU_LCD_HOME)
    {
        if (operation != MCU_LCD_HOME)
            for (int row = 0; row < LCD_ROWS; ++row)
            {
                memset(screen->text[row], ' ', LCD_COLUMNS);
                screen->text[row][LCD_COLUMNS] = 0;
            }
        screen->column = screen->row = 0;
    }
    else if (operation == MCU_LCD_BACKLIGHT || operation == MCU_LCD_NO_BACKLIGHT)
        screen->backlight = operation == MCU_LCD_BACKLIGHT;
    else if (operation == MCU_LCD_SET_CURSOR)
    {
        screen->column = a < 0 ? 0 : a > LCD_COLUMNS ? LCD_COLUMNS : a;
        screen->row = b < 0 ? 0 : b >= LCD_ROWS ? LCD_ROWS - 1 : b;
    }
    else if (operation == MCU_LCD_PRINT)
        for (const char *s = text; *s && screen->column < LCD_COLUMNS; ++s)
            screen->text[screen->row][screen->column++] = (unsigned char)*s < 128 ? *s : '?';
}

static void host_lcd(void *context, int object, McuLcdOperation operation, int a, int b, const char *text)
{
    HostContext *host = context;
    (void)object;
    for (int i = 0; i < host->bench->part_count; ++i)
        if (host->bench->parts[i].type == PART_LCD_I2C && lcd_connected(host, i))
            lcd_apply(&host->runtime->lcd[i], operation, a, b, text);
}

static int instance_prepare(McuRuntime *runtime, int index, const Part *part, const McuBoard *board)
{
    McuInstance *instance = &runtime->mcu[index];
    const char *source = mcu_program_text(part);
    if (instance->source && instance->type == part->type && !strcmp(instance->source, source))
        return instance->vm != NULL;
    instance_clear(instance);
    instance->type = part->type;
    instance->source = _strdup(source);
    instance->started_ms = runtime->clock_ms;
    if (!instance->source)
        return 0;
    instance->program = mcu_compile(source, board->constants, board->constant_count, instance->error,
                                    sizeof(instance->error), &instance->error_line);
    instance->vm = instance->program ? mcu_vm_create(instance->program) : NULL;
    return instance->vm != NULL;
}

static void release_pins(Part *part)
{
    memset(part->io_level, 0, sizeof(part->io_level));
    memset(part->io_mode, 0, sizeof(part->io_mode));
}

void mcu_runtime_step(McuRuntime *runtime, Workbench *bench,
                      const double (*elements)[COMPONENT_MAX_ELEMENTS], double elapsed_ms)
{
    if (!runtime || !bench)
        return;
    runtime->clock_ms += elapsed_ms > 0 ? elapsed_ms : 0;
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
    {
        const McuBoard *board = i < bench->part_count ? mcu_board(bench->parts[i].type) : NULL;
        if (!board)
        {
            if (runtime->mcu[i].source)
                instance_clear(&runtime->mcu[i]);
            continue;
        }
        if (!instance_prepare(runtime, i, &bench->parts[i], board))
        {
            release_pins(&bench->parts[i]);
            continue;
        }
        McuInstance *instance = &runtime->mcu[i];
        HostContext context = {runtime, bench, i, board, elements ? elements[i] : NULL};
        McuHost host = {&context, host_pin_mode, host_digital_write, host_digital_read, host_analog_read,
                        host_analog_write, host_serial, host_lcd};
        if (!mcu_vm_run(instance->vm, &host, runtime->clock_ms - instance->started_ms, instance->error,
                        sizeof(instance->error)))
            release_pins(&bench->parts[i]);
    }
}
