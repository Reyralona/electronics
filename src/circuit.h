#ifndef CIRCUIT_H
#define CIRCUIT_H

typedef struct {
    int source_on;
    int switch_closed;
    unsigned int disconnected_wires; /* Bits 0..2: source-switch, switch-LED, return. */
} Circuit;

/* A fixed series circuit: source -> switch -> LED -> source. */
int circuit_led_on(const Circuit *circuit);

/* The prototype supports three predefined pairs of terminals, numbered 0..5. */
int circuit_wire_for_terminals(int first, int second);

enum { CIRCUIT_SOURCE_OFF = 1, CIRCUIT_SWITCH_OPEN = 2, CIRCUIT_WIRE_OPEN = 4 };
/* Reports observed states, not inferred electrical faults. */
unsigned int circuit_observed_obstacles(const Circuit *circuit);

#endif
