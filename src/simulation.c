#include "simulation.h"

void simulation_update(Simulation *simulation, const Circuit *circuit)
{
    if (simulation->running) simulation->led_on = circuit_led_on(circuit);
}

void simulation_step(Simulation *simulation, const Circuit *circuit)
{
    simulation->led_on = circuit_led_on(circuit);
}
