#include "workbench_group_edit.h"
#include "workbench_group_name.h"
#include "workbench_file.h"
#include "workbench_pack.h"
#include "spice_process.h"
#include <assert.h>
#include <string.h>

static void test_names(Workbench *bench)
{
    const char *name = "Indicador \xc3\xa1rvore \"A\"";
    assert(bench_group_set_name(bench, 1, name));
    Workbench before = *bench;
    assert(!bench_group_set_name(bench, 1, "   "));
    assert(!bench_group_set_name(bench, 1, "linha\nquebrada"));
    assert(!bench_group_set_name(bench, 1, "\xc0\xaf"));
    assert(!bench_group_set_name(bench, 1, "\xed\xa0\x80"));
    char oversized[BENCH_GROUP_NAME_CAPACITY + 1];
    memset(oversized, 'a', sizeof(oversized) - 1);
    oversized[sizeof(oversized) - 1] = 0;
    assert(!bench_group_set_name(bench, 1, oversized));
    assert(memcmp(&before, bench, sizeof(before)) == 0);
    FILE *file = tmpfile();
    Workbench loaded = {0};
    assert(file && workbench_write(file, bench));
    rewind(file);
    assert(workbench_read(file, &loaded));
    assert(!strcmp(loaded.groups[0].name, name));
    fclose(file);
}

int main(int argc, char **argv)
{
    Workbench parent = {0}, draft = {0};
    BenchSelection selection = {0};
    char message[256];
    bench_add(&parent, PART_SOURCE, 300, 200);
    bench_add(&parent, PART_RESISTOR, 500, 200);
    bench_add(&parent, PART_LED, 700, 200);
    bench_add(&parent, PART_RESISTOR, 900, 400);
    bench_toggle_wire(&parent, (Pin){0, 0}, (Pin){1, 0});
    bench_toggle_wire(&parent, (Pin){1, 1}, (Pin){2, 0});
    bench_toggle_wire(&parent, (Pin){2, 1}, (Pin){0, 1});
    selection.parts[1] = selection.parts[2] = 1;
    assert(bench_pack_selection(&parent, &selection, message, sizeof(message)));
    test_names(&parent);
    Workbench before = parent;
    assert(bench_group_extract(&parent, 1, &draft));
    assert(draft.part_count == 2 && draft.wire_count == 1);
    assert(draft.parts[0].edit_origin == 2 && draft.parts[1].edit_origin == 3);
    assert(!draft.parts[0].group && bench_pin_exposed(&draft.parts[0], 0));
    assert(bench_set_value(&draft, 0, 2200));
    assert(bench_set_scale(&draft, 0, .25f));
    assert(memcmp(&before, &parent, sizeof(parent)) == 0);
    assert(!bench_group_apply(&parent, 1, parent.revision - 1, &draft, "Teste", message, sizeof(message)));
    assert(memcmp(&before, &parent, sizeof(parent)) == 0);

    Workbench replacement = draft;
    bench_remove(&replacement, 0);
    assert(bench_add(&replacement, PART_RESISTOR, 500, 200) == 1);
    assert(!replacement.parts[1].edit_origin);
    bench_toggle_wire(&replacement, (Pin){1, 1}, (Pin){0, 0});
    assert(!bench_group_apply(&parent, 1, parent.revision, &replacement, "Teste", message, sizeof(message)));
    assert(memcmp(&before, &parent, sizeof(parent)) == 0);
    assert(bench_group_apply(&parent, 1, parent.revision, &draft, "Indicador editado", message, sizeof(message)));
    assert(parent.part_count == 4 && parent.parts[0].type == PART_SOURCE && parent.parts[1].value == 1000);
    assert(bench_part_scale(&parent.parts[2]) == .25f);
    assert(parent.parts[2].value == 2200 && parent.parts[3].type == PART_LED);
    assert(parent.wires[0].a.component == 0 && parent.wires[0].b.component == 2);
    assert(parent.wires[1].a.component == 3 && parent.wires[1].b.component == 0);
    assert(!strcmp(parent.groups[0].name, "Indicador editado"));
    assert(!parent.parts[2].edit_origin && !parent.parts[3].edit_origin);
    if (argc > 1)
    {
        double current, currents[WORKBENCH_MAX_COMPONENTS];
        assert(spice_dc_analyze(&parent, argv[1], &current, currents, message, sizeof(message)));
        assert(currents[3] > .001 && currents[3] < .002);
    }
    return 0;
}
