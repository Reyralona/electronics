/* Chip library from a person's point of view: each step is something a user does in the GUI and
 * each assertion is what they expect to see in the Blocos tab afterwards. Reported 16/11/2026:
 * creating a chip, entering it, renaming and saving added a second library entry instead of
 * updating the chip that had just been created. */
#include "app_editor.h"
#include "app_packing.h"
#include "workbench_file.h"
#include <assert.h>
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static const char *const library_dir = "build/test-blocks";

static void empty_library(void)
{
    for (int i = 1; i <= BLOCK_LIBRARY_SLOTS; ++i)
    {
        char path[128];
        snprintf(path, sizeof(path), "%s/block-%d.elec", library_dir, i);
        remove(path);
    }
}

/* What the Blocos tab shows: how many chips are listed. */
static int library_count(AppState *app)
{
    app_packing_refresh(app);
    int count = 0;
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        count += app->library_available[i];
    return count;
}

/* Whether a chip with this exact name is listed. */
static int library_lists(AppState *app, const char *name)
{
    app_packing_refresh(app);
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        if (app->library_available[i] && !strcmp(app->library_names[i], name))
            return 1;
    return 0;
}

static int part_in_group(const Workbench *bench, int group)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group == group)
            return i;
    return -1;
}

/* A person picks parts by what they see, not by index: applying a chip reorders the bench. */
static int loose_part(const Workbench *bench, PartType type)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == type && !bench->parts[i].group)
            return i;
    return -1;
}

/* Select a loose resistor and LED and press Ctrl+G. Returns the new chip's group on the bench. */
static int make_chip(AppState *app)
{
    int resistor = loose_part(&app->bench, PART_RESISTOR), led = loose_part(&app->bench, PART_LED);
    assert(resistor >= 0 && led >= 0);
    bench_selection_clear(&app->selection);
    app->selection.parts[resistor] = app->selection.parts[led] = 1;
    assert(app_packing_key(app, SDLK_G, SDL_KMOD_CTRL));
    int group = app->bench.parts[resistor].group;
    assert(group);
    return group;
}

/* Click the chip on the bench and press Enter to go inside it. */
static AppState *enter_chip(AppState *app, int group)
{
    app->selected = part_in_group(&app->bench, group);
    assert(app->selected >= 0);
    assert(app_editor_key(app, NULL, SDLK_RETURN, 0));
    AppState *inside = app_editor_route(app);
    assert(inside != app && inside->editor_parent == app);
    return inside;
}

/* Rename (F4 / Renomear, type, Enter) and press "Salvar modelo". */
static void rename_and_save(AppState *inside, const char *name)
{
    assert(app_editor_set_name(inside, name));
    assert(app_editor_save(inside));
}

/* "Voltar" without applying; the dialog is answered "Descartar" if anything is unsaved. */
static void leave_chip(AppState *inside)
{
    AppState *parent = inside->editor_parent;
    app_editor_request_close(inside, 0);
    if (inside->editor_confirm_close)
        app_editor_confirm(inside, 1);
    assert(app_editor_route(inside) == parent);
}

/* Save the bench to a project file and open it again, as after closing and reopening the program. */
static void reopen_project(AppState *app)
{
    const char *folder = "build/test-slots";
    const char *path = "build/test-slots/chip-flow.elec";
    assert(_mkdir(folder) == 0 || errno == EEXIST);
    WorkbenchSettings settings = workbench_settings_default();
    assert(workbench_save_project(path, &app->bench, &settings));
    assert(workbench_load_project(path, &app->bench, &settings));
    remove(path);
}

static void build_bench(AppState *app)
{
    bench_add(&app->bench, PART_SOURCE, 300, 200);
    bench_add(&app->bench, PART_RESISTOR, 500, 200);
    bench_add(&app->bench, PART_LED, 700, 200);
    bench_add(&app->bench, PART_RESISTOR, 500, 400);
    bench_add(&app->bench, PART_LED, 700, 400);
    assert(bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){2, 0}) == 1);
    assert(bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){3, 0}) == 1);
    assert(bench_toggle_wire(&app->bench, (Pin){3, 1}, (Pin){4, 0}) == 1);
}

