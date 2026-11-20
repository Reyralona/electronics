/* Separate process: one ngspice DLL and one document per host.
 * ABI subset follows the installed sharedspice.h and https://ngspice.sourceforge.io/shared.html.
 * Ngspice diagnostics go to stderr; stdout contains only the request/response protocol. */
#include "spice_session_protocol.h"
#include "rom_image.h"
#include <windows.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    char *name;
    int type;
    short flags;
    double *real;
    void *complex;
    int length;
} SharedVector;

typedef int (*OutputCallback)(char *, int, void *);
typedef int (*ExitCallback)(int, bool, bool, int, void *);
/* Per-point values sent to the data callback; delivered even under "save none". */
struct vecvalues
{
    char *name;
    double creal;
    double cimag;
    bool is_scale;
    bool is_complex;
};
struct vecvaluesall
{
    int veccount;
    int vecindex;
    struct vecvalues **vecsa;
};
struct vecinfoall;
typedef int (*StatusCallback)(char *, int, void *);
typedef int (*DataCallback)(struct vecvaluesall *, int, int, void *);
typedef int (*InitDataCallback)(struct vecinfoall *, int, void *);
typedef int (*ThreadCallback)(bool, int, void *);
typedef int (*Initialize)(OutputCallback, StatusCallback, ExitCallback, DataCallback,
                          InitDataCallback, ThreadCallback, void *);
typedef int (*Command)(char *);
typedef int (*Circuit)(char **);
typedef SharedVector *(*VectorInfo)(char *);

static int solver_error;
static SpiceHostError error_code;
static long error_number;
static char error_text[512];

/* The first failure of a request wins; ngspice often prints several lines for one error. */
static void set_error(SpiceHostError code, long number, const char *text)
{
    solver_error = 1;
    if (error_code)
        return;
    error_code = code;
    error_number = number;
    snprintf(error_text, sizeof(error_text), "%s", text);
}

static int output(char *message, int identifier, void *user)
{
    (void)identifier;
    (void)user;
    fprintf(stderr, "%s\n", message);
    if (strstr(message, "Error") || strstr(message, "error:") || strstr(message, "fatal") ||
        strstr(message, "no such command") || strstr(message, "aborted") ||
        strstr(message, "timestep too small") || strstr(message, "Timestep too small") ||
        strstr(message, "singular matrix") || strstr(message, "convergence failed"))
    {
        const char *detail = !strncmp(message, "stderr ", 7) ? message + 7 : message;
        set_error(SPICE_HOST_SOLVER, 0, detail);
    }
    return 0;
}

/* One vector sampled on a regular time grid with linear interpolation between solver points. */
typedef struct
{
    char name[64];
    double interval, step, start, next;
    double previous_time, previous_value;
    int index, scale, has_previous, count;
    double values[SPICE_SESSION_MAX_SAMPLES];
} Probe;

static Probe probe;

/* ngspice names numbered nodes "V(841)"; the app asks for "841", as with the "V" request. */
static int probe_matches(const char *name)
{
    if (!_stricmp(name, probe.name))
        return 1;
    size_t length = strlen(name);
    if (length < 4 || (name[0] != 'V' && name[0] != 'v') || name[1] != '(' || name[length - 1] != ')')
        return 0;
    return length - 3 == strlen(probe.name) && !_strnicmp(name + 2, probe.name, length - 3);
}

static void probe_resolve(const struct vecvaluesall *all)
{
    probe.index = -1;
    probe.scale = -1;
    for (int i = 0; i < all->veccount; ++i)
    {
        if (all->vecsa[i]->is_scale)
            probe.scale = i;
        else if (probe_matches(all->vecsa[i]->name))
            probe.index = i;
    }
}

static void probe_restart_grid(double start)
{
    probe.start = start;
    probe.next = start;
    probe.step = probe.interval;
    probe.count = 0;
}

static void probe_halve_resolution(void)
{
    for (int i = 0; i < SPICE_SESSION_MAX_SAMPLES / 2; ++i)
        probe.values[i] = probe.values[2 * i];
    probe.count = SPICE_SESSION_MAX_SAMPLES / 2;
    probe.step *= 2;
}

