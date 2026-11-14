#ifndef SIMULATION_H
#define SIMULATION_H
#include "circuit.h"

typedef struct { int running; int led_on; } Simulation;
void simulation_update(Simulation *simulation, const Circuit *circuit);
#endif