int main(void)
{
    static AppState app;
    app_state_init(&app);
    assert(app.history);
    app.selftest = 1;
    app.sim.sim_running = 0;
    empty_library();
    build_bench(&app);

    /* "Crio o chip": Ctrl+G lists it once under its default name. */
    int chip = make_chip(&app);
    assert(library_count(&app) == 1);
    char default_name[BENCH_GROUP_NAME_CAPACITY];
    snprintf(default_name, sizeof(default_name), "%s", app.bench.groups[chip - 1].name);
    assert(library_lists(&app, default_name));

    /* "Entro nele, renomeio e salvo": the same entry is renamed; nothing new appears. */
    AppState *inside = enter_chip(&app, chip);
    rename_and_save(inside, "4AND");
    assert(library_count(inside) == 1);
    assert(library_lists(inside, "4AND") && !library_lists(inside, default_name));

    /* "Mudo um valor e salvo de novo": still one entry. */
    assert(bench_set_value(&inside->bench, 0, 2200));
    assert(app_editor_save(inside));
    assert(library_count(inside) == 1);

    /* "Renomeio e saio respondendo Salvar no aviso": still one entry, with the newest name. */
    assert(app_editor_set_name(inside, "4AND final"));
    app_editor_request_close(inside, 0);
    assert(inside->editor_confirm_close);
    app_editor_confirm(inside, 0);
    assert(app_editor_route(inside) == &app);
    assert(library_count(&app) == 1 && library_lists(&app, "4AND final"));

    /* "Entro de novo sem ter aplicado, salvo": the bench chip still has its old name, but it is the
     * same chip, so the same entry is updated. */
    inside = enter_chip(&app, chip);
    rename_and_save(inside, "4AND v2");
    assert(app_editor_apply(inside));
    assert(app_editor_route(inside) == &app);
    assert(library_count(&app) == 1 && library_lists(&app, "4AND v2"));
    assert(!strcmp(app.bench.groups[chip - 1].name, "4AND v2"));

    /* "Crio OUTRO chip": a different chip must get its own entry. */
    int other = make_chip(&app);
    assert(other != chip);
    assert(library_count(&app) == 2);

    /* "Coloco uma copia do 4AND da lista, entro e salvo": it is the 4AND model, no new entry. */
    int slot_4and = -1;
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        if (app.library_available[i] && !strcmp(app.library_names[i], "4AND v2"))
            slot_4and = i;
    assert(slot_4and >= 0);
    app_packing_choose(&app, slot_4and);
    assert(app.packed_placing);
    int before_place = app.bench.part_count;
    app_packing_place(&app, 1100, 500);
    int copy = app.bench.parts[before_place].group;
    assert(copy && copy != chip && copy != other);
    inside = enter_chip(&app, copy);
    rename_and_save(inside, "4AND v3");
    leave_chip(inside);
    assert(library_count(&app) == 2 && library_lists(&app, "4AND v3"));

    /* "Fecho e reabro o projeto, entro na copia e salvo": the copy on the bench is still called
     * "4AND v2" while the list says "4AND v3"; it is still the same chip, so no new entry. */
    reopen_project(&app);
    inside = enter_chip(&app, copy);
    assert(!strcmp(inside->editor_name, "4AND v2"));
    assert(bench_set_value(&inside->bench, 0, 3300));
    assert(app_editor_save(inside));
    leave_chip(inside);
    assert(library_count(&app) == 2 && library_lists(&app, "4AND v2") && !library_lists(&app, "4AND v3"));

    /* "Abro um projeto salvo por uma versao anterior" (chips without identity): the chip whose name
     * is listed exactly once is that entry. */
    char other_name[BENCH_GROUP_NAME_CAPACITY];
    snprintf(other_name, sizeof(other_name), "%s", app.bench.groups[other - 1].name);
    app.bench.groups[other - 1].model = 0;
    inside = enter_chip(&app, other);
    rename_and_save(inside, "Indicador");
    leave_chip(inside);
    assert(library_count(&app) == 2 && library_lists(&app, "Indicador") && !library_lists(&app, other_name));

    /* "Apago o 4AND da lista, crio um chip novo (ele ocupa o lugar livre) e depois salvo a copia
     * antiga do 4AND": the new chip must not be overwritten; the 4AND comes back as its own entry. */
    slot_4and = -1;
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        if (app.library_available[i] && !strcmp(app.library_names[i], "4AND v2"))
            slot_4and = i;
    assert(slot_4and >= 0);
    assert(!app_packing_delete(&app, slot_4and));
    assert(app_packing_delete(&app, slot_4and));
    assert(library_count(&app) == 1);
    bench_add(&app.bench, PART_RESISTOR, 500, 600);
    bench_add(&app.bench, PART_LED, 700, 600);
    int fresh = make_chip(&app);
    assert(library_count(&app) == 2);
    char fresh_name[BENCH_GROUP_NAME_CAPACITY];
    snprintf(fresh_name, sizeof(fresh_name), "%s", app.bench.groups[fresh - 1].name);
    inside = enter_chip(&app, copy);
    assert(app_editor_save(inside));
    leave_chip(inside);
    assert(library_count(&app) == 3);
    assert(library_lists(&app, fresh_name) && library_lists(&app, "4AND v2"));

    empty_library();
    app_state_dispose(&app);
    puts("Chip library flow (create, enter, rename, save, reopen, delete) passed.");
    return 0;
}
