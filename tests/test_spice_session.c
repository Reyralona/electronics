#include "spice_session.h"
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static char message[1024];
static void command(SpiceSession *session, const char *text)
{
    int success = spice_session_command(session, text, message, sizeof(message));
    if (!success)
        fprintf(stderr, "%s: %s\n", text, message);
    assert(success);
}
static double value(SpiceSession *session, const char *name)
{
    double result;
    int success = spice_session_value(session, name, &result, message, sizeof(message));
    if (!success)
        fprintf(stderr, "%s: %s\n", name, message);
    assert(success);
    return result;
}
static void load(SpiceSession *session, const char *deck)
{
    char directory[MAX_PATH], path[MAX_PATH];
    assert(GetTempPathA(sizeof(directory), directory));
    assert(GetTempFileNameA(directory, "bsc", 0, path));
    FILE *file = fopen(path, "w");
    assert(file && fputs(deck, file) >= 0);
    assert(!fclose(file));
    int success = spice_session_load(session, path, message, sizeof(message));
    if (!success)
        fprintf(stderr, "Load: %s\n", message);
    assert(success);
    DeleteFileA(path);
}
static void advance(SpiceSession *session, double until, int first)
{
    char text[100];
    command(session, "delete all");
    snprintf(text, sizeof(text), "stop when time >= %.17g", until);
    command(session, text);
    command(session, first ? "tran 1e-5 1 uic" : "resume");
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    SpiceSession *rc = spice_session_open(argv[1], message, sizeof(message));
    if (!rc)
        fprintf(stderr, "%s\n", message);
    assert(rc);
    load(rc, "RC memory\nVbattery supply 0 5\nRfeed supply out 1k\nRload out 0 1k\n"
             "Cstore out 0 1u IC=0\n.end\n");
    advance(rc, .005, 1);
    double charged = value(rc, "out");
    assert(charged > 2.49);
    SpiceSession *dff = spice_session_open(argv[1], message, sizeof(message));
    assert(dff);
    load(dff, "DFF memory\nVdata data 0 0\nVclock clock 0 0\nAinput [data clock] [d clk] adc\n"
              "Aff d clk null null qd null ff\nAout [qd] [q] dac\nRload q 0 10k\n"
              ".model adc adc_bridge(in_low=1.5 in_high=3.5)\n"
              ".model dac dac_bridge(out_low=0 out_high=5)\n"
              ".model ff d_dff(ic=0 rise_delay=1n fall_delay=1n)\n.end\n");
    advance(dff, .001, 1);
    command(dff, "alter Vdata = 5");
    advance(dff, .002, 0);
    assert(value(dff, "q") < .1);
    command(dff, "alter Vclock = 5");
    advance(dff, .003, 0);
    assert(value(dff, "q") > 4.9);
    command(dff, "alter Vdata = 0");
    advance(dff, .004, 0);
    assert(value(dff, "q") > 4.9);
    assert(fabs(value(rc, "out") - charged) < 1e-12);
    command(rc, "alter Rfeed = 1e12");
    advance(rc, .006, 0);
    assert(value(rc, "out") > .85 && value(rc, "out") < 1);
    command(dff, "alter Vclock = 0");
    advance(dff, .005, 0);
    command(dff, "alter Vclock = 5");
    advance(dff, .006, 0);
    assert(value(dff, "q") < .1);
    double preserved = value(rc, "out");
    assert(!spice_session_command(rc, "bancada_invalid_command", message, sizeof(message)));
    assert(strstr(message, "no such command"));
    assert(fabs(value(rc, "out") - preserved) < 1e-12);
    spice_session_close(rc);
    spice_session_close(dff);
    puts("Persistent RC/DFF sessions and independent contexts passed.");
    return 0;
}
