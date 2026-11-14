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