static double probe_interpolate(double time, double value, double at)
{
    if (!probe.has_previous || time <= probe.previous_time || at <= probe.previous_time)
        return value;
    double fraction = (at - probe.previous_time) / (time - probe.previous_time);
    return probe.previous_value + (value - probe.previous_value) * fraction;
}

static void probe_record(double time, double value)
{
    if (probe.next < 0)
        probe_restart_grid(time);
    while (probe.next <= time)
    {
        if (probe.count == SPICE_SESSION_MAX_SAMPLES)
            probe_halve_resolution();
        else
            probe.values[probe.count++] = probe_interpolate(time, value, probe.next);
        probe.next = probe.start + probe.count * probe.step;
    }
    probe.previous_time = time;
    probe.previous_value = value;
    probe.has_previous = 1;
}

static int receive_data(struct vecvaluesall *all, int count, int identifier, void *user)
{
    (void)count;
    (void)identifier;
    (void)user;
    if (!probe.name[0] || !all)
        return 0;
    if (probe.index < 0 || probe.index >= all->veccount || probe.scale < 0 || probe.scale >= all->veccount ||
        !probe_matches(all->vecsa[probe.index]->name))
        probe_resolve(all);
    if (probe.index < 0 || probe.scale < 0)
        return 0;
    probe_record(all->vecsa[probe.scale]->creal, all->vecsa[probe.index]->creal);
    return 0;
}

/* Each tran/resume announces its vectors again; indices are found on the next point. */
static int receive_vector_info(struct vecinfoall *info, int identifier, void *user)
{
    (void)info;
    (void)identifier;
    (void)user;
    probe.index = -1;
    return 0;
}

static int set_probe(const char *arguments)
{
    double interval = 0;
    char name[sizeof(probe.name)] = "";
    int fields = sscanf(arguments, "%lf %63s", &interval, name);
    if (fields < 1 || !(interval > 0) || interval > 1e12)
        return 1;
    memset(&probe, 0, sizeof(probe));
    snprintf(probe.name, sizeof(probe.name), "%s", fields == 2 ? name : "");
    probe.interval = interval;
    probe.index = -1;
    probe.next = -1;
    return 0;
}

static void send_samples(void)
{
    printf("OK %.17g %.17g %d", probe.start, probe.step, probe.count);
    for (int i = 0; i < probe.count; ++i)
        printf(" %.9g", probe.values[i]);
    putchar('\n');
    fflush(stdout);
    if (probe.next >= 0)
        probe_restart_grid(probe.next);
}

static int controlled_exit(int status, bool unload, bool quitting, int identifier, void *user)
{
    (void)unload;
    (void)quitting;
    (void)identifier;
    (void)user;
    set_error(SPICE_HOST_STOPPED, status, "");
    return 0;
}

static int bind_function(HMODULE module, const char *name, void *function, size_t size)
{
    FARPROC address = GetProcAddress(module, name);
    if (!address || size != sizeof(address))
        return 0;
    memcpy(function, &address, size);
    return 1;
}

static int load_circuit(Circuit circuit, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Cannot open circuit file %s: %s\n", path, strerror(errno));
        set_error(SPICE_HOST_FILE, errno, "");
        return 1;
    }
    if (fseek(file, 0, SEEK_END))
    {
        fclose(file);
        return 1;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET))
    {
        fclose(file);
        return 1;
    }
    char *text = malloc((size_t)length + 1);
    if (!text)
    {
        set_error(SPICE_HOST_MEMORY, 0, "");
        fclose(file);
        return 1;
    }
    size_t count = fread(text, 1, (size_t)length, file);
    int failed = ferror(file) || count != (size_t)length;
    fclose(file);
    text[count] = 0;
    size_t lines = 2;
    for (size_t i = 0; i < count; ++i)
        lines += text[i] == '\n';
    char **deck = calloc(lines, sizeof(*deck));
    if (!deck || failed)
    {
        set_error(failed ? SPICE_HOST_FILE : SPICE_HOST_MEMORY, 0, "");
        free(deck);
        free(text);
        return 1;
    }
    size_t line = 0;
    deck[line++] = text;
    for (size_t i = 0; i < count; ++i)
        if (text[i] == '\n')
        {
            text[i] = 0;
            if (i && text[i - 1] == '\r')
                text[i - 1] = 0;
            if (i + 1 < count)
                deck[line++] = text + i + 1;
        }
    int result = circuit(deck);
    free(deck);
    free(text);
    return result;
}

