#include "app_preferences.h"
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    FIELD_INT,
    FIELD_DOUBLE
} FieldKind;

typedef struct
{
    const char *key;
    FieldKind kind;
    size_t offset;
} PreferenceField;

#define INT_FIELD(name) {#name, FIELD_INT, offsetof(WorkbenchSettings, name)}
#define DOUBLE_FIELD(name) {#name, FIELD_DOUBLE, offsetof(WorkbenchSettings, name)}
static const PreferenceField fields[] = {
    INT_FIELD(dark_theme),     INT_FIELD(gui_font_size),     INT_FIELD(show_grid),
    INT_FIELD(show_handles),   INT_FIELD(round_wires),       INT_FIELD(show_current),
    INT_FIELD(wire_color),     INT_FIELD(probe_pin),         INT_FIELD(probe_current),
    INT_FIELD(snap_grid),      INT_FIELD(snap_board),        DOUBLE_FIELD(grid_spacing),
    INT_FIELD(show_pin_labels), INT_FIELD(language),
    DOUBLE_FIELD(transient_step), DOUBLE_FIELD(transient_stop),
};
#define FIELD_COUNT (sizeof(fields) / sizeof(fields[0]))
#define KEY_CAPACITY 64

static const PreferenceField *find_field(const char *key)
{
    for (size_t i = 0; i < FIELD_COUNT; ++i)
        if (!strcmp(fields[i].key, key))
            return &fields[i];
    return NULL;
}

int app_preferences_write(FILE *file, const WorkbenchSettings *settings)
{
    if (!file || !settings || !workbench_settings_valid(settings))
        return 0;
    if (fprintf(file, "bancada-preferences %d\n", APP_PREFERENCES_VERSION) < 0)
        return 0;
    const unsigned char *base = (const unsigned char *)settings;
    for (size_t i = 0; i < FIELD_COUNT; ++i)
    {
        int written;
        if (fields[i].kind == FIELD_INT)
            written = fprintf(file, "%s %d\n", fields[i].key, *(const int *)(base + fields[i].offset));
        else
            written = fprintf(file, "%s %.17g\n", fields[i].key, *(const double *)(base + fields[i].offset));
        if (written < 0)
            return 0;
    }
    return 1;
}

static int read_value(FILE *file, const PreferenceField *field, WorkbenchSettings *draft)
{
    unsigned char *base = (unsigned char *)draft;
    if (field->kind == FIELD_INT)
        return fscanf(file, "%d", (int *)(base + field->offset)) == 1;
    return fscanf(file, "%lf", (double *)(base + field->offset)) == 1;
}

static int skip_line(FILE *file)
{
    int c;
    while ((c = fgetc(file)) != EOF && c != '\n')
        ;
    return 1;
}

int app_preferences_read(FILE *file, WorkbenchSettings *settings)
{
    char magic[KEY_CAPACITY];
    int version;
    if (!file || !settings || fscanf(file, "%63s %d", magic, &version) != 2 ||
        strcmp(magic, "bancada-preferences") || version < 1)
        return 0;
    WorkbenchSettings draft = workbench_settings_default();
    char key[KEY_CAPACITY];
    while (fscanf(file, "%63s", key) == 1)
    {
        const PreferenceField *field = find_field(key);
        if (field ? !read_value(file, field, &draft) : !skip_line(file))
            return 0;
    }
    if (!feof(file) || !workbench_settings_valid(&draft))
        return 0;
    *settings = draft;
    return 1;
}

int app_preferences_save(const char *path, const WorkbenchSettings *settings)
{
    char temporary[MAX_PATH];
    if (!path || snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary))
        return 0;
    FILE *file = fopen(temporary, "w");
    if (!file)
        return 0;
    int written = app_preferences_write(file, settings);
    if (fclose(file) || !written)
    {
        remove(temporary);
        return 0;
    }
    if (!MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        remove(temporary);
        return 0;
    }
    return 1;
}

int app_preferences_load(const char *path, WorkbenchSettings *settings)
{
    FILE *file = path ? fopen(path, "r") : NULL;
    if (!file)
        return 0;
    int loaded = app_preferences_read(file, settings);
    fclose(file);
    return loaded;
}

int app_preferences_equal(const WorkbenchSettings *a, const WorkbenchSettings *b)
{
    const unsigned char *left = (const unsigned char *)a, *right = (const unsigned char *)b;
    for (size_t i = 0; i < FIELD_COUNT; ++i)
    {
        size_t offset = fields[i].offset;
        if (fields[i].kind == FIELD_INT && *(const int *)(left + offset) != *(const int *)(right + offset))
            return 0;
        if (fields[i].kind == FIELD_DOUBLE && *(const double *)(left + offset) != *(const double *)(right + offset))
            return 0;
    }
    return 1;
}
