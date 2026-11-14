#ifndef CIRCUIT_H
#define CIRCUIT_H

typedef struct {
    int source_on;
    int switch_closed;
} Circuit;

/* A fixed series circuit: source -> switch -> LED -> source. */
int circuit_led_on(const Circuit *circuit);

#endif
