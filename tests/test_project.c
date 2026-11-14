#include <assert.h>
#include <stdio.h>
#include "project.h"

int main(void)
{
    const char *path = "build/test-project.elec";
    Project original = {{1, 1, 5}, {{1.25f, -2.0f, .3f, .3f}, {0, 0, .3f, .24f}, {2, 3, .3f, .3f}}};
    Project loaded = {{0, 0, 0}, {{0, 0, .3f, .3f}, {0, 0, .3f, .24f}, {0, 0, .3f, .3f}}};
    FILE *file;
    assert(project_save(path, &original));
    assert(project_load(path, &loaded));
    assert(loaded.circuit.source_on == 1 && loaded.circuit.switch_closed == 1);
    assert(loaded.circuit.disconnected_wires == 5);
    assert(loaded.blocks[0].x == 1.25f && loaded.blocks[0].y == -2.0f);
    assert(loaded.blocks[2].x == 2 && loaded.blocks[2].y == 3);
    file = fopen(path, "w");
    assert(file);
    fputs("electronics 1\n1 1\n1.25 -2\n0 0\n2 3\n", file);
    assert(fclose(file) == 0);
    assert(project_load(path, &loaded));
    assert(loaded.circuit.disconnected_wires == 0);
    file = fopen(path, "w");
    assert(file);
    fputs("electronics 999\n1 1\n", file);
    assert(fclose(file) == 0);
    assert(!project_load(path, &loaded));
    assert(loaded.circuit.source_on == 1 && loaded.blocks[0].x == 1.25f);
    assert(remove(path) == 0);
    return 0;
}
