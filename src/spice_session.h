#ifndef SPICE_SESSION_H
#define SPICE_SESSION_H
#include <stddef.h>
typedef struct SpiceSession SpiceSession;
/* Each session owns an isolated child process. All calls belong to one worker at a time. */
SpiceSession *spice_session_open(const char *executable, char *message, size_t capacity);
int spice_session_load(SpiceSession *session, const char *path, char *message, size_t capacity);
int spice_session_command(SpiceSession *session, const char *command, char *message, size_t capacity);
int spice_session_value(SpiceSession *session, const char *vector, double *value, char *message, size_t capacity);
void spice_session_close(SpiceSession *session);
#endif
