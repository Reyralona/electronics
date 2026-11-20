#ifndef SPICE_SESSION_H
#define SPICE_SESSION_H
#include <stddef.h>
#include "spice_session_protocol.h"
#include "rom_image.h"
/* Regularly spaced samples of the probed vector since the previous request. */
typedef struct
{
    double start, interval;
    int count;
    double values[SPICE_SESSION_MAX_SAMPLES];
} SpiceSampleBlock;
typedef struct SpiceSession SpiceSession;
/* Each session owns an isolated child process. All calls belong to one worker at a time. */
SpiceSession *spice_session_open(const char *executable, char *message, size_t capacity);
int spice_session_load(SpiceSession *session, const char *path, char *message, size_t capacity);
int spice_session_command(SpiceSession *session, const char *command, char *message, size_t capacity);
int spice_session_value(SpiceSession *session, const char *vector, double *value, char *message, size_t capacity);
/* Samples `vector` every `interval` simulated seconds; NULL or "" stops sampling. */
int spice_session_probe(SpiceSession *session, const char *vector, double interval, char *message, size_t capacity);
int spice_session_samples(SpiceSession *session, SpiceSampleBlock *block, char *message, size_t capacity);
int spice_session_eeprom(SpiceSession *session, int *part, RomImage *image, char *message, size_t capacity);
/* Reply limit for later requests; 0 restores SPICE_SESSION_TIMEOUT_MS. */
void spice_session_set_timeout(SpiceSession *session, unsigned milliseconds);
void spice_session_close(SpiceSession *session);
#endif
