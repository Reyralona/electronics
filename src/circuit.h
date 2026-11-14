#ifndef CIRCUIT_H
#define CIRCUIT_H

typedef struct {
    int source_on;
    int switch_closed;
    unsigned int disconnected_wires; /* Bits 0..2: source-switch, switch-LED, return. */
} Circuit;

/* A fixed series circuit: source -> switch -> LED -> source. */
int circuit_led_on(const Circuit *circuit);

#endif
