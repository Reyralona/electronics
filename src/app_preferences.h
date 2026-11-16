#ifndef APP_PREFERENCES_H
#define APP_PREFERENCES_H
#include <stdio.h>
#include "workbench_file.h"
/* Application preferences persisted between sessions as "key value" lines after a
 * "bancada-preferences 1" header. Unknown keys are ignored and missing keys keep defaults, so
 * older and newer files stay readable. Pure module: no SDL/OpenGL. */
#define APP_PREFERENCES_VERSION 1
int app_preferences_write(FILE *file, const WorkbenchSettings *settings);
/* Transactional: on failure *settings is unchanged. */
int app_preferences_read(FILE *file, WorkbenchSettings *settings);
/* Writes a temporary file and replaces the target, so a failed save keeps the previous file. */
int app_preferences_save(const char *path, const WorkbenchSettings *settings);
int app_preferences_load(const char *path, WorkbenchSettings *settings);
int app_preferences_equal(const WorkbenchSettings *a, const WorkbenchSettings *b);
#endif
