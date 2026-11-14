#include "circuit.h"

int circuit_led_on(const Circuit *circuit)
{
    return circuit->source_on && circuit->switch_closed && circuit->disconnected_wires == 0;
}

int circuit_wire_for_terminals(int first, int second)
{
    if (first < 0 || first >= 6 || second < 0 || second >= 6 || first == second)
        return -1;
    if (first / 2 != second / 2) return -1;
    return first / 2;
}

unsigned int circuit_observed_obstacles(const Circuit *circuit)
{
    unsigned int result = 0;
    if (!circuit->source_on) result |= CIRCUIT_SOURCE_OFF;
    if (!circuit->switch_closed) result |= CIRCUIT_SWITCH_OPEN;
    if (circuit->disconnected_wires) result |= CIRCUIT_WIRE_OPEN;
    return result;
}
