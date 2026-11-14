#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "workbench.h"
#include "workbench_file.h"
#include "spice_process.h"
#include "spice_job.h"
#include "workbench_history.h"
#include "workbench_view.h"

static void rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS); glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h); glEnd();
}
static void color(float r,float g,float b) { glColor3f(r,g,b); }
static void line(float x,float y,float x2,float y2) { glBegin(GL_LINES); glVertex2f(x,y); glVertex2f(x2,y2); glEnd(); }
static void circle(float x,float y,float radius);
static void wire_contact_tip(WirePoint hole,WirePoint next)
{
    float dx=next.x-hole.x,dy=next.y-hole.y,length=hypotf(dx,dy);
    if(length<.01f) return;
    dx/=length; dy/=length;
    color(.12f,.36f,.48f); glLineWidth(5);
    line(hole.x+dx*14,hole.y+dy*14,hole.x+dx*5,hole.y+dy*5);
    color(.75f,.52f,.22f); glLineWidth(3);
    line(hole.x+dx*6,hole.y+dy*6,hole.x,hole.y);
    color(.98f,.8f,.43f); circle(hole.x,hole.y,2.5f);
    glLineWidth(1);
}
static void circle(float x,float y,float radius) {
    int i; glBegin(GL_TRIANGLE_FAN); glVertex2f(x,y);
    for(i=0;i<=32;++i) { float a=(float)i*6.2831853f/32; glVertex2f(x+cosf(a)*radius,y+sinf(a)*radius); } glEnd();
}
static GLuint make_font(void) {
    HDC dc=wglGetCurrentDC(); GLuint base; HFONT font; HGDIOBJ old;
    if(!dc) return 0;
    base=glGenLists(128); if(!base) return 0;
    font=CreateFontA(-17,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,0,0,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    if(!font) { glDeleteLists(base,128); return 0; }
    old=SelectObject(dc,font);
    if(!old || old==HGDI_ERROR || !wglUseFontBitmapsA(dc,0,128,base)) {
        if(old && old!=HGDI_ERROR) SelectObject(dc,old);
        DeleteObject(font); glDeleteLists(base,128); return 0;
    }
    SelectObject(dc,old); DeleteObject(font); return base;
}
static void text(GLuint font,float x,float y,const char *str) {
    if(!font) return;
    glRasterPos2f(x,y); glListBase(font); glCallLists((GLsizei)strlen(str),GL_UNSIGNED_BYTE,str);
}

static int test_click_count(SDL_Window *window,float x,float y,int clicks)
{
    SDL_Event event={0};
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID=SDL_GetWindowID(window);
    event.button.button=SDL_BUTTON_LEFT;
    event.button.clicks=(Uint8)clicks;
    event.button.x=x; event.button.y=y;
    return SDL_PushEvent(&event);
}
static int test_click(SDL_Window *window,float x,float y) { return test_click_count(window,x,y,1); }
static int test_drag_finish(SDL_Window *window,float x,float y)
{
    SDL_Event event={0};
    event.type=SDL_EVENT_MOUSE_MOTION; event.motion.windowID=SDL_GetWindowID(window); event.motion.x=x; event.motion.y=y;
    if(!SDL_PushEvent(&event)) return 0;
    SDL_zero(event); event.type=SDL_EVENT_MOUSE_BUTTON_UP; event.button.windowID=SDL_GetWindowID(window); event.button.button=SDL_BUTTON_LEFT;
    return SDL_PushEvent(&event);
}
static int test_key(SDL_Window *window,SDL_Keycode key)
{
    SDL_Event event={0};
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.windowID=SDL_GetWindowID(window);
    event.key.key=key; event.key.mod=SDL_KMOD_CTRL;
    return SDL_PushEvent(&event);
}
static int test_wheel(SDL_Window *window,float x,float y,float amount)
{
    SDL_Event event={0};
    event.type=SDL_EVENT_MOUSE_WHEEL; event.wheel.windowID=SDL_GetWindowID(window);
    event.wheel.mouse_x=x; event.wheel.mouse_y=y; event.wheel.y=amount;
    return SDL_PushEvent(&event);
}
static int test_pan(SDL_Window *window)
{
    SDL_Event event={0};
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.windowID=SDL_GetWindowID(window);
    event.button.button=SDL_BUTTON_MIDDLE; event.button.x=600; event.button.y=400;
    if(!SDL_PushEvent(&event)) return 0;
    SDL_zero(event); event.type=SDL_EVENT_MOUSE_MOTION; event.motion.windowID=SDL_GetWindowID(window);
    event.motion.xrel=100; event.motion.yrel=40;
    if(!SDL_PushEvent(&event)) return 0;
    SDL_zero(event); event.type=SDL_EVENT_MOUSE_BUTTON_UP; event.button.windowID=SDL_GetWindowID(window); event.button.button=SDL_BUTTON_MIDDLE;
    return SDL_PushEvent(&event);
}
static void pin_xy(const Part *p,int pin,float *x,float *y) {
    bench_pin_position(p,pin,x,y);
}
static void part_draw(const Part *p,int selected,int led) {
    float x=p->x,y=p->y;
    if(p->type==PART_BREADBOARD) {
        color(.67f,.70f,.72f); rect(x-224,y-108,454,224);
        color(.96f,.96f,.93f); rect(x-227,y-112,454,224);
        color(.80f,.19f,.17f); rect(x-215,y-109,430,2); rect(x-215,y+68,430,2);
        color(.16f,.31f,.47f); rect(x-215,y-72,430,2); rect(x-215,y+107,430,2);
        color(.84f,.85f,.82f); rect(x-218,y-6,436,10);
        if(selected) { color(.12f,.49f,.84f); glLineWidth(2); glBegin(GL_LINE_LOOP); glVertex2f(x-230,y-115); glVertex2f(x+230,y-115); glVertex2f(x+230,y+115); glVertex2f(x-230,y+115); glEnd(); glLineWidth(1); }
        return;
    }
    color(.68f,.71f,.73f); rect(x-51,y-3,102,6);
    color(.72f,.74f,.76f);
    if(p->type==PART_SOURCE || p->type==PART_SWITCH) rect(x-39,y-23,82,50);
    else rect(x-19,y-11,40,24);
    if(p->type==PART_SOURCE) {
        color(.22f,.26f,.31f); rect(x-40,y-27,80,50);
        color(.18f,.53f,.73f); rect(x-37,y-25,74,45);
        color(.9f,.95f,.97f); line(x-22,y,x-8,y); line(x-15,y-7,x-15,y+7); line(x+13,y,x+27,y);
        color(p->on?.25f:.55f,p->on?.9f:.55f,.34f); circle(x+25,y-16,4);
    } else if(p->type==PART_SWITCH) {
        color(.23f,.25f,.27f); rect(x-36,y-22,72,42);
        color(.77f,.79f,.79f); rect(x-31,y-17,62,30);
        color(.2f,.22f,.24f); circle(x-17,y,5); circle(x+17,y,5);
        glLineWidth(5); line(x-17,y,x+17,p->on?y:y-13); glLineWidth(1);
    } else if(p->type==PART_RESISTOR) {
        color(.73f,.60f,.40f); rect(x-18,y-8,36,16);
        color(.89f,.78f,.56f); rect(x-17,y-10,34,15);
        color(.4f,.21f,.12f); rect(x-11,y-10,3,15); color(.12f,.12f,.12f); rect(x-4,y-10,3,15);
        color(.8f,.25f,.13f); rect(x+3,y-10,3,15); color(.7f,.58f,.2f); rect(x+11,y-10,3,15);
    } else if(p->type==PART_CAPACITOR) {
        color(.16f,.19f,.25f); rect(x-18,y-23,36,46);
        color(.25f,.31f,.42f); rect(x-18,y-26,36,44);
        color(.55f,.62f,.7f); rect(x+9,y-22,4,34);
    } else if(p->type==PART_INDUCTOR) {
        int coil;
        color(.25f,.2f,.16f); rect(x-31,y-13,62,26);
        color(.66f,.36f,.18f); rect(x-29,y-15,58,23);
        color(.94f,.65f,.35f);
        for(coil=0;coil<6;++coil) rect(x-26+coil*9,y-14,3,21);
    } else {
        color(.75f,.75f,.72f); rect(x-16,y+5,32,7);
        if(led<0) color(.48f,.48f,.48f); else color(led?1.0f:.6f,led?.28f:.12f,led?.24f:.14f); circle(x,y-3,13);
        if(led<0) color(.7f,.7f,.7f); else color(led?1.0f:.8f,led?.63f:.28f,led?.55f:.27f); circle(x-4,y-7,4);
    }
    if(selected) { color(.12f,.49f,.84f); glLineWidth(2); glBegin(GL_LINE_LOOP); glVertex2f(x-46,y-32); glVertex2f(x+46,y-32); glVertex2f(x+46,y+32); glVertex2f(x-46,y+32); glEnd(); glLineWidth(1); }
}

int main(int argc, char **argv) {
    SDL_Window *window; SDL_GLContext context; GLuint font; Workbench bench={0};
    Pin pending={-1,-1}; int selected=-1, placing=-1, dragging=-1, running=1, sim_running=1;
    WirePoint route[WIRE_MAX_KNEES]; int route_count=0, drag_wire=-1, drag_knee=-1;
    int settings=0, show_grid=1, show_handles=1, round_wires=1, show_current=0, dc_wire_valid=0;
    int leds[WORKBENCH_MAX_COMPONENTS]={0}; float offset_x=0,offset_y=0;
    Workbench dc_snapshot={0}; int dc_valid=0; double dc_current=0; char dc_status[128];
    double dc_led_currents[WORKBENCH_MAX_COMPONENTS]={0};
    double dc_wire_currents[WORKBENCH_MAX_WIRES]={0};
    SpiceJob dc_job={0};
    BenchHistory *history=calloc(1,sizeof(*history)); int move_recorded=0;
    BenchView view={1,0,0}; int panning=0;
    int editing=-1; char edit_text[64]={0};
    int selftest=argc>1 && strcmp(argv[1],"--self-test-history")==0, selftest_phase=0, exit_code=0;
    const char *status="Escolha um componente na biblioteca e clique na bancada.";
    int capture=argc>1 && (strcmp(argv[1],"--capture")==0 || strcmp(argv[1],"--capture-settings")==0 || strcmp(argv[1],"--capture-analog")==0 || strcmp(argv[1],"--capture-dc")==0 || strcmp(argv[1],"--capture-current")==0 || strcmp(argv[1],"--capture-edit")==0 || strcmp(argv[1],"--capture-zoom")==0 || strcmp(argv[1],"--capture-snap")==0 || strcmp(argv[1],"--capture-board")==0);
    if(argc>1 && strcmp(argv[1],"--capture-settings")==0) settings=1;
    if(capture) {
        bench_add(&bench,PART_SOURCE,360,220); bench_add(&bench,PART_LED,720,220); bench_add(&bench,PART_BREADBOARD,550,430);
        bench_toggle_wire(&bench,(Pin){0,0},(Pin){1,0});
        bench.wires[0].knee_count=2; bench.wires[0].knees[0]=(WirePoint){308,130}; bench.wires[0].knees[1]=(WirePoint){668,130};
        bench_toggle_wire(&bench,(Pin){1,1},(Pin){0,1});
        bench.wires[1].knee_count=2; bench.wires[1].knees[0]=(WirePoint){772,300}; bench.wires[1].knees[1]=(WirePoint){412,300};
        if(strcmp(argv[1],"--capture-analog")==0) { bench_add(&bench,PART_RESISTOR,380,520); bench_add(&bench,PART_CAPACITOR,720,520); bench_add(&bench,PART_INDUCTOR,970,520); }
        if(strcmp(argv[1],"--capture-snap")==0) { int part=bench_add(&bench,PART_RESISTOR,538,393); bench_snap_to_breadboard(&bench,part); bench_view_zoom_at(&view,1.2f,550,430); }
        if(strcmp(argv[1],"--capture-board")==0) {
            bench_toggle_wire(&bench,(Pin){0,0},(Pin){2,100});
            bench.wires[2].knee_count=1; bench.wires[2].knees[0]=(WirePoint){431,300};
            bench_toggle_wire(&bench,(Pin){0,1},(Pin){2,140});
            bench.wires[3].knee_count=1; bench.wires[3].knees[0]=(WirePoint){380,441};
            bench_view_zoom_at(&view,1.4f,550,430);
        }
        if(strcmp(argv[1],"--capture-dc")==0 || strcmp(argv[1],"--capture-current")==0 || strcmp(argv[1],"--capture-edit")==0) {
            bench_add(&bench,PART_RESISTOR,530,220);
            selected=3;
            bench_toggle_wire(&bench,(Pin){0,0},(Pin){1,0});
            bench_toggle_wire(&bench,(Pin){0,0},(Pin){3,0});
            bench.wires[1].knee_count=2; bench.wires[1].knees[0]=(WirePoint){308,150}; bench.wires[1].knees[1]=(WirePoint){478,150};
            bench_toggle_wire(&bench,(Pin){3,1},(Pin){1,0});
        }
    }
    if(!history) { fprintf(stderr,"Memoria insuficiente para historico.\n"); return 1; }
    if(!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr,"SDL: %s\n",SDL_GetError()); free(history); return 1; }
    window=SDL_CreateWindow("Bancada experimental",1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!window) { fprintf(stderr,"Janela: %s\n",SDL_GetError()); SDL_Quit(); free(history); return 1; }
    context=SDL_GL_CreateContext(window);
    if(!context) { fprintf(stderr,"OpenGL: %s\n",SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); free(history); return 1; }
    font=make_font(); SDL_GL_SetSwapInterval(1);
    if(capture && strcmp(argv[1],"--capture-zoom")==0) bench_view_zoom_at(&view,1.8f,550,430);
    if(capture && strcmp(argv[1],"--capture-edit")==0) { editing=3; strcpy(edit_text,"2200"); }
    if(capture && (strcmp(argv[1],"--capture-dc")==0 || strcmp(argv[1],"--capture-current")==0)) {
        show_current=strcmp(argv[1],"--capture-current")==0;
        if(show_current) { dc_valid=spice_dc_analyze_wires(&bench,"build/spice-local/ucrt64/bin/ngspice_con.exe",&dc_current,dc_led_currents,dc_wire_currents,dc_status,sizeof(dc_status)); dc_wire_valid=dc_valid; }
        else dc_valid=spice_dc_analyze(&bench,"build/spice-local/ucrt64/bin/ngspice_con.exe",&dc_current,dc_led_currents,dc_status,sizeof(dc_status));
        if(dc_valid) dc_snapshot=bench;
        status=dc_status;
    }
    while(running) {
        SDL_Event e; int width,height,i;
        if(selftest) {
            int queued=1;
            if(selftest_phase==0) queued=test_click(window,50,150) && test_click(window,400,250);
            else if(selftest_phase==1) queued=test_click(window,50,438) && test_click(window,600,250);
            else if(selftest_phase==2) queued=test_click(window,351,250) && test_click(window,450,180) && test_click(window,551,250);
            else if(selftest_phase==3) queued=test_key(window,SDLK_Z);
            else if(selftest_phase==4) queued=test_key(window,SDLK_Y);
            else if(selftest_phase==5) queued=test_click(window,450,180) && test_drag_finish(window,460,190);
            else if(selftest_phase==6) queued=test_key(window,SDLK_Z);
            else if(selftest_phase==7) queued=test_key(window,SDLK_Y);
            else if(selftest_phase==8) queued=test_click_count(window,460,190,2);
            else if(selftest_phase==9) queued=test_key(window,SDLK_Z);
            else if(selftest_phase==10) queued=test_wheel(window,400,250,1);
            else if(selftest_phase==11) queued=test_click(window,341.2f,250);
            else if(selftest_phase==12) queued=test_click(window,581.2f,250);
            else if(selftest_phase==13) queued=test_key(window,SDLK_Z);
            else if(selftest_phase==14) queued=test_key(window,SDLK_HOME);
            else if(selftest_phase==15) queued=test_pan(window);
            else if(selftest_phase==16) queued=test_click(window,451,290);
            else if(selftest_phase==17) queued=test_key(window,SDLK_ESCAPE);
            else if(selftest_phase==18) queued=test_key(window,SDLK_HOME);
            else if(selftest_phase==19) queued=test_click(window,50,366) && test_click(window,850,430);
            else if(selftest_phase==20) queued=test_click(window,50,438) && test_click(window,838,393);
            else if(selftest_phase==21) queued=test_key(window,SDLK_Z);
            else if(selftest_phase==22) queued=test_key(window,SDLK_Y);
            if(!queued) { fprintf(stderr,"Nao foi possivel inserir eventos de teste.\n"); exit_code=1; running=0; }
        }
        while(SDL_PollEvent(&e)) {
            if(e.type==SDL_EVENT_QUIT) running=0;
            else if(editing>=0 && e.type==SDL_EVENT_TEXT_INPUT) {
                const char *p=e.text.text; size_t length=strlen(edit_text);
                while(*p && length+1<sizeof(edit_text)) { if((unsigned char)*p>=32 && (unsigned char)*p<127) edit_text[length++]=*p; ++p; }
                edit_text[length]='\0';
            }
            else if(editing>=0 && e.type==SDL_EVENT_KEY_DOWN) {
                if(e.key.key==SDLK_ESCAPE) { editing=-1; SDL_StopTextInput(window); status="Edicao cancelada."; }
                else if(e.key.key==SDLK_BACKSPACE) { size_t length=strlen(edit_text); if(length) edit_text[length-1]='\0'; }
                else if(e.key.key==SDLK_RETURN || e.key.key==SDLK_KP_ENTER) {
                    double value;
                    Workbench before=bench;
                    if(bench_parse_value(edit_text,&value) && bench_set_value(&bench,editing,value)) { if(before.parts[editing].value!=value) bench_history_record(history,&before); editing=-1; SDL_StopTextInput(window); status="Valor alterado; pressione P para recalcular."; }
                    else status="Valor invalido. Use um numero positivo entre 1e-12 e 1e12.";
                }
            }
            else if(editing>=0 && (e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_MOTION)) { continue; }
            else if(e.type==SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                if(e.key.key==SDLK_F1) { settings=!settings; dragging=-1; drag_wire=-1; continue; }
                if(e.key.key==SDLK_HOME) { view=(BenchView){1,0,0}; status="Vista restaurada."; continue; }
                if((e.key.mod & SDL_KMOD_CTRL) && (e.key.key==SDLK_Z || e.key.key==SDLK_Y)) {
                    int changed=e.key.key==SDLK_Z?bench_history_undo(history,&bench):bench_history_redo(history,&bench);
                    if(changed) { selected=-1; dragging=-1; drag_wire=-1; pending.component=-1; route_count=0; memset(leds,0,sizeof(leds)); status=e.key.key==SDLK_Z?"Acao desfeita.":"Acao refeita."; }
                    else status="Nenhuma acao disponivel.";
                    continue;
                }
                if(e.key.key==SDLK_ESCAPE) { pending.component=-1; route_count=0; placing=-1; dragging=-1; drag_wire=-1; status="Selecao cancelada."; }
                else if(e.key.key==SDLK_SPACE) { sim_running=!sim_running; status=sim_running?"Simulacao executando.":"Simulacao pausada."; }
                else if(e.key.key==SDLK_N && !sim_running) { if(bench_boolean_supported(&bench)) { bench_simulate(&bench,leds); status="Um passo executado."; } else status="Calculo eletrico ainda indisponivel."; }
                else if(e.key.key==SDLK_P) {
                    if(dc_job.thread) status="Analise em andamento.";
                    else if((show_current?spice_job_start_wires:spice_job_start)(&dc_job,&bench,"build/spice-local/ucrt64/bin/ngspice_con.exe")) { dc_valid=0; dc_wire_valid=0; status="Analisando ponto DC..."; }
                    else status="Falha ao iniciar tarefa de analise.";
                }
                else if(e.key.key==SDLK_E && selected>=0) {
                    PartType type=bench.parts[selected].type;
                    if(type==PART_SOURCE || type==PART_RESISTOR || type==PART_CAPACITOR || type==PART_INDUCTOR) {
                        if(SDL_StartTextInput(window)) { editing=selected; snprintf(edit_text,sizeof(edit_text),"%.9g",bench.parts[selected].value); status="Digite o valor em unidades SI; Enter confirma, Esc cancela."; }
                        else status="Nao foi possivel iniciar edicao de texto.";
                    } else status="Este componente nao possui valor editavel.";
                }
                else if(selected>=0 && (e.key.key==SDLK_EQUALS || e.key.key==SDLK_PLUS || e.key.key==SDLK_MINUS)) {
                    double scale=e.key.key==SDLK_MINUS?.5:2.0;
                    Workbench before=bench;
                    if(bench_set_value(&bench,selected,bench.parts[selected].value*scale)) { bench_history_record(history,&before); status="Valor alterado; pressione P para recalcular."; }
                    else status="Valor nao editavel ou fora do limite.";
                }
                else if(e.key.key==SDLK_S) { status=workbench_save("bancada.elec",&bench)?"Salvo em bancada.elec.":"Falha ao salvar."; }
                else if(e.key.key==SDLK_L) {
                    Workbench before=bench;
                    if(workbench_load("bancada.elec",&bench)) { bench_history_record(history,&before); selected=-1; dragging=-1; drag_wire=-1; pending.component=-1; route_count=0; memset(leds,0,sizeof(leds)); bench_simulate(&bench,leds); status="Bancada carregada."; }
                    else status="Falha ao carregar; bancada preservada.";
                }
                else if(e.key.key==SDLK_DELETE && selected>=0) { bench_history_record(history,&bench); bench_remove(&bench,selected); selected=-1; pending.component=-1; memset(leds,0,sizeof(leds)); status="Componente e seus fios removidos."; }
            } else if(e.type==SDL_EVENT_MOUSE_WHEEL && !settings && editing<0) {
                int w,h; float x,y,direction=e.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-e.wheel.y:e.wheel.y;
                SDL_GetWindowSize(window,&w,&h);
                if(w>0 && h>0) { x=e.wheel.mouse_x*1280.0f/w; y=e.wheel.mouse_y*720.0f/h; if(x>=220 && y>=70 && y<660 && direction!=0) bench_view_zoom_at(&view,direction>0?1.2f:1.0f/1.2f,x,y); }
            } else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button==SDL_BUTTON_MIDDLE && !settings && editing<0) {
                int w,h; SDL_GetWindowSize(window,&w,&h);
                if(w>0 && h>0 && e.button.x*1280.0f/w>=220 && e.button.y*720.0f/h>=70 && e.button.y*720.0f/h<660) panning=1;
            } else if(e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_MIDDLE) panning=0;
            else if(e.type==SDL_EVENT_MOUSE_MOTION && panning && editing<0) {
                int w,h; SDL_GetWindowSize(window,&w,&h); if(w>0 && h>0) { view.pan_x+=e.motion.xrel*1280.0f/w; view.pan_y+=e.motion.yrel*720.0f/h; }
            } else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button==SDL_BUTTON_LEFT) {
                int w,h,hit=-1,hitpin=-1; float x,y; SDL_GetWindowSize(window,&w,&h); if(w<=0||h<=0) continue;
                x=e.button.x*1280.0f/w; y=e.button.y*720.0f/h;
                if(x>=830 && x<1010 && y<68) { settings=!settings; dragging=-1; drag_wire=-1; continue; }
                if(settings) {
                    if(x>850 && x<1240) {
                        if(y>=120 && y<160) show_grid=!show_grid;
                        else if(y>=160 && y<200) show_handles=!show_handles;
                        else if(y>=200 && y<240) round_wires=!round_wires;
                        else if(y>=240 && y<280) { show_current=!show_current; if(show_current && !dc_wire_valid) status="Pressione P para medir as correntes dos fios."; }
                    }
                    continue;
                }
                if(x<220 && y>=140 && y<644) { int item=(int)((y-140)/72); if(item<7) { placing=item; pending.component=-1; status="Clique na bancada para posicionar."; } continue; }
                if(x<220||y<70||y>660) continue;
                bench_view_to_world(&view,x,y,&x,&y);
                if(pending.component<0 && placing<0 && show_handles) {
                    int k;
                    for(i=0;i<bench.wire_count && drag_wire<0;++i) for(k=0;k<bench.wires[i].knee_count;++k) {
                        WirePoint point=bench.wires[i].knees[k];
                        if(fabsf(x-point.x)<8 && fabsf(y-point.y)<8) { drag_wire=i; drag_knee=k; break; }
                    }
                    if(drag_wire>=0) {
                        if(e.button.clicks>=2) { Wire *wire=&bench.wires[drag_wire]; bench_history_record(history,&bench); memmove(&wire->knees[drag_knee],&wire->knees[drag_knee+1],(size_t)(wire->knee_count-drag_knee-1)*sizeof(WirePoint)); --wire->knee_count; drag_wire=-1; status="Joelho removido."; }
                        else { move_recorded=0; status="Arraste o joelho; duplo clique remove."; }
                        continue;
                    }
                }
                if(placing>=0) { Workbench before=bench; int added=bench_add(&bench,(PartType)placing,x,y); if(added>=0) { int snapped=bench_snap_to_breadboard(&bench,added); bench_history_record(history,&before); status=snapped?"Componente encaixado na breadboard.":"Componente adicionado. Selecione terminais para ligar."; placing=-1; } else status="Limite de componentes atingido."; continue; }
                { int pass; for(pass=0;pass<2 && hit<0;++pass) for(i=bench.part_count-1;i>=0;--i) { int p,board=bench.parts[i].type==PART_BREADBOARD; float radius=board?4.0f:12.0f; if(board!=pass) continue; for(p=0;p<bench_pin_count(bench.parts[i].type);++p) { float px,py; pin_xy(&bench.parts[i],p,&px,&py); if((x-px)*(x-px)+(y-py)*(y-py)<=radius*radius) { hit=i; hitpin=p; break; } } if(hit>=0) break; } }
                if(hitpin>=0) { Pin clicked={hit,hitpin}; if(pending.component<0) { pending=clicked; route_count=0; status="Clique para criar joelhos; termine em outro terminal."; } else if(pending.component==hit && pending.pin==hitpin) { pending.component=-1; route_count=0; status="Conexao cancelada."; } else { Workbench before=bench; int result=bench_toggle_wire(&bench,pending,clicked); if(result==1) { Wire *wire=&bench.wires[bench.wire_count-1]; wire->knee_count=route_count; memcpy(wire->knees,route,(size_t)route_count*sizeof(WirePoint)); } if(result) bench_history_record(history,&before); status=result==1?"Fio conectado.":result==2?"Fio removido.":"Nao foi possivel conectar."; pending.component=-1; route_count=0; } continue; }
                if(pending.component>=0) { if(route_count<WIRE_MAX_KNEES) { route[route_count++]=(WirePoint){x,y}; status="Joelho adicionado; termine em um terminal."; } else status="Limite de 16 joelhos por fio."; continue; }
                { int pass; for(pass=0;pass<2 && hit<0;++pass) for(i=bench.part_count-1;i>=0;--i) { int board=bench.parts[i].type==PART_BREADBOARD; float hw=board?227.0f:44.0f, hh=board?112.0f:30.0f; if(board!=pass) continue; if(fabsf(x-bench.parts[i].x)<hw && fabsf(y-bench.parts[i].y)<hh) { hit=i; break; } } }
                selected=hit;
                if(hit>=0) { if(e.button.clicks>=2 && (bench.parts[hit].type==PART_SOURCE || bench.parts[hit].type==PART_SWITCH)) { bench_history_record(history,&bench); bench.parts[hit].on=!bench.parts[hit].on; status="Estado alterado."; } else { dragging=hit; move_recorded=0; offset_x=x-bench.parts[hit].x; offset_y=y-bench.parts[hit].y; status="Arraste para mover; dois cliques alternam fonte ou chave."; } }
                else { pending.component=-1; status="Bancada livre."; }
            } else if(e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_LEFT) { if(dragging>=0 && move_recorded && bench_snap_to_breadboard(&bench,dragging)) status="Componente encaixado na breadboard."; dragging=-1; drag_wire=-1; }
            else if(e.type==SDL_EVENT_MOUSE_MOTION && drag_wire>=0) { int w,h; SDL_GetWindowSize(window,&w,&h); if(w>0&&h>0 && drag_wire<bench.wire_count) { float x=e.motion.x*1280.0f/w,y=e.motion.y*720.0f/h; WirePoint *point=&bench.wires[drag_wire].knees[drag_knee]; bench_view_to_world(&view,x,y,&x,&y); if(point->x!=x || point->y!=y) { if(!move_recorded) { bench_history_record(history,&bench); move_recorded=1; } *point=(WirePoint){x,y}; } } }
            else if(e.type==SDL_EVENT_MOUSE_MOTION && dragging>=0) { int w,h; SDL_GetWindowSize(window,&w,&h); if(w>0&&h>0) { float x=e.motion.x*1280.0f/w,y=e.motion.y*720.0f/h; bench_view_to_world(&view,x,y,&x,&y); x-=offset_x; y-=offset_y; if(bench.parts[dragging].x!=x || bench.parts[dragging].y!=y) { if(!move_recorded) { bench_history_record(history,&bench); move_recorded=1; } bench.parts[dragging].x=x; bench.parts[dragging].y=y; } } }
        }
        if(selftest) {
            int valid=1;
            if(selftest_phase==0) valid=bench.part_count==1 && bench.parts[0].type==PART_SOURCE;
            else if(selftest_phase==1) valid=bench.part_count==2 && bench.parts[1].type==PART_RESISTOR;
            else if(selftest_phase==2) valid=bench.wire_count==1 && bench.wires[0].knee_count==1 && bench.wires[0].knees[0].x==450;
            else if(selftest_phase==3) valid=bench.wire_count==0;
            else if(selftest_phase==4) valid=bench.wire_count==1 && bench.wires[0].knee_count==1 && bench.wires[0].knees[0].y==180;
            else if(selftest_phase==5) valid=bench.wires[0].knees[0].x==460 && bench.wires[0].knees[0].y==190;
            else if(selftest_phase==6) valid=bench.wires[0].knees[0].x==450 && bench.wires[0].knees[0].y==180;
            else if(selftest_phase==7) valid=bench.wires[0].knees[0].x==460 && bench.wires[0].knees[0].y==190;
            else if(selftest_phase==8) valid=bench.wires[0].knee_count==0;
            else if(selftest_phase==9) valid=bench.wires[0].knee_count==1 && bench.wires[0].knees[0].x==460;
            else if(selftest_phase==10) valid=fabsf(view.zoom-1.2f)<.001f;
            else if(selftest_phase==11) valid=pending.component==0 && pending.pin==0;
            else if(selftest_phase==12) valid=bench.wire_count==0;
            else if(selftest_phase==13) valid=bench.wire_count==1;
            else if(selftest_phase==14) valid=view.zoom==1 && view.pan_x==0 && view.pan_y==0;
            else if(selftest_phase==15) valid=view.pan_x==100 && view.pan_y==40;
            else if(selftest_phase==16) valid=pending.component==0 && pending.pin==0;
            else if(selftest_phase==17) valid=pending.component<0;
            else if(selftest_phase==18) valid=view.zoom==1 && view.pan_x==0 && view.pan_y==0;
            else if(selftest_phase==19) valid=bench.part_count==3 && bench.parts[2].type==PART_BREADBOARD;
            else if(selftest_phase==20 || selftest_phase==22) { Pin a,b; valid=bench.part_count==4 && bench.parts[3].type==PART_RESISTOR && bench_find_contact(&bench,3,0,&a) && bench_find_contact(&bench,3,1,&b) && a.component==2 && b.component==2; }
            else if(selftest_phase==21) valid=bench.part_count==3;
            if(!valid) { fprintf(stderr,"Autoteste de eventos falhou na fase %d.\n",selftest_phase); exit_code=1; running=0; }
            if(++selftest_phase==23) { if(!exit_code) puts("Autoteste de eventos, historico, navegacao e encaixe passou."); running=0; }
        }
        if(spice_job_poll(&dc_job)) {
            if(memcmp(&dc_job.snapshot,&bench,sizeof(bench))!=0) { dc_valid=0; status="Montagem mudou durante analise; resultado descartado."; }
            else { dc_valid=dc_job.success; dc_wire_valid=dc_job.success && dc_job.measures_wires; dc_snapshot=dc_job.snapshot; dc_current=dc_job.current; memcpy(dc_led_currents,dc_job.led_currents,sizeof(dc_led_currents)); if(dc_wire_valid) memcpy(dc_wire_currents,dc_job.wire_currents,sizeof(dc_wire_currents)); memcpy(dc_status,dc_job.message,sizeof(dc_status)); status=dc_status; }
        }
        if(dc_valid && memcmp(&dc_snapshot,&bench,sizeof(bench))!=0) { dc_valid=0; status="Montagem alterada; pressione P para recalcular."; }
        if(dc_valid) { for(i=0;i<bench.part_count;++i) leds[i]=bench.parts[i].type==PART_LED && dc_led_currents[i]>=1e-6; }
        else if(!bench_boolean_supported(&bench)) { for(i=0;i<WORKBENCH_MAX_COMPONENTS;++i) leds[i]=-1; }
        else if(sim_running) bench_simulate(&bench,leds);
        SDL_GetWindowSizeInPixels(window,&width,&height); glViewport(0,0,width,height);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,1280,720,0,-1,1); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glClearColor(.92f,.94f,.95f,1); glClear(GL_COLOR_BUFFER_BIT);
        color(.97f,.98f,.98f); rect(0,0,1280,68); color(.11f,.2f,.27f); text(font,24,42,"ELECTRONICS  /  BANCADA EXPERIMENTAL");
        text(font,830,42,"Configuracoes [F1]");
        if(!bench_boolean_supported(&bench)) color(.66f,.36f,.12f);
        else color(sim_running?.16f:.65f,sim_running?.55f:.42f,.24f);
        text(font,1030,42,dc_valid?"PONTO DC CALCULADO":!bench_boolean_supported(&bench)?"CALCULO INDISPONIVEL":sim_running?"SIMULACAO ATIVA":"SIMULACAO PAUSADA");
        color(.98f,.985f,.99f); rect(0,68,220,652); color(.17f,.24f,.29f); text(font,22,108,"COMPONENTES");
        for(i=0;i<7;++i) { float y=140+i*72; color(placing==i?.79f:.92f,placing==i?.9f:.94f,.96f); rect(14,y,192,62); color(.16f,.22f,.26f); text(font,76,y+36,i==0?"Fonte":i==1?"Interruptor":i==2?"LED":i==3?"Breadboard":i==4?"Resistor":i==5?"Capacitor":"Indutor"); color(i==0?.2f:i==1?.38f:.84f,i==0?.54f:.42f,i==2?.28f:.6f); circle(42,y+31,13); }
        glEnable(GL_SCISSOR_TEST);
        glScissor((int)(width*220.0/1280), (int)(height*60.0/720), (int)(width*1060.0/1280), (int)(height*590.0/720));
        glPushMatrix(); glTranslatef(220+view.pan_x,70+view.pan_y,0); glScalef(view.zoom,view.zoom,1); glTranslatef(-220,-70,0);
        color(.85f,.88f,.89f); glLineWidth(1); if(show_grid) { for(i=240;i<1280;i+=24) line((float)i,69,(float)i,659); for(i=92;i<660;i+=24) line(220,(float)i,1280,(float)i); }
        for(i=0;i<bench.part_count;++i) if(bench.parts[i].type==PART_BREADBOARD) {
            int p; part_draw(&bench.parts[i],selected==i,leds[i]);
            for(p=0;p<bench_pin_count(PART_BREADBOARD);++p) { float x,y; pin_xy(&bench.parts[i],p,&x,&y); color(.18f,.22f,.24f); circle(x,y,3.0f); if(pending.component==i && pending.pin==p) { color(1,.7f,.2f); circle(x,y,3.0f); } }
        }
        color(.25f,.45f,.57f); glLineWidth(3);
        for(i=0;i<bench.wire_count;++i) {
            WirePoint start,end,path[WIRE_PATH_CAPACITY]; int k,count,pass; Wire *w=&bench.wires[i];
            pin_xy(&bench.parts[w->a.component],w->a.pin,&start.x,&start.y); pin_xy(&bench.parts[w->b.component],w->b.pin,&end.x,&end.y);
            count=wire_path(w,start,end,round_wires?18.0f:0.0f,path);
            for(pass=0;pass<3;++pass) {
                float offset=pass==0?3.0f:0.0f;
                if(pass==0) { color(.72f,.76f,.78f); glLineWidth(6); }
                else if(pass==1) { color(.12f,.36f,.48f); glLineWidth(5); }
                else { color(.38f,.65f,.73f); glLineWidth(2); }
                glBegin(GL_LINE_STRIP); for(k=0;k<count;++k) glVertex2f(path[k].x+offset,path[k].y+offset); glEnd();
            }
            if(show_current && dc_valid && dc_wire_valid && fabs(dc_wire_currents[i])>1e-6) {
                float total=0, position; int segment;
                color(1.0f,.36f,.04f); glLineWidth(3);
                glBegin(GL_LINE_STRIP); for(k=0;k<count;++k) glVertex2f(path[k].x,path[k].y); glEnd();
                for(k=1;k<count;++k) total+=hypotf(path[k].x-path[k-1].x,path[k].y-path[k-1].y);
                if(total>0) {
                    position=sim_running?fmodf((float)SDL_GetTicks()*.08f,total):0.0f;
                    if(dc_wire_currents[i]<0) position=total-position;
                    for(segment=1;segment<count;++segment) {
                        float dx=path[segment].x-path[segment-1].x,dy=path[segment].y-path[segment-1].y, length=hypotf(dx,dy);
                        if(length>0 && position<=length) { color(1,.9f,.38f); circle(path[segment-1].x+dx*position/length,path[segment-1].y+dy*position/length,4); break; }
                        position-=length;
                    }
                }
            }
        }
        if(pending.component>=0) { float x,y; int k; pin_xy(&bench.parts[pending.component],pending.pin,&x,&y); color(.9f,.55f,.15f); for(k=0;k<route_count;++k) { line(x,y,route[k].x,route[k].y); x=route[k].x; y=route[k].y; } }
        if(show_handles) for(i=0;i<bench.wire_count;++i) { int k; for(k=0;k<bench.wires[i].knee_count;++k) { WirePoint p=bench.wires[i].knees[k]; color(.1f,.3f,.45f); rect(p.x-4,p.y-4,8,8); } }
        glLineWidth(1);
        for(i=0;i<bench.part_count;++i) if(bench.parts[i].type!=PART_BREADBOARD) { int p; part_draw(&bench.parts[i],selected==i,leds[i]); for(p=0;p<bench_pin_count(bench.parts[i].type);++p) { float x,y; Pin contact; int snapped=bench_find_contact(&bench,i,p,&contact); pin_xy(&bench.parts[i],p,&x,&y); color(.18f,.22f,.24f); circle(x,y,snapped?5.0f:8.0f); if(pending.component==i&&pending.pin==p) { color(1,.7f,.2f); circle(x,y,snapped?3.0f:5.0f); } else if(snapped) { color(.18f,.78f,.46f); circle(x,y,3.0f); } else { color(.93f,.95f,.96f); circle(x,y,5); } } }
        for(i=0;i<bench.wire_count;++i) {
            const Wire *wire=&bench.wires[i]; WirePoint start,end,path[WIRE_PATH_CAPACITY]; int count,k;
            pin_xy(&bench.parts[wire->a.component],wire->a.pin,&start.x,&start.y);
            pin_xy(&bench.parts[wire->b.component],wire->b.pin,&end.x,&end.y);
            count=wire_path(wire,start,end,round_wires?18.0f:0.0f,path);
            if(bench.parts[wire->a.component].type==PART_BREADBOARD)
                for(k=1;k<count;++k) if(hypotf(path[k].x-start.x,path[k].y-start.y)>.01f) { wire_contact_tip(start,path[k]); break; }
            if(bench.parts[wire->b.component].type==PART_BREADBOARD)
                for(k=count-2;k>=0;--k) if(hypotf(path[k].x-end.x,path[k].y-end.y)>.01f) { wire_contact_tip(end,path[k]); break; }
        }
        glPopMatrix(); glDisable(GL_SCISSOR_TEST);
        if(selected>=0) {
            Part *part=&bench.parts[selected]; char reading[100];
            const char *unit=part->type==PART_SOURCE?"V":part->type==PART_RESISTOR?"ohm":part->type==PART_CAPACITOR?"F":part->type==PART_INDUCTOR?"H":NULL;
            color(.97f,.98f,.98f); rect(840,80,420,dc_valid?70:38); color(.14f,.22f,.28f);
            if(unit) { snprintf(reading,sizeof(reading),"Valor: %.5g %s   [ E / + / - ]",part->value,unit); text(font,850,105,reading); }
            else text(font,850,105,part->type==PART_LED?"LED generico":"Componente selecionado");
            if(dc_valid && (part->type==PART_LED || part->type==PART_RESISTOR)) {
                snprintf(reading,sizeof(reading),"Corrente: %.5g mA (esq -> dir)",dc_led_currents[selected]*1000.0);
                text(font,850,137,reading);
            }
        }
        color(.97f,.98f,.98f); rect(220,660,1060,60); color(.2f,.26f,.3f); text(font,238,685,status); text(font,238,709,"Terminais: fios | Duplo clique: alternar | P: analisar DC | Espaco/N: pausa/passo | Delete: excluir");
        if(settings) {
            color(.62f,.68f,.72f); rect(846,90,400,286);
            color(.98f,.98f,.99f); rect(840,84,400,286);
            color(.14f,.22f,.28f); text(font,860,112,"CONFIGURACOES DE VISUALIZACAO");
            text(font,860,148,show_grid?"[x] Grade":"[ ] Grade");
            text(font,860,188,show_handles?"[x] Pontos de controle dos fios":"[ ] Pontos de controle dos fios");
            text(font,860,228,round_wires?"[x] Curvas arredondadas":"[ ] Curvas arredondadas");
            text(font,860,268,show_current?"[x] Visualizar corrente nos fios":"[ ] Visualizar corrente nos fios");
            text(font,860,312,"P calcula correntes; velocidade visual ilustrativa");
            text(font,860,350,"F1 fecha este painel");
        }
        if(editing>=0) {
            char reading[110];
            color(.62f,.68f,.72f); rect(446,268,400,146); color(.98f,.98f,.99f); rect(440,262,400,146);
            color(.14f,.22f,.28f); text(font,460,294,"VALOR EM UNIDADES SI");
            snprintf(reading,sizeof(reading),"> %s_",edit_text); text(font,460,336,reading);
            text(font,460,380,"Enter confirma | Esc cancela");
        }
        SDL_GL_SwapWindow(window);
        if(capture) {
            unsigned char *pixels=malloc((size_t)width*height*3), *flipped=malloc((size_t)width*height*3);
            if(pixels && flipped) {
                int row; SDL_Surface *surface;
                glReadBuffer(GL_FRONT); glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels);
                for(row=0;row<height;++row) memcpy(flipped+(size_t)row*width*3,pixels+(size_t)(height-row-1)*width*3,(size_t)width*3);
                surface=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGB24,flipped,width*3);
                if(surface) { if(!SDL_SaveBMP(surface,"build/bancada-capture.bmp")) fprintf(stderr,"Captura: %s\n",SDL_GetError()); SDL_DestroySurface(surface); }
            }
            free(pixels); free(flipped); running=0;
        }
    }
    spice_job_join(&dc_job);
    free(history);
    if(font) glDeleteLists(font,128);
    SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit(); return exit_code;
}
