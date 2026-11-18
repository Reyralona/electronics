#ifndef SPICE_SESSION_PROTOCOL_H
#define SPICE_SESSION_PROTOCOL_H
/* Internal IPC bound, not a limit on circuits or saved paths. */
#define SPICE_SESSION_COMMAND_CAPACITY 4096
#define SPICE_SESSION_TIMEOUT_MS 10000
/* Live graph samples kept by the host between two "S" requests. When a batch produces more, the
 * host halves the resolution of what it holds, so one reply never grows beyond this. */
#define SPICE_SESSION_MAX_SAMPLES 256
/* Requests: "L <file>", "C <ngspice command>", "V <vector>" (last value),
 * "P <interval> [vector]" (sample a vector on a time grid; no vector stops sampling) and
 * "S" (answers "OK <start> <interval> <count> <values...>" and empties the samples). */
/* A failed request answers "ERR <code> <number> <ngspice text>". The host sends no interface text:
 * the app turns the code into a sentence in the current language. */
typedef enum
{
    SPICE_HOST_SOLVER = 1, /* ngspice reported an error; the text is its own message. */
    SPICE_HOST_STOPPED,    /* ngspice asked to exit; the number is its status. */
    SPICE_HOST_FILE,       /* The circuit file could not be read; the number is errno or 0. */
    SPICE_HOST_MEMORY,
    SPICE_HOST_LOAD,       /* The ngspice DLL did not load; the number is the Windows error. */
    SPICE_HOST_REQUEST     /* Unknown command, or a vector without a value. */
} SpiceHostError;
#endif
