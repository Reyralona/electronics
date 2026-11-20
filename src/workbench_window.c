#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>
#include <direct.h>
#include <wchar.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "workbench.h"
#include "component_registry.h"
#include "workbench_file.h"
#include "workbench_examples.h"
#include "spice_process.h"
#include "spice_job.h"
#include "spice_trace.h"
#include "workbench_history.h"
#include "workbench_view.h"
#include "sim_auto.h"
#include "dev_capture.h"
#include "app_settings.h"
#include "ui_text.h"
#include "app_state.h"
#include "app_editor.h"
#include "app_text_utf8.h"
#include "app_input.h"
#include "app_render.h"
#include "app_layout.h"
#include "app_simulation.h"
#include "dev_scenarios.h"
#define WINDOW_MULTISAMPLES 4
#define WINDOW_GL_MULTISAMPLE 0x809D /* GL_MULTISAMPLE, OpenGL 1.3 */

static void save_project_thumbnail(AppState *app, GLuint font, int width, int height)
{
    int slot = app->project_thumbnail_pending;
    if (app->selftest || app->editor_parent)
        return;
    if (slot < 0 && app->projects_open)
    {
        for (int cell = 0; cell < PROJECT_SLOTS_PER_PAGE; ++cell)
        {
            int candidate = app->project_page * PROJECT_SLOTS_PER_PAGE + cell;
            if (candidate >= app->project_slot_count || !app->saved_slots[candidate])
                continue;
            char path[128];
            snprintf(path, sizeof(path), "saves/slot-%d.bmp", candidate + 1);
            struct _stat metadata;
            if (_stat(path, &metadata) != 0)
            {
                slot = candidate;
                break;
            }
        }
    }
    if (slot < 0)
        return;
    app->project_thumbnail_pending = -1;
    char project_path[128];
    snprintf(project_path, sizeof(project_path), "saves/slot-%d.elec", slot + 1);
    Workbench saved = {0};
    WorkbenchSettings settings;
    if (!workbench_load_project(project_path, &saved, &settings))
    {
        bench_snapshot_clear(&saved);
        return;
    }
    AppState *preview = malloc(sizeof(*preview));
    if (!preview)
    {
        bench_snapshot_clear(&saved);
        return;
    }
    *preview = *app;
    preview->bench = saved;
    preview->projects_open = 0;
    preview->settings = 0;
    preview->examples_open = 0;
    preview->selected = -1;
    bench_selection_clear(&preview->selection);
    memset(&preview->sim, 0, sizeof(preview->sim));
    memset(preview->leds, 0, sizeof(preview->leds));
    preview->show_handles = 0;
    preview->project_thumbnail_render = 1;
    bench_view_fit_workbench(&preview->view, &preview->bench);
    app_render_draw(preview, font, width, height);
    if (dev_capture_save("build/project-thumbnail-frame.bmp", width, height))
    {
        SDL_Surface *frame = SDL_LoadBMP("build/project-thumbnail-frame.bmp");
        SDL_Surface *thumbnail = SDL_CreateSurface(264, 112, SDL_PIXELFORMAT_RGB24);
        if (frame && thumbnail)
        {
            AppRect viewport = app_layout_viewport(width, height);
            float scale = viewport.w / app_layout_canvas_width();
            SDL_Rect source = {(int)(viewport.x + BENCH_VIEW_LEFT * scale), (int)(viewport.y + BENCH_VIEW_TOP * scale),
                               (int)(BENCH_VIEW_WIDTH * scale), (int)(BENCH_VIEW_HEIGHT * scale)};
            SDL_Rect target = {0, 0, 264, 112};
            if (SDL_BlitSurfaceScaled(frame, &source, thumbnail, &target, SDL_SCALEMODE_LINEAR))
            {
                char path[128];
                snprintf(path, sizeof(path), "saves/slot-%d.bmp", slot + 1);
                if (SDL_SaveBMP(thumbnail, path))
                    app->project_thumbnail_revision++;
            }
        }
        if (frame)
            SDL_DestroySurface(frame);
        if (thumbnail)
            SDL_DestroySurface(thumbnail);
    }
    remove("build/project-thumbnail-frame.bmp");
    free(preview);
    bench_snapshot_clear(&saved);
}