static void respond(int failed)
{
    if (failed || solver_error)
        printf("ERR %d %ld %s\n", error_code ? (int)error_code : SPICE_HOST_REQUEST, error_number, error_text);
    else
        puts("OK");
    fflush(stdout);
}

static void send_eeprom(void)
{
    HMODULE module = GetModuleHandleA("bancada-eeprom.dll");
    int (*next)(int *, unsigned char *) = NULL;
    int part = -1;
    unsigned char bytes[ROM_IMAGE_BYTES];
    if (module && bind_function(module, "BancadaEepromNext", &next, sizeof(next)) && next(&part, bytes))
    {
        printf("OK %d ", part);
        for (int address = 0; address < ROM_IMAGE_BYTES; ++address)
            printf("%02X", bytes[address]);
        putchar('\n');
    }
    else
        puts("OK -1");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    HMODULE module = LoadLibraryExA(argv[1], NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    Initialize initialize = NULL;
    Command command = NULL;
    Circuit circuit = NULL;
    VectorInfo vector_info = NULL;
    if (!module || !bind_function(module, "ngSpice_Init", &initialize, sizeof(initialize)) ||
        !bind_function(module, "ngSpice_Command", &command, sizeof(command)) ||
        !bind_function(module, "ngSpice_Circ", &circuit, sizeof(circuit)) ||
        !bind_function(module, "ngGet_Vec_Info", &vector_info, sizeof(vector_info)))
    {
        printf("ERR %d %lu\n", SPICE_HOST_LOAD, GetLastError());
        return 3;
    }
    int failed = initialize(output, NULL, controlled_exit, receive_data, receive_vector_info, NULL, NULL);
    char model[MAX_PATH], input[SPICE_SESSION_COMMAND_CAPACITY];
    /* ngspice resolves relative codemodel paths against the host executable, not its CWD.
     * The packaged layout therefore needs no quoting or Windows 8.3 short names. */
    char executable[MAX_PATH], packaged[2 * MAX_PATH], requested[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, executable, sizeof(executable));
    if (!length || length >= sizeof(executable))
        executable[0] = 0;
    char *separator = length && length < sizeof(executable) ? strrchr(executable, '\\') : NULL;
    if (separator)
        *separator = 0;
    snprintf(packaged, sizeof(packaged), "%s\\spice-local\\ucrt64\\lib\\ngspice\\digital.cm", executable);
    DWORD requested_length = GetFullPathNameA(argv[2], sizeof(requested), requested, NULL);
    if (separator && requested_length && requested_length < sizeof(requested) &&
        _stricmp(packaged, requested) == 0 && GetFileAttributesA(packaged) != INVALID_FILE_ATTRIBUTES)
        snprintf(model, sizeof(model), "spice-local/ucrt64/lib/ngspice/digital.cm");
    else if (!GetShortPathNameA(argv[2], model, sizeof(model)))
        snprintf(model, sizeof(model), "%s", argv[2]);
    for (size_t i = 0; model[i]; ++i)
        if (model[i] == '\\')
            model[i] = '/';
    snprintf(input, sizeof(input), "codemodel %s", model);
    failed = command(input) || failed;
    respond(failed);
    while (!failed && fgets(input, sizeof(input), stdin))
    {
        input[strcspn(input, "\r\n")] = 0;
        solver_error = 0;
        error_code = 0;
        error_number = 0;
        error_text[0] = 0;
        if (!strcmp(input, "Q"))
            break;
        if (!strncmp(input, "L ", 2))
            respond(load_circuit(circuit, input + 2));
        else if (!strncmp(input, "C ", 2))
            respond(command(input + 2));
        else if (!strncmp(input, "V ", 2))
        {
            SharedVector *vector = vector_info(input + 2);
            if (!vector || !vector->real || vector->length < 1)
                respond(1);
            else
            {
                printf("OK %.17g\n", vector->real[vector->length - 1]);
                fflush(stdout);
            }
        }
        else if (!strncmp(input, "P ", 2))
            respond(set_probe(input + 2));
        else if (!strcmp(input, "S"))
            send_samples();
        else if (!strcmp(input, "E"))
            send_eeprom();
        else
            respond(1);
    }
    FreeLibrary(module);
    return 0;
}
