#include "spice_session.h"
#include "spice_session_protocol.h"
#include "ui_text.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SpiceSession
{
    HANDLE process, input, output;
    int keep_log;
    unsigned timeout_ms;
    char log_path[MAX_PATH];
};

/* Turns "<code> <number> <ngspice text>" from the host into interface text. */
static void host_error(const char *reply, char *message, size_t capacity)
{
    int code = 0;
    long number = 0;
    int consumed = 0;
    if (sscanf(reply, "%d %ld%n", &code, &number, &consumed) != 2)
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_REPLY_INVALID));
        return;
    }
    const char *text = reply + consumed;
    if (*text == ' ')
        ++text;
    switch (code)
    {
    case SPICE_HOST_SOLVER:
        snprintf(message, capacity, tr(TXT_ENG_SOLVER_MESSAGE), text);
        break;
    case SPICE_HOST_STOPPED:
        snprintf(message, capacity, tr(TXT_ENG_SOLVER_STOPPED), number);
        break;
    case SPICE_HOST_FILE:
        snprintf(message, capacity, "%s", tr(TXT_ENG_TEMP_FILE_FAILED));
        break;
    case SPICE_HOST_MEMORY:
        snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY));
        break;
    case SPICE_HOST_LOAD:
        snprintf(message, capacity, tr(TXT_ENG_SOLVER_LOAD_FAILED), number);
        break;
    case SPICE_HOST_REQUEST:
        snprintf(message, capacity, "%s", tr(TXT_ENG_SOLVER_REQUEST_FAILED));
        break;
    default:
        snprintf(message, capacity, "%s", tr(TXT_ENG_REPLY_INVALID));
        break;
    }
}

static int response(SpiceSession *session, char *text, size_t capacity, char *message, size_t message_capacity)
{
    ULONGLONG started = GetTickCount64();
    size_t count = 0;
    while (GetTickCount64() - started < session->timeout_ms)
    {
        DWORD available = 0, received = 0;
        if (!PeekNamedPipe(session->output, NULL, 0, NULL, &available, NULL))
            break;
        if (!available)
        {
            if (WaitForSingleObject(session->process, 0) == WAIT_OBJECT_0)
            {
                DWORD exit_code = 0;
                GetExitCodeProcess(session->process, &exit_code);
                session->keep_log = 1;
                snprintf(message, message_capacity, tr(TXT_ENG_CALCULATION_STOPPED_LOG), exit_code,
                         session->log_path);
                return 0;
            }
            /* Short replies should not pay the Windows timer quantum on every command. */
            Sleep(GetTickCount64() - started < 5 ? 0 : 1);
            continue;
        }
        char c;
        if (!ReadFile(session->output, &c, 1, &received, NULL) || !received)
            break;
        if (c == '\n')
        {
            text[count] = 0;
            if (!strncmp(text, "OK", 2))
                return 1;
            session->keep_log = 1;
            if (!strncmp(text, "ERR ", 4))
                host_error(text + 4, message, message_capacity);
            else
                snprintf(message, message_capacity, "%s", tr(TXT_ENG_REPLY_INVALID));
            return 0;
        }
        if (count + 1 >= capacity)
        {
            snprintf(message, message_capacity, "%s", tr(TXT_ENG_REPLY_INVALID));
            return 0;
        }
        if (c != '\r')
            text[count++] = c;
    }
    session->keep_log = 1;
    snprintf(message, message_capacity, tr(TXT_ENG_CALCULATION_TIMEOUT_LOG),
             (int)session->timeout_ms, session->log_path);
    return 0;
}

static int request(SpiceSession *session, const char *text, char *reply, size_t reply_capacity,
                   char *message, size_t capacity)
{
    if (!session || !text || strchr(text, '\n') || strchr(text, '\r'))
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_REQUEST_INVALID));
        return 0;
    }
    size_t length = strlen(text);
    if (length + 2 >= SPICE_SESSION_COMMAND_CAPACITY)
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_REQUEST_INVALID));
        return 0;
    }
    char input[SPICE_SESSION_COMMAND_CAPACITY];
    memcpy(input, text, length);
    input[length++] = '\n';
    DWORD written;
    if (!WriteFile(session->input, input, (DWORD)length, &written, NULL) || written != length)
    {
        snprintf(message, capacity, tr(TXT_ENG_CHANNEL_FAILED), GetLastError());
        return 0;
    }
    return response(session, reply, reply_capacity, message, capacity);
}

void spice_session_close(SpiceSession *session)
{
    if (!session)
        return;
    if (session->input)
    {
        DWORD written;
        WriteFile(session->input, "Q\n", 2, &written, NULL);
        CloseHandle(session->input);
    }
    if (session->process)
    {
        if (WaitForSingleObject(session->process, 1000) != WAIT_OBJECT_0)
        {
            TerminateProcess(session->process, 1);
            WaitForSingleObject(session->process, 1000);
        }
        CloseHandle(session->process);
    }
    if (session->output)
        CloseHandle(session->output);
    if (!session->keep_log && session->log_path[0])
        DeleteFileA(session->log_path);
    free(session);
}

