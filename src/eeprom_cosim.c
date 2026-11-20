/* d_cosim ABI from ngspice 45.2 cosim.h/cmtypes.h; see docs/PLANO_BANCADA_EEPROM.md.
 * One aggregate instance per circuit, owned by the isolated solver process. */
#include "eeprom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { int state, strength; } Digital;
typedef struct Cosim Cosim;
struct Cosim
{
    unsigned in_count, out_count, inout_count;
    void (*cleanup)(Cosim *);
    void (*step)(Cosim *);
    void (*in_fn)(Cosim *, unsigned, Digital *);
    void (*out_fn)(Cosim *, unsigned, Digital *);
    void *handle;
    volatile double vtime;
    int method;
    unsigned lib_argc, sim_argc;
    const char *const *lib_argv;
    const char *const *sim_argv;
    void *(*dlopen_fn)(const char *);
};

enum { INPUTS = 23, OUTPUTS = 9, DIGITAL_UNKNOWN = 2 };
typedef struct
{
    Eeprom memory;
    int part, pins[INPUTS], outputs[OUTPUTS];
} Device;
typedef struct
{
    Device *devices;
    unsigned count;
} Model;
static Model *active;

static EepromPins signals(const Device *device)
{
    EepromPins pins = {0};
    pins.address_known = pins.data_known = 1;
    for (int bit = 0; bit < 11; ++bit)
    {
        if (device->pins[bit] == 1)
            pins.address |= 1u << bit;
        else if (device->pins[bit] != 0)
            pins.address_known = 0;
    }
    for (int bit = 0; bit < 8; ++bit)
    {
        if (device->pins[11 + bit] == 1)
            pins.data |= 1u << bit;
        else if (device->pins[11 + bit] != 0)
            pins.data_known = 0;
    }
    pins.ce = device->pins[19];
    pins.oe = device->pins[20];
    pins.we = device->pins[21];
    pins.powered = device->pins[22] == 1;
    return pins;
}

static void input(Cosim *info, unsigned bit, Digital *value)
{
    Model *model = info->handle;
    if (bit >= model->count * INPUTS)
        return;
    Device *device = &model->devices[bit / INPUTS];
    device->pins[bit % INPUTS] = value->state == DIGITAL_UNKNOWN ? -1 : value->state;
    eeprom_update(&device->memory, info->vtime, signals(device));
}

static void step(Cosim *info)
{
    Model *model = info->handle;
    double until = info->vtime;
    for (unsigned i = 0; i < model->count; ++i)
    {
        double event = eeprom_next_event(&model->devices[i].memory);
        if (event < until)
            until = event;
    }
    info->vtime = until;
    for (unsigned i = 0; i < model->count; ++i)
    {
        Device *device = &model->devices[i];
        eeprom_update(&device->memory, until, signals(device));
        unsigned byte = 0;
        int enabled = eeprom_output(&device->memory, &byte);
        for (int bit = 0; bit < OUTPUTS; ++bit)
        {
            int state = bit == 8 ? enabled : (int)(byte >> bit & 1);
            if (device->outputs[bit] == state)
                continue;
            Digital output = {state, 0};
            info->out_fn(info, i * OUTPUTS + (unsigned)bit, &output);
            device->outputs[bit] = state;
        }
    }
}

static void cleanup(Cosim *info)
{
    Model *model = info->handle;
    if (active == model)
        active = NULL;
    free(model->devices);
    free(model);
}

__declspec(dllexport) void Cosim_setup(Cosim *info)
{
    Model *model = calloc(1, sizeof(*model));
    if (!model)
        return;
    model->count = info->lib_argc;
    model->devices = calloc(model->count, sizeof(*model->devices));
    if (!model->devices)
    {
        free(model);
        return;
    }
    for (unsigned i = 0; i < model->count; ++i)
    {
        Device *device = &model->devices[i];
        const char *argument = info->lib_argv[i];
        int consumed = 0;
        if (sscanf(argument, "%d:%n", &device->part, &consumed) != 1 || consumed <= 0 || device->part < 0 ||
            strlen(argument + consumed) != ROM_IMAGE_BYTES * 2)
        {
            free(model->devices);
            free(model);
            return;
        }
        RomImage image;
        for (int address = 0; address < ROM_IMAGE_BYTES; ++address)
        {
            unsigned byte;
            const char *pair = argument + consumed + address * 2;
            if (!isxdigit((unsigned char)pair[0]) || !isxdigit((unsigned char)pair[1]) ||
                sscanf(pair, "%2x", &byte) != 1)
            {
                free(model->devices);
                free(model);
                return;
            }
            image.data[address] = (unsigned char)byte;
        }
        eeprom_init(&device->memory, &image);
        for (int pin = 0; pin < INPUTS; ++pin)
            device->pins[pin] = -1;
        for (int bit = 0; bit < OUTPUTS; ++bit)
            device->outputs[bit] = -1;
    }
    info->in_count = model->count * INPUTS;
    info->out_count = model->count * OUTPUTS;
    info->inout_count = 0;
    info->handle = model;
    info->in_fn = input;
    info->step = step;
    info->cleanup = cleanup;
    info->method = 0;
    active = model;
}

/* Called only between solver requests by spice-live-host, never by the UI thread. */
__declspec(dllexport) int BancadaEepromNext(int *part, unsigned char *bytes)
{
    if (!active || !part || !bytes)
        return 0;
    for (unsigned i = 0; i < active->count; ++i)
    {
        Device *device = &active->devices[i];
        if (!device->memory.dirty)
            continue;
        *part = device->part;
        memcpy(bytes, device->memory.image.data, ROM_IMAGE_BYTES);
        device->memory.dirty = 0;
        return 1;
    }
    return 0;
}
