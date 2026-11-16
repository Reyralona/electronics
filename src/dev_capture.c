#include "dev_capture.h"
#include <SDL3/SDL_opengl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int dev_capture_requested(const char *argument)
{
    static const char *modes[] = {
        "--capture-live-rc", "--capture-live-off",
        "--capture-catalog", "--capture-leads", "--capture-environment",
        "--capture-inspector", "--capture-inspector-minimized",
        "--capture-dark",
        "--capture-scale",
        "--capture-knee-edit",
        "--capture-editor", "--capture-editor-name", "--capture-packed-rotated",
        "--capture-pins",       "--capture-pin-led",       "--capture-packed",     "--capture",
        "--capture-settings",   "--capture-projects", "--capture-examples",      "--capture-selection",  "--capture-analog",
        "--capture-dc",         "--capture-current",       "--capture-edit",       "--capture-zoom",
        "--capture-snap",       "--capture-board",         "--capture-rotation",   "--capture-colors",
        "--capture-trace",      "--capture-trace-current", "--capture-trace-wire", "--capture-and",
        "--capture-logic",      "--capture-clock",         "--capture-play",       "--capture-led-on",
        "--capture-led-off",    "--capture-dff",           "--capture-ci",         "--capture-example0",
        "--capture-example1",   "--capture-example2",      "--capture-burn",       "--capture-auto",
        "--capture-auto-clock", "--capture-zoomout",
    };
    if (!argument)
        return 0;
    if (!strncmp(argument, "--capture-palette=", 18) ||
        !strncmp(argument, "--capture-component=", 20) || !strncmp(argument, "--capture-component-dark=", 25) ||
        !strncmp(argument, "--capture-example=", 18))
        return 1;
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
    {
        if (strcmp(argument, modes[i]) == 0)
            return 1;
    }
    return 0;
}

int dev_capture_save(const char *path, int width, int height)
{
    unsigned char *pixels = NULL, *flipped = NULL;
    SDL_Surface *surface = NULL;
    int saved = 0;
    size_t stride, bytes;
    if (!path || width <= 0 || height <= 0 || width > INT32_MAX / 3)
    {
        SDL_SetError("Dimensoes invalidas para captura.");
        return 0;
    }
    stride = (size_t)width * 3;
    if ((size_t)height > SIZE_MAX / stride)
    {
        SDL_SetError("Captura excede capacidade de memoria.");
        return 0;
    }
    bytes = stride * (size_t)height;
    pixels = malloc(bytes);
    flipped = malloc(bytes);
    if (!pixels || !flipped)
    {
        SDL_SetError("Sem memoria para captura.");
        goto cleanup;
    }
    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    for (int row = 0; row < height; ++row)
    {
        memcpy(flipped + (size_t)row * stride, pixels + (size_t)(height - row - 1) * stride, stride);
    }
    surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGB24, flipped, (int)stride);
    if (surface)
        saved = SDL_SaveBMP(surface, path);
cleanup:
    if (surface)
        SDL_DestroySurface(surface);
    free(pixels);
    free(flipped);
    return saved;
}

int dev_test_click_count(SDL_Window *window, float x, float y, int clicks)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = (Uint8)clicks;
    event.button.x = x;
    event.button.y = y;
    return SDL_PushEvent(&event);
}
int dev_test_click(SDL_Window *window, float x, float y)
{
    return dev_test_click_count(window, x, y, 1);
}
int dev_test_drag_finish(SDL_Window *window, float x, float y)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.x = x;
    event.motion.y = y;
    if (!SDL_PushEvent(&event))
        return 0;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_LEFT;
    return SDL_PushEvent(&event);
}
int dev_test_shortcut(SDL_Window *window, SDL_Keycode key, SDL_Keymod mod)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.key = key;
    event.key.mod = mod;
    return SDL_PushEvent(&event);
}
int dev_test_key(SDL_Window *window, SDL_Keycode key)
{
    return dev_test_shortcut(window, key, (key == SDLK_Z || key == SDLK_Y) ? SDL_KMOD_CTRL : 0);
}
int dev_test_replace_text(SDL_Window *window, const char *value)
{
    SDL_Event event;
    SDL_zero(event);
    int i;
    for (i = 0; i < 32; ++i)
        if (!dev_test_key(window, SDLK_BACKSPACE))
            return 0;
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(window);
    event.text.text = value;
    return SDL_PushEvent(&event) && dev_test_key(window, SDLK_RETURN);
}
int dev_test_wheel(SDL_Window *window, float x, float y, float amount)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.windowID = SDL_GetWindowID(window);
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouse_x = x;
    event.wheel.mouse_y = y;
    event.wheel.y = amount;
    return SDL_PushEvent(&event);
}
int dev_test_pan(SDL_Window *window)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_MIDDLE;
    event.button.x = 600;
    event.button.y = 400;
    if (!SDL_PushEvent(&event))
        return 0;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.xrel = 100;
    event.motion.yrel = 40;
    if (!SDL_PushEvent(&event))
        return 0;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_MIDDLE;
    return SDL_PushEvent(&event);
}
