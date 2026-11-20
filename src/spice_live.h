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
/* Reply limit for each solver request; 0 keeps SPICE_SESSION_TIMEOUT_MS. A failed batch closes the
 * session, and the next batch restarts it from t = 0 (reported by spice_live_restarted). */
void spice_live_set_timeout_ms(SpiceLive *live, unsigned milliseconds);
double spice_live_time(const SpiceLive *live);
int spice_live_sync_memory(SpiceLive *live, Workbench *bench, char *message, size_t capacity);
int spice_live_voltage(SpiceLive *live, const Workbench *bench, Pin pin, double *voltage,
                       char *message, size_t capacity);
#endif