/* ngspice lives next to the executable (build/spice-local); fall back to the path relative to the project root. */
static void find_ngspice(char *path, size_t capacity)
{
    const char *base = SDL_GetBasePath();
    snprintf(path, capacity, "build/spice-local/ucrt64/bin/ngspice_con.exe");
    if (base)
    {
        char candidate[1024];
        snprintf(candidate, sizeof(candidate), "%sspice-local/ucrt64/bin/ngspice_con.exe", base);
        if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
            snprintf(path, capacity, "%s", candidate);
    }
}

/* Distribution markers leave the developer/test working directory unchanged. */
static int distribution_directory(void)
{
    wchar_t executable[MAX_PATH], marker[MAX_PATH], directory[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, executable, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return 0;
    wchar_t *separator = wcsrchr(executable, L'\\');
    if (!separator)
        return 0;
    *separator = 0;
    if (swprintf(marker, MAX_PATH, L"%ls\\installed.mode", executable) < 0)
        return 0;
    if (GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES)
    {
        char *preferences = SDL_GetPrefPath("Bancada", "Bancada");
        if (!preferences)
            return 0;
        int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, preferences, -1, directory, MAX_PATH);
        SDL_free(preferences);
        return converted && _wchdir(directory) == 0;
    }
    if (swprintf(marker, MAX_PATH, L"%ls\\portable.mode", executable) < 0)
        return 0;
    if (GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES)
        return _wchdir(executable) == 0;
    return 1;
}

static void set_window_icon(SDL_Window *window)
{
    SDL_Surface *icon = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32);
    if (!icon)
        return;
    Uint32 background = SDL_MapSurfaceRGBA(icon, 18, 31, 44, 255);
    Uint32 body = SDL_MapSurfaceRGBA(icon, 36, 158, 165, 255);
    Uint32 pins = SDL_MapSurfaceRGBA(icon, 230, 239, 242, 255);
    SDL_FillSurfaceRect(icon, NULL, background);
    SDL_Rect rectangle = {8, 8, 16, 16};
    SDL_FillSurfaceRect(icon, &rectangle, body);
    for (int pin = 0; pin < 3; ++pin)
    {
        rectangle = (SDL_Rect){3, 10 + pin * 5, 6, 2};
        SDL_FillSurfaceRect(icon, &rectangle, pins);
        rectangle.x = 23;
        SDL_FillSurfaceRect(icon, &rectangle, pins);
    }
    rectangle = (SDL_Rect){12, 12, 8, 2};
    SDL_FillSurfaceRect(icon, &rectangle, pins);
    rectangle = (SDL_Rect){15, 12, 2, 8};
    SDL_FillSurfaceRect(icon, &rectangle, pins);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

/* Value of a "--name=value" option anywhere after the first argument, or NULL. */
static const char *option_value(int argc, char **argv, const char *prefix)
{
    size_t length = strlen(prefix);
    for (int i = 1; i < argc; ++i)
        if (!strncmp(argv[i], prefix, length))
            return argv[i] + length;
    return NULL;
}

