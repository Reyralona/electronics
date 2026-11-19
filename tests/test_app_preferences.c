#include "app_preferences.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static FILE *text_file(const char *content)
{
    FILE *file = tmpfile();
    assert(file);
    fputs(content, file);
    rewind(file);
    return file;
}

static void roundtrip(void)
{
    WorkbenchSettings original = workbench_settings_default();
    original.dark_theme = 1;
    original.gui_font_size = 18;
    original.grid_spacing = 12.5;
    original.transient_step = 2e-5;
    original.simulation_speed = .25;
    original.wire_color = 3;
    FILE *file = tmpfile();
    assert(file && app_preferences_write(file, &original));
    rewind(file);
    WorkbenchSettings loaded = workbench_settings_default();
    assert(app_preferences_read(file, &loaded));
    fclose(file);
    assert(app_preferences_equal(&original, &loaded));
}

/* Preferences saved before the shared palette convert the default wire color. */
static void legacy_wire_color(void)
{
    FILE *file = text_file("bancada-preferences 1\nwire_color 2\n");
    WorkbenchSettings loaded = workbench_settings_default();
    assert(app_preferences_read(file, &loaded));
    fclose(file);
    assert(loaded.wire_color == BENCH_COLOR_BLACK);
    file = text_file("bancada-preferences 2\nwire_color 9\n");
    assert(app_preferences_read(file, &loaded));
    fclose(file);
    assert(loaded.wire_color == BENCH_COLOR_LIME);
}

/* An unknown language index is rejected without touching the other settings. */
static void invalid_language(void)
{
    FILE *file = text_file("bancada-preferences 1\ndark_theme 1\nlanguage 7\n");
    WorkbenchSettings loaded = workbench_settings_default();
    assert(!app_preferences_read(file, &loaded));
    fclose(file);
    assert(loaded.language == 0 && loaded.dark_theme == 0);
}

static void tolerant_keys(void)
{
    FILE *file = text_file("bancada-preferences 2\nfuture_option 7 extra words\ndark_theme 1\n");
    WorkbenchSettings loaded = workbench_settings_default();
    assert(app_preferences_read(file, &loaded));
    fclose(file);
    WorkbenchSettings expected = workbench_settings_default();
    expected.dark_theme = 1;
    assert(app_preferences_equal(&expected, &loaded));
}

static void rejects_invalid(const char *content)
{
    FILE *file = text_file(content);
    WorkbenchSettings target = workbench_settings_default();
    target.gui_font_size = 17;
    WorkbenchSettings before = target;
    assert(!app_preferences_read(file, &target));
    fclose(file);
    assert(app_preferences_equal(&before, &target) && target.gui_font_size == 17);
}

static void save_replaces_file(void)
{
    const char *path = "build/test-preferences.txt";
    WorkbenchSettings first = workbench_settings_default(), second = first, loaded = first;
    second.snap_grid = 1;
    assert(app_preferences_save(path, &first));
    assert(app_preferences_save(path, &second));
    assert(app_preferences_load(path, &loaded) && app_preferences_equal(&second, &loaded));
    WorkbenchSettings invalid = second;
    invalid.gui_font_size = 99;
    assert(!app_preferences_save(path, &invalid));
    assert(app_preferences_load(path, &loaded) && app_preferences_equal(&second, &loaded));
    remove(path);
    assert(!app_preferences_load(path, &loaded));
}

int main(void)
{
    roundtrip();
    legacy_wire_color();
    invalid_language();
    tolerant_keys();
    rejects_invalid("not-preferences 1\n");
    rejects_invalid("bancada-preferences 1\ngui_font_size 99\n");
    rejects_invalid("bancada-preferences 1\ngrid_spacing banana\n");
    rejects_invalid("bancada-preferences 1\ntransient_step 1\ntransient_stop 1e-9\n");
    rejects_invalid("bancada-preferences 1\nsimulation_speed 0\n");
    rejects_invalid("bancada-preferences 1\nsimulation_speed 0.0005\n");
    rejects_invalid("bancada-preferences 1\nsimulation_speed 2\n");
    save_replaces_file();
    puts("Preferences roundtrip, tolerant keys, invalid files and atomic replacement passed.");
    return 0;
}
