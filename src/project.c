#include "project.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

int project_save(const char *path, const Project *project)
{
    char directory[MAX_PATH], temporary[MAX_PATH];
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *separator = slash;
    size_t length;
    FILE *file;
    int i, ok;
    if (backslash && (!separator || backslash > separator)) separator = backslash;
    length = separator ? (size_t)(separator - path + 1) : 0;
    if (length >= sizeof(directory)) return 0;
    if (length) { memcpy(directory, path, length); directory[length] = '\0'; }
    else strcpy(directory, ".");
    if (!GetTempFileNameA(directory, "ele", 0, temporary)) return 0;
    file = fopen(temporary, "w");
    if (!file) { DeleteFileA(temporary); return 0; }
    ok = fprintf(file, "electronics 2\n%d %d %u\n", project->circuit.source_on,
                 project->circuit.switch_closed, project->circuit.disconnected_wires) > 0;
    for (i = 0; i < 3 && ok; ++i)
        ok = fprintf(file, "%.9g %.9g\n", project->blocks[i].x, project->blocks[i].y) > 0;
    if (fclose(file) != 0) ok = 0;
    if (ok) ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileA(temporary);
    return ok;
}

int project_load(const char *path, Project *project)
{
    FILE *file = fopen(path, "r");
    Project loaded = *project;
    int version, i, ok = 1;
    char magic[32], extra;
    if (!file) return 0;
    if (fscanf(file, "%31s %d", magic, &version) != 2 ||
        strcmp(magic, "electronics") != 0 || (version != 1 && version != 2) ||
        fscanf(file, "%d %d", &loaded.circuit.source_on, &loaded.circuit.switch_closed) != 2 ||
        (loaded.circuit.source_on != 0 && loaded.circuit.source_on != 1) ||
        (loaded.circuit.switch_closed != 0 && loaded.circuit.switch_closed != 1)) ok = 0;
    if (ok && version == 1) loaded.circuit.disconnected_wires = 0;
    if (ok && version == 2 &&
        (fscanf(file, "%u", &loaded.circuit.disconnected_wires) != 1 ||
         loaded.circuit.disconnected_wires > 7)) ok = 0;
    for (i = 0; i < 3 && ok; ++i) {
        if (fscanf(file, "%f %f", &loaded.blocks[i].x, &loaded.blocks[i].y) != 2 ||
            !isfinite(loaded.blocks[i].x) || !isfinite(loaded.blocks[i].y) ||
            fabsf(loaded.blocks[i].x) > 1000 || fabsf(loaded.blocks[i].y) > 1000) ok = 0;
    }
    if (ok && fscanf(file, " %c", &extra) != EOF) ok = 0;
    if (ferror(file)) ok = 0;
    fclose(file);
    if (ok) *project = loaded;
    return ok;
}