/* "--language=en-US" overrides the saved preference for this run (captures, reviews). */
static void apply_language_option(int argc, char **argv)
{
    const char *code = option_value(argc, argv, "--language=");
    for (int language = 0; code && language < UI_LANG_COUNT; ++language)
        if (!strcmp(code, ui_text_language_code((UiLanguage)language)))
            ui_text_set_language((UiLanguage)language);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--print-data-directory"))
    {
        if (!distribution_directory())
            return 1;
        wchar_t directory[MAX_PATH];
        if (!_wgetcwd(directory, MAX_PATH))
            return 1;
        char utf8[3 * MAX_PATH];
        if (!WideCharToMultiByte(CP_UTF8, 0, directory, -1, utf8, sizeof(utf8), NULL, NULL))
            return 1;
        printf("%s\n", utf8);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--list-components"))
    {
        for (size_t i = 0; i < component_registry_count(); ++i)
        {
            const ComponentInfo *info = component_info((PartType)i);
            if (info)
                printf("%s|%s|%u\n", info->id, info->display_name, info->flags);
        }
        return 0;
    }
    SDL_Window *window;
    SDL_GLContext context;
    GLuint font;
    static AppState app;
    app_state_init(&app);
    app.selftest = argc > 1 && strcmp(argv[1], "--self-test-history") == 0;
    app.capture = argc > 1 && dev_capture_requested(argv[1]);
    if (!app.selftest && !app.capture && !distribution_directory())
    {
        wchar_t detail[256];
        if (MultiByteToWideChar(CP_UTF8, 0, tr(TXT_UI_DATA_DIRECTORY_FAILED), -1, detail, 256))
            MessageBoxW(NULL, detail, L"Bancada", MB_OK | MB_ICONERROR);
        app_state_dispose(&app);
        return 1;
    }
    app.auto_capture =
        app.capture && (strcmp(argv[1], "--capture-burn") == 0 || strncmp(argv[1], "--capture-auto", 14) == 0);
    app.sim.auto_enabled = (!app.selftest && !app.capture) || app.auto_capture;
    find_ngspice(app.ngspice, sizeof(app.ngspice));
    if (!app.selftest && !app.capture)
        app_settings_load_preferences(&app);
    apply_language_option(argc, argv);
    app.status = tr(TXT_APP_START);

    if (dev_scenario_prepare(&app, argc, argv))
        return 1;
    if (!app.history)
    {
        fprintf(stderr, "Memoria insuficiente para historico.\n");
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        app_state_dispose(&app);
        return 1;
    }
    Uint64 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (app.selftest)
        window_flags |= SDL_WINDOW_HIDDEN;
    /* 4x multisampling smooths circles, rounded corners and diagonal wires; drivers without it get none. */
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, WINDOW_MULTISAMPLES);
    window = SDL_CreateWindow("Bancada", 1280, 720, window_flags);
    if (!window)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        window = SDL_CreateWindow("Bancada", 1280, 720, window_flags);
    }
    if (!window)
    {
        fprintf(stderr, "Janela: %s\n", SDL_GetError());
        SDL_Quit();
        app_state_dispose(&app);
        return 1;
    }
    set_window_icon(window);
    context = SDL_GL_CreateContext(window);
    if (context)
        glEnable(WINDOW_GL_MULTISAMPLE);
    if (!context)
    {
        fprintf(stderr, "OpenGL: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        app_state_dispose(&app);
        return 1;
    }
    font = app_render_font_create();
    /* A saved scale larger than the monitor opens maximized instead of losing the preference. */
    if (!app_input_apply_scale(&app, window) && !SDL_MaximizeWindow(window))
        app.status = tr(TXT_UI_WINDOW_SIZE_FAILED);
    int window_scale = app.gui_font_size;
    SDL_GL_SetSwapInterval(1);
    if (dev_scenario_analyze(&app, argv))
        return 1;
    AppState *active = &app;
    while (app.running)
    {
        int width, height;
        if (app.selftest)
            dev_scenario_queue(&app, window);
        app_input_poll(active, window);
        active = app_editor_route(active);
        if (active->gui_font_size != window_scale)
        {
            if (!app_input_apply_scale(active, window))
                active->gui_font_size = window_scale;
            window_scale = active->gui_font_size;
        }
        if (app.capture && !strcmp(argv[1], "--capture-editor-deep") && active->editor_parent)
        {
            active->selected = 0;
            AppState *inner = app_editor_open(active);
            if (inner)
                active = inner;
        }
        if (app.capture && !strcmp(argv[1], "--capture-editor-name") && active->editor_parent)
            app_editor_begin_name(active, window);
        if (app.selftest)
            dev_scenario_validate(&app);
        app_simulation_update(active);
        app_settings_autosave(&app);
        SDL_GetWindowSizeInPixels(window, &width, &height);
        app_layout_set_canvas((float)width, (float)height,
                              app_input_window_scale(active, window) * SDL_GetWindowPixelDensity(window));
        save_project_thumbnail(active, font, width, height);
        app_render_draw(active, font, width, height);
        if (app.capture)
        {
            const char *destination = "build/bancada-capture.bmp";
            if (option_value(argc, argv, "--capture-output="))
                destination = option_value(argc, argv, "--capture-output=");
            if (!dev_capture_save(destination, width, height))
            {
                fprintf(stderr, "Captura: %s\n", SDL_GetError());
                app.exit_code = 1;
            }
            app.running = 0;
        }
        SDL_GL_SwapWindow(window);
    }
    while (active->editor_parent)
        active = app_editor_dispose(active);
    spice_job_join(&app.dc_job);
    if (app.selftest)
    {
        remove(APP_TEST_PATH(APP_TEST_BLOCKS, "block-1.elec"));
    }
    app_state_dispose(&app);
    app_text_utf8_clear();
    if (font)
        glDeleteLists(font, 128);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return app.exit_code;
}