SpiceSession *spice_session_open(const char *executable, char *message, size_t capacity)
{
    if (!executable || !message || !capacity)
        return NULL;
    SpiceSession *session = calloc(1, sizeof(*session));
    HANDLE child_input = NULL, child_output = NULL, log = INVALID_HANDLE_VALUE;
    char runtime[MAX_PATH], host[MAX_PATH], model[2 * MAX_PATH], directory[MAX_PATH], command[6 * MAX_PATH];
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION process = {0};
    if (!session)
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY));
        return NULL;
    }
    session->timeout_ms = SPICE_SESSION_TIMEOUT_MS;
    DWORD length = GetFullPathNameA(executable, sizeof(runtime), runtime, NULL);
    if (!length || length >= sizeof(runtime))
        goto failure;
    char *separator = strrchr(runtime, '\\');
    if (!separator)
        goto failure;
    *separator = 0;
    snprintf(model, sizeof(model), "%s/../lib/ngspice/digital.cm", runtime);
    size_t used = strlen(runtime);
    if (used + strlen("/libngspice-0.dll") >= sizeof(runtime))
        goto failure;
    strcat(runtime, "\\libngspice-0.dll");
    length = GetModuleFileNameA(NULL, host, sizeof(host));
    if (!length || length >= sizeof(host))
        goto failure;
    separator = strrchr(host, '\\');
    if (!separator)
        goto failure;
    *separator = 0;
    if (strlen(host) + strlen("/spice-live-host.exe") >= sizeof(host))
        goto failure;
    strcat(host, "\\spice-live-host.exe");
    if (!GetTempPathA(sizeof(directory), directory) ||
        !GetTempFileNameA(directory, "bsl", 0, session->log_path))
        goto failure;
    log = CreateFileA(session->log_path, GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (log == INVALID_HANDLE_VALUE || !CreatePipe(&child_input, &session->input, &security, 0) ||
        !CreatePipe(&session->output, &child_output, &security, 0) ||
        !SetHandleInformation(session->input, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(session->output, HANDLE_FLAG_INHERIT, 0))
        goto failure;
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_input;
    startup.hStdOutput = child_output;
    startup.hStdError = log;
    snprintf(command, sizeof(command), "\"%s\" \"%s\" \"%s\"", host, runtime, model);
    if (!CreateProcessA(host, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process))
        goto failure;
    session->process = process.hProcess;
    CloseHandle(process.hThread);
    CloseHandle(child_input);
    CloseHandle(child_output);
    CloseHandle(log);
    char reply[512];
    if (!response(session, reply, sizeof(reply), message, capacity))
    {
        spice_session_close(session);
        return NULL;
    }
    return session;
failure:
    snprintf(message, capacity, tr(TXT_ENG_SOLVER_START_FAILED), GetLastError());
    if (child_input)
        CloseHandle(child_input);
    if (child_output)
        CloseHandle(child_output);
    if (log != INVALID_HANDLE_VALUE)
        CloseHandle(log);
    spice_session_close(session);
    return NULL;
}

void spice_session_set_timeout(SpiceSession *session, unsigned milliseconds)
{
    if (!session)
        return;
    session->timeout_ms = milliseconds ? milliseconds : SPICE_SESSION_TIMEOUT_MS;
}

int spice_session_load(SpiceSession *session, const char *path, char *message, size_t capacity)
{
    char command[SPICE_SESSION_COMMAND_CAPACITY], reply[512];
    if (snprintf(command, sizeof(command), "L %s", path) >= (int)sizeof(command))
        return 0;
    return request(session, command, reply, sizeof(reply), message, capacity);
}

int spice_session_command(SpiceSession *session, const char *text, char *message, size_t capacity)
{
    char command[SPICE_SESSION_COMMAND_CAPACITY], reply[512];
    if (snprintf(command, sizeof(command), "C %s", text) >= (int)sizeof(command))
        return 0;
    return request(session, command, reply, sizeof(reply), message, capacity);
}

int spice_session_value(SpiceSession *session, const char *vector, double *value, char *message, size_t capacity)
{
    char command[SPICE_SESSION_COMMAND_CAPACITY], reply[512], extra;
    if (!value || snprintf(command, sizeof(command), "V %s", vector) >= (int)sizeof(command) ||
        !request(session, command, reply, sizeof(reply), message, capacity))
        return 0;
    double result;
    if (sscanf(reply, "OK %lf %c", &result, &extra) != 1 || !isfinite(result))
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_RESULT_INVALID));
        return 0;
    }
    *value = result;
    return 1;
}
