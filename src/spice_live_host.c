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
static char last_error[512];

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
        solver_error = 1;
        const char *detail = !strncmp(message, "stderr ", 7) ? message + 7 : message;
        snprintf(last_error, sizeof(last_error), "%s", detail);
    }
    return 0;
}

static int controlled_exit(int status, bool unload, bool quitting, int identifier, void *user)
{
    (void)unload;
    (void)quitting;
    (void)identifier;
    (void)user;
    solver_error = 1;
    snprintf(last_error, sizeof(last_error), "ngspice pediu encerramento (%d).", status);
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
        snprintf(last_error, sizeof(last_error), "Nao foi possivel abrir netlist: %s", strerror(errno));
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
        snprintf(last_error, sizeof(last_error), "Memoria insuficiente para ler netlist.");
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
        snprintf(last_error, sizeof(last_error), failed ? "Falha ao ler arquivo de netlist." :
                                                         "Memoria insuficiente para linhas do netlist.");
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
        printf("ERR %s\n", last_error[0] ? last_error : "Comando ou vetor ngspice indisponivel.");
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
        printf("ERR DLL ngspice indisponivel (Windows %lu).\n", GetLastError());
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
        last_error[0] = 0;
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
