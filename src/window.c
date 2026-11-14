#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "project.h"

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

static void line(float x1, float y1, float x2, float y2)
{
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

int main(void)
{
    SDL_Window *window;
    SDL_GLContext context;
    Project project = {{1, 0}, {{-0.7f, -0.15f, 0.3f, 0.3f},
                                 {-0.15f, -0.12f, 0.3f, 0.24f},
                                 {0.4f, -0.15f, 0.3f, 0.3f}}};
    Circuit *circuit = &project.circuit;
    Block *blocks = project.blocks;
    const char *status = "S salvar | L carregar";
    int dragging = -1;
    float drag_x = 0, drag_y = 0;
    float zoom = 1.0f, pan_x = 0.0f, pan_y = 0.0f;
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
                else if (event.key.key == SDLK_I) circuit->switch_closed = !circuit->switch_closed;
                else if (event.key.key == SDLK_F) circuit->source_on = !circuit->source_on;
                else if (event.key.key == SDLK_S) status = project_save("circuito.elec", &project) ? "Salvo: circuito.elec" : "Erro ao salvar";
                else if (event.key.key == SDLK_L) status = project_load("circuito.elec", &project) ? "Carregado: circuito.elec" : "Erro ao carregar";
                else if (event.key.key == SDLK_LEFT) pan_x += 0.1f;
                else if (event.key.key == SDLK_RIGHT) pan_x -= 0.1f;
                else if (event.key.key == SDLK_UP) pan_y -= 0.1f;
                else if (event.key.key == SDLK_DOWN) pan_y += 0.1f;
                else if (event.key.key == SDLK_HOME) { zoom = 1.0f; pan_x = 0.0f; pan_y = 0.0f; }
            }
            else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (event.wheel.y > 0) zoom *= 1.1f;
                else if (event.wheel.y < 0) zoom /= 1.1f;
                if (zoom < 0.4f) zoom = 0.4f;
                if (zoom > 3.0f) zoom = 3.0f;
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                     (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)) {
                int logical_width, logical_height;
                SDL_GetWindowSize(window, &logical_width, &logical_height);
                if (logical_width > 0 && logical_height > 0) {
                    float x = (2.0f * event.button.x / logical_width - 1.0f - pan_x) / zoom;
                    float y = (1.0f - 2.0f * event.button.y / logical_height - pan_y) / zoom;
                    int i;
                    for (i = 2; i >= 0; --i) {
                        Block *b = &blocks[i];
                        if (!inside(x, y, b->x, b->y, b->w, b->h)) continue;
                        if (event.button.button == SDL_BUTTON_RIGHT) {
                            dragging = i;
                            drag_x = x - b->x;
                            drag_y = y - b->y;
                        } else if (i == 0) circuit->source_on = !circuit->source_on;
                        else if (i == 1) circuit->switch_closed = !circuit->switch_closed;
                        break;
                    }
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
                dragging = -1;
            else if (event.type == SDL_EVENT_MOUSE_MOTION && dragging >= 0) {
                int logical_width, logical_height;
                SDL_GetWindowSize(window, &logical_width, &logical_height);
                if (logical_width > 0 && logical_height > 0) {
                    blocks[dragging].x = (2.0f * event.motion.x / logical_width - 1.0f - pan_x) / zoom - drag_x;
                    blocks[dragging].y = (1.0f - 2.0f * event.motion.y / logical_height - pan_y) / zoom - drag_y;
                }
            }
        }
        snprintf(title, sizeof(title), "Fonte: %s | Interruptor: %s | LED: %s | %s",
                 circuit->source_on ? "ligada" : "desligada",
                 circuit->switch_closed ? "fechado" : "aberto",
                 circuit_led_on(circuit) ? "aceso" : "apagado", status);
        SDL_SetWindowTitle(window, title);
        SDL_GetWindowSizeInPixels(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.11f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(pan_x, pan_y, 0.0f);
        glScalef(zoom, zoom, 1.0f);
        glColor3f(0.7f, 0.7f, 0.7f);
        glLineWidth(3.0f);
        line(blocks[0].x + blocks[0].w, blocks[0].y + blocks[0].h / 2,
             blocks[1].x, blocks[1].y + blocks[1].h / 2);
        line(blocks[1].x + blocks[1].w, blocks[1].y + blocks[1].h / 2,
             blocks[2].x, blocks[2].y + blocks[2].h / 2);
        line(blocks[2].x + blocks[2].w / 2, blocks[2].y,
             blocks[2].x + blocks[2].w / 2, -0.7f);
        line(blocks[2].x + blocks[2].w / 2, -0.7f, blocks[0].x + blocks[0].w / 2, -0.7f);
        line(blocks[0].x + blocks[0].w / 2, -0.7f,
             blocks[0].x + blocks[0].w / 2, blocks[0].y);
        if (circuit->source_on) glColor3f(0.25f, 0.55f, 0.85f);
        else glColor3f(0.15f, 0.25f, 0.35f);
        rectangle(blocks[0].x, blocks[0].y, blocks[0].w, blocks[0].h);
        glColor3f(circuit->switch_closed ? 0.3f : 0.55f, 0.55f, 0.35f);
        rectangle(blocks[1].x, blocks[1].y, blocks[1].w, blocks[1].h);
        if (circuit_led_on(circuit)) glColor3f(1.0f, 0.85f, 0.2f);
        else glColor3f(0.3f, 0.27f, 0.15f);
        rectangle(blocks[2].x, blocks[2].y, blocks[2].w, blocks[2].h);
        SDL_GL_SwapWindow(window);
    }
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
