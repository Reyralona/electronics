#ifndef PROJECT_H
#define PROJECT_H
#include "circuit.h"

typedef struct { float x, y, w, h; } Block;
typedef struct { Circuit circuit; Block blocks[3]; } Project;

int project_save(const char *path, const Project *project);
int project_load(const char *path, Project *project);
#endif
