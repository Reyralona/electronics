#ifndef DEV_CAPTURE_H
#define DEV_CAPTURE_H
#include <SDL3/SDL.h>

/* Development-only input injection and framebuffer capture. */
int dev_capture_requested(const char *argument);
int dev_capture_save(const char *path, int width, int height);
int dev_test_click_count(SDL_Window *window, float x, float y, int clicks);
int dev_test_click(SDL_Window *window, float x, float y);
int dev_test_drag_finish(SDL_Window *window, float x, float y);
int dev_test_key(SDL_Window *window, SDL_Keycode key);
int dev_test_replace_text(SDL_Window *window, const char *value);
int dev_test_wheel(SDL_Window *window, float x, float y, float amount);
int dev_test_pan(SDL_Window *window);
#endif
