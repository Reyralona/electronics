#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "project.h"
#include "history.h"
#include "simulation.h"

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

static void component_face(const Block *block, float r, float g, float b)
{
    glColor3f(0.025f, 0.035f, 0.045f);
    rectangle(block->x + 0.015f, block->y - 0.025f, block->w, block->h);
    glColor3f(r, g, b);
    rectangle(block->x, block->y, block->w, block->h);
}

static void terminal_position(const Block *blocks, int terminal, float *x, float *y)
{
    static const int owner[] = {0, 1, 1, 2, 2, 0};
    const Block *b = &blocks[owner[terminal]];
    if (terminal == 4 || terminal == 5) { *x = b->x + b->w / 2; *y = b->y; }
    else { *x = b->x + (terminal == 0 || terminal == 2 ? b->w : 0); *y = b->y + b->h / 2; }
}

int main(void)
{
    SDL_Window *window;
    SDL_GLContext context;
    Project project = {{1, 0, 0}, {{-0.7f, -0.15f, 0.3f, 0.3f},
                                 {-0.15f, -0.12f, 0.3f, 0.24f},
                                 {0.4f, -0.15f, 0.3f, 0.3f}}};
    Circuit *circuit = &project.circuit;
    Block *blocks = project.blocks;
    History history = {0};
    Simulation simulation = {1, 0};
    const char *status = "S salvar | L carregar";
    char diagnostic[120];
    int dragging = -1;
    int drag_recorded = 0;
    int selected_terminal = -1;
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
        char title[256];
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = 0;
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE && selected_terminal >= 0) { selected_terminal = -1; status = "Conexao cancelada"; }
                else if (event.key.key == SDLK_ESCAPE) running = 0;
                else if (event.key.key == SDLK_SPACE) {
                    simulation.running = !simulation.running;
                    status = simulation.running ? "Simulacao em execucao" : "Simulacao pausada";
                }
                else if (event.key.key == SDLK_N && !simulation.running) {
                    simulation_step(&simulation, circuit);
                    status = "Um passo executado";
                }
                else if (event.key.key == SDLK_D) {
                    unsigned int flags = circuit_observed_obstacles(circuit);
                    strcpy(diagnostic, "Montagem:");
                    if (flags & CIRCUIT_SOURCE_OFF) strcat(diagnostic, " fonte desligada;");
                    if (flags & CIRCUIT_SWITCH_OPEN) strcat(diagnostic, " interruptor aberto;");
                    if (flags & CIRCUIT_WIRE_OPEN) strcat(diagnostic, " ligacao ausente;");
                    if (!flags) strcat(diagnostic, " caminho completo.");
                    status = diagnostic;
                }
                else if ((event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_Z) {
                    dragging = -1;
                    status = history_undo(&history, &project) ? "Desfeito" : "Nada para desfazer";
                }
                else if ((event.key.mod & SDL_KMOD_CTRL) && event.key.key == SDLK_Y) {
                    dragging = -1;
                    status = history_redo(&history, &project) ? "Refeito" : "Nada para refazer";
                }
                else if (event.key.key == SDLK_I) { history_record(&history, &project); circuit->switch_closed = !circuit->switch_closed; }
                else if (event.key.key == SDLK_F) { history_record(&history, &project); circuit->source_on = !circuit->source_on; }
                else if (event.key.key >= SDLK_1 && event.key.key <= SDLK_3) {
                    unsigned int wire = (unsigned int)(event.key.key - SDLK_1);
                    history_record(&history, &project);
                    circuit->disconnected_wires ^= 1u << wire;
                    status = "Ligacao alterada: teclas 1, 2, 3";
                }
                else if (event.key.key == SDLK_S) status = project_save("circuito.elec", &project) ? "Salvo: circuito.elec" : "Erro ao salvar";
                else if (event.key.key == SDLK_L) {
                    Project loaded = project;
                    if (project_load("circuito.elec", &loaded)) {
                        history_record(&history, &project);
                        project = loaded;
                        dragging = -1;
                        status = "Carregado: circuito.elec";
                    } else status = "Erro ao carregar";
                }
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
                    int hit_terminal = -1;
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        for (i = 5; i >= 0; --i) {
                            float tx, ty;
                            terminal_position(blocks, i, &tx, &ty);
                            if (inside(x, y, tx - .035f, ty - .035f, .07f, .07f)) { hit_terminal = i; break; }
                        }
                        if (hit_terminal >= 0) {
                            if (selected_terminal < 0) {
                                selected_terminal = hit_terminal;
                                status = "Selecione o outro terminal";
                            } else if (selected_terminal == hit_terminal) {
                                selected_terminal = -1;
                                status = "Conexao cancelada";
                            } else {
                                int wire = circuit_wire_for_terminals(selected_terminal, hit_terminal);
                                if (wire < 0) status = "Par nao suportado neste prototipo";
                                else {
                                    history_record(&history, &project);
                                    circuit->disconnected_wires ^= 1u << wire;
                                    status = "Ligacao alterada";
                                }
                                selected_terminal = -1;
                            }
                            continue;
                        }
                        if (selected_terminal >= 0) { selected_terminal = -1; status = "Conexao cancelada"; continue; }
                    }
                    for (i = 2; i >= 0; --i) {
                        Block *b = &blocks[i];
                        if (!inside(x, y, b->x, b->y, b->w, b->h)) continue;
                        if (event.button.button == SDL_BUTTON_RIGHT) {
                            dragging = i;
                            drag_recorded = 0;
                            drag_x = x - b->x;
                            drag_y = y - b->y;
                        } else if (i == 0) { history_record(&history, &project); circuit->source_on = !circuit->source_on; }
                        else if (i == 1) { history_record(&history, &project); circuit->switch_closed = !circuit->switch_closed; }
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
                    if (!drag_recorded) { history_record(&history, &project); drag_recorded = 1; }
                    blocks[dragging].x = (2.0f * event.motion.x / logical_width - 1.0f - pan_x) / zoom - drag_x;
                    blocks[dragging].y = (1.0f - 2.0f * event.motion.y / logical_height - pan_y) / zoom - drag_y;
                }
            }
        }
        simulation_update(&simulation, circuit);
        snprintf(title, sizeof(title), "%s | Fonte: %s | Interruptor: %s | LED: %s | %s",
                 simulation.running ? "Executando" : "Pausada",
                 circuit->source_on ? "ligada" : "desligada",
                 circuit->switch_closed ? "fechado" : "aberto",
                 simulation.led_on ? "aceso" : "apagado", status);
        SDL_SetWindowTitle(window, title);
        SDL_GetWindowSizeInPixels(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.11f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(pan_x, pan_y, 0.0f);
        glScalef(zoom, zoom, 1.0f);
        {
            int grid;
            glColor3f(0.13f, 0.17f, 0.21f);
            glLineWidth(1.0f);
            for (grid = -25; grid <= 25; ++grid) {
                float position = grid * 0.2f;
                line(position, -5.0f, position, 5.0f);
                line(-5.0f, position, 5.0f, position);
            }
        }
        glColor3f(0.7f, 0.7f, 0.7f);
        glLineWidth(3.0f);
        if (!(circuit->disconnected_wires & 1u)) line(blocks[0].x + blocks[0].w, blocks[0].y + blocks[0].h / 2,
             blocks[1].x, blocks[1].y + blocks[1].h / 2);
        if (!(circuit->disconnected_wires & 2u)) line(blocks[1].x + blocks[1].w, blocks[1].y + blocks[1].h / 2,
             blocks[2].x, blocks[2].y + blocks[2].h / 2);
        if (!(circuit->disconnected_wires & 4u)) {
        line(blocks[2].x + blocks[2].w / 2, blocks[2].y,
             blocks[2].x + blocks[2].w / 2, -0.7f);
        line(blocks[2].x + blocks[2].w / 2, -0.7f, blocks[0].x + blocks[0].w / 2, -0.7f);
        line(blocks[0].x + blocks[0].w / 2, -0.7f,
             blocks[0].x + blocks[0].w / 2, blocks[0].y);
        }
        if (circuit->source_on) component_face(&blocks[0], 0.25f, 0.55f, 0.85f);
        else component_face(&blocks[0], 0.15f, 0.25f, 0.35f);
        component_face(&blocks[1], 0.32f, 0.43f, 0.35f);
        if (simulation.led_on) component_face(&blocks[2], 1.0f, 0.85f, 0.2f);
        else component_face(&blocks[2], 0.3f, 0.27f, 0.15f);
        glColor3f(0.95f, 0.95f, 0.9f);
        glLineWidth(3.0f);
        /* Source: plus and minus. */
        line(blocks[0].x + .06f, blocks[0].y + .20f, blocks[0].x + .12f, blocks[0].y + .20f);
        line(blocks[0].x + .09f, blocks[0].y + .17f, blocks[0].x + .09f, blocks[0].y + .23f);
        line(blocks[0].x + .18f, blocks[0].y + .10f, blocks[0].x + .24f, blocks[0].y + .10f);
        /* Switch: lever is horizontal when closed. */
        line(blocks[1].x + .06f, blocks[1].y + .08f, blocks[1].x + .23f,
             blocks[1].y + (circuit->switch_closed ? .08f : .17f));
        /* LED: diode symbol. */
        line(blocks[2].x + .09f, blocks[2].y + .08f, blocks[2].x + .09f, blocks[2].y + .22f);
        line(blocks[2].x + .09f, blocks[2].y + .08f, blocks[2].x + .2f, blocks[2].y + .15f);
        line(blocks[2].x + .09f, blocks[2].y + .22f, blocks[2].x + .2f, blocks[2].y + .15f);
        line(blocks[2].x + .21f, blocks[2].y + .08f, blocks[2].x + .21f, blocks[2].y + .22f);
        {
            int terminal;
            for (terminal = 0; terminal < 6; ++terminal) {
                float x, y;
                terminal_position(blocks, terminal, &x, &y);
                if (terminal == selected_terminal) glColor3f(1.0f, 0.7f, 0.15f);
                else glColor3f(0.9f, 0.9f, 0.9f);
                rectangle(x - .025f, y - .025f, .05f, .05f);
            }
        }
        SDL_GL_SwapWindow(window);
    }
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
