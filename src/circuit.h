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

#endif
