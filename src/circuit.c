#include "circuit.h"

int circuit_led_on(const Circuit *circuit)
{
    return circuit->source_on && circuit->switch_closed && circuit->disconnected_wires == 0;
}
