#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "circuit.h"

static void rectangle(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static int inside(float x, float y, float left, float bottom, float width, float height)
{
    return x >= left && x <= left + width && y >= bottom && y <= bottom + height;
}

int main(void)
{
    SDL_Window *window;
    SDL_GLContext context;
    Circuit circuit = {1, 0};
    int running = 1;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("Circuito", 800, 500, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    context = SDL_GL_CreateContext(window);
    if (!context) {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    while (running) {
        SDL_Event event;
        int width, height;
        char title[160];
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = 0;
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE) running = 0;
                else if (event.key.key == SDLK_I) circuit.switch_closed = !circuit.switch_closed;
                else if (event.key.key == SDLK_F) circuit.source_on = !circuit.source_on;
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                int logical_width, logical_height;
                SDL_GetWindowSize(window, &logical_width, &logical_height);
                if (logical_width > 0 && logical_height > 0) {
                    float x = 2.0f * event.button.x / logical_width - 1.0f;
                    float y = 1.0f - 2.0f * event.button.y / logical_height;
                    if (inside(x, y, -0.7f, -0.15f, 0.3f, 0.3f))
                        circuit.source_on = !circuit.source_on;
                    else if (inside(x, y, -0.15f, -0.12f, 0.3f, 0.24f))
                        circuit.switch_closed = !circuit.switch_closed;
                }
            }
        }
        snprintf(title, sizeof(title), "Fonte: %s | Interruptor: %s | LED: %s | Clique ou F / I | Esc",
                 circuit.source_on ? "ligada" : "desligada",
                 circuit.switch_closed ? "fechado" : "aberto",
                 circuit_led_on(&circuit) ? "aceso" : "apagado");
        SDL_SetWindowTitle(window, title);
        SDL_GetWindowSizeInPixels(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.11f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(0.35f, 0.4f, 0.45f);
        rectangle(-0.75f, -0.025f, 1.5f, 0.05f);
        if (circuit.source_on) glColor3f(0.25f, 0.55f, 0.85f);
        else glColor3f(0.15f, 0.25f, 0.35f);
        rectangle(-0.7f, -0.15f, 0.3f, 0.3f);
        glColor3f(circuit.switch_closed ? 0.3f : 0.55f, 0.55f, 0.35f);
        rectangle(-0.15f, -0.12f, 0.3f, 0.24f);
        if (circuit_led_on(&circuit)) glColor3f(1.0f, 0.85f, 0.2f);
        else glColor3f(0.3f, 0.27f, 0.15f);
        rectangle(0.4f, -0.15f, 0.3f, 0.3f);
        SDL_GL_SwapWindow(window);
    }
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
