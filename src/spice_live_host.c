/* Separate process: one ngspice DLL and one document per host.
 * ABI subset follows the installed sharedspice.h and https://ngspice.sourceforge.io/shared.html.
 * Ngspice diagnostics go to stderr; stdout contains only the request/response protocol. */
#include "spice_session_protocol.h"
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
struct vecvaluesall;
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
    int failed = initialize(output, NULL, controlled_exit, NULL, NULL, NULL, NULL);
    char model[MAX_PATH], input[SPICE_SESSION_COMMAND_CAPACITY];
    if (!GetShortPathNameA(argv[2], model, sizeof(model)))
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
        else
            respond(1);
    }
    FreeLibrary(module);
    return 0;
}
