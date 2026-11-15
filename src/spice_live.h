#ifndef SPICE_LIVE_H
#define SPICE_LIVE_H
#include "sim_auto.h"
typedef struct SpiceLive SpiceLive;
SpiceLive *spice_live_create(void);
void spice_live_destroy(SpiceLive *live);
/* Advances an existing transient. Control/value edits retain the solver state.
 * Topology and clock-period changes explicitly rebuild the session. */
int spice_live_step(SpiceLive *live, const Workbench *bench, const char *executable,
                    const SimAutoOptions *options, double span, SimAutoResult *result);
int spice_live_restarted(const SpiceLive *live);
double spice_live_time(const SpiceLive *live);
int spice_live_voltage(SpiceLive *live, const Workbench *bench, Pin pin, double *voltage,
                       char *message, size_t capacity);
#endif
