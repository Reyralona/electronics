#include <assert.h>
#include "simulation.h"

int main(void)
{
    Circuit circuit = {1, 0, 0};
    Simulation simulation = {1, 0};
    simulation_update(&simulation, &circuit);
    assert(!simulation.led_on);
    circuit.switch_closed = 1;
    simulation_update(&simulation, &circuit);
    assert(simulation.led_on);
    simulation.running = 0;
    circuit.disconnected_wires = 1;
    simulation_update(&simulation, &circuit);
    assert(simulation.led_on);
    simulation.running = 1;
    simulation_update(&simulation, &circuit);
    assert(!simulation.led_on);
    return 0;
}
