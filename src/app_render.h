#ifndef APP_RENDER_H
#define APP_RENDER_H
#include <SDL3/SDL_opengl.h>
#include "app_state.h"
GLuint app_render_font_create(void);
void app_render_draw(const AppState *app, GLuint font, int width, int height);
#endif
