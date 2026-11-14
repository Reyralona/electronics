#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "workbench.h"

static void rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS); glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h); glEnd();
}
static void color(float r,float g,float b) { glColor3f(r,g,b); }
static void line(float x,float y,float x2,float y2) { glBegin(GL_LINES); glVertex2f(x,y); glVertex2f(x2,y2); glEnd(); }
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
static void pin_xy(const Part *p,int pin,float *x,float *y) { *x=p->x+(pin ? 52.0f : -52.0f); *y=p->y; }
static void part_draw(const Part *p,int selected,int led) {
    float x=p->x,y=p->y;
    color(.68f,.71f,.73f); rect(x-51,y-3,102,6);
    color(.72f,.74f,.76f); rect(x-39,y-23,82,50);
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
    } else {
        color(.75f,.75f,.72f); rect(x-31,y+7,62,10);
        color(led?1.0f:.6f,led?.28f:.12f,led?.24f:.14f); circle(x,y-3,22);
        color(led?1.0f:.8f,led?.63f:.28f,led?.55f:.27f); circle(x-6,y-10,7);
    }
    if(selected) { color(.12f,.49f,.84f); glLineWidth(2); glBegin(GL_LINE_LOOP); glVertex2f(x-46,y-32); glVertex2f(x+46,y-32); glVertex2f(x+46,y+32); glVertex2f(x-46,y+32); glEnd(); glLineWidth(1); }
}

int main(void) {
    SDL_Window *window; SDL_GLContext context; GLuint font; Workbench bench={0};
    Pin pending={-1,-1}; int selected=-1, placing=-1, dragging=-1, running=1, sim_running=1;
    int leds[WORKBENCH_MAX_COMPONENTS]={0}; float offset_x=0,offset_y=0;
    const char *status="Escolha um componente na biblioteca e clique na bancada.";
    if(!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
    window=SDL_CreateWindow("Bancada experimental",1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!window) { fprintf(stderr,"Janela: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    context=SDL_GL_CreateContext(window);
    if(!context) { fprintf(stderr,"OpenGL: %s\n",SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    font=make_font(); SDL_GL_SetSwapInterval(1);
    while(running) {
        SDL_Event e; int width,height,i;
        while(SDL_PollEvent(&e)) {
            if(e.type==SDL_EVENT_QUIT) running=0;
            else if(e.type==SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                if(e.key.key==SDLK_ESCAPE) { pending.component=-1; placing=-1; dragging=-1; status="Selecao cancelada."; }
                else if(e.key.key==SDLK_SPACE) { sim_running=!sim_running; status=sim_running?"Simulacao executando.":"Simulacao pausada."; }
                else if(e.key.key==SDLK_N && !sim_running) { bench_simulate(&bench,leds); status="Um passo executado."; }
                else if(e.key.key==SDLK_DELETE && selected>=0) { bench_remove(&bench,selected); selected=-1; pending.component=-1; memset(leds,0,sizeof(leds)); status="Componente e seus fios removidos."; }
            } else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button==SDL_BUTTON_LEFT) {
                int w,h,hit=-1,hitpin=-1; float x,y; SDL_GetWindowSize(window,&w,&h); if(w<=0||h<=0) continue;
                x=e.button.x*1280.0f/w; y=e.button.y*720.0f/h;
                if(x<220 && y>=140 && y<392) { int item=(int)((y-140)/84); if(item<3) { placing=item; pending.component=-1; status="Clique na bancada para posicionar."; } continue; }
                if(x<220||y<70||y>660) continue;
                if(placing>=0) { if(bench_add(&bench,(PartType)placing,x,y)>=0) { status="Componente adicionado. Selecione terminais para ligar."; placing=-1; } else status="Limite de componentes atingido."; continue; }
                for(i=bench.part_count-1;i>=0;--i) { int p; for(p=0;p<2;++p) { float px,py; pin_xy(&bench.parts[i],p,&px,&py); if((x-px)*(x-px)+(y-py)*(y-py)<=144) { hit=i; hitpin=p; break; } } if(hit>=0) break; }
                if(hitpin>=0) { Pin clicked={hit,hitpin}; if(pending.component<0) { pending=clicked; status="Selecione outro terminal; clique no mesmo para cancelar."; } else if(pending.component==hit && pending.pin==hitpin) { pending.component=-1; status="Conexao cancelada."; } else { int result=bench_toggle_wire(&bench,pending,clicked); status=result==1?"Fio conectado.":result==2?"Fio removido.":"Nao foi possivel conectar."; pending.component=-1; } continue; }
                for(i=bench.part_count-1;i>=0;--i) if(fabsf(x-bench.parts[i].x)<44 && fabsf(y-bench.parts[i].y)<30) { hit=i; break; }
                selected=hit;
                if(hit>=0) { if(e.button.clicks>=2 && bench.parts[hit].type!=PART_LED) { bench.parts[hit].on=!bench.parts[hit].on; status="Estado alterado."; } else { dragging=hit; offset_x=x-bench.parts[hit].x; offset_y=y-bench.parts[hit].y; status="Arraste para mover; dois cliques alternam fonte ou chave."; } }
                else { pending.component=-1; status="Bancada livre."; }
            } else if(e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_LEFT) dragging=-1;
            else if(e.type==SDL_EVENT_MOUSE_MOTION && dragging>=0) { int w,h; SDL_GetWindowSize(window,&w,&h); if(w>0&&h>0) { bench.parts[dragging].x=e.motion.x*1280.0f/w-offset_x; bench.parts[dragging].y=e.motion.y*720.0f/h-offset_y; } }
        }
        if(sim_running) bench_simulate(&bench,leds);
        SDL_GetWindowSizeInPixels(window,&width,&height); glViewport(0,0,width,height);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,1280,720,0,-1,1); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glClearColor(.92f,.94f,.95f,1); glClear(GL_COLOR_BUFFER_BIT);
        color(.97f,.98f,.98f); rect(0,0,1280,68); color(.11f,.2f,.27f); text(font,24,42,"ELECTRONICS  /  BANCADA EXPERIMENTAL");
        color(sim_running?.16f:.65f,sim_running?.55f:.42f,.24f); text(font,1030,42,sim_running?"SIMULACAO ATIVA":"SIMULACAO PAUSADA");
        color(.98f,.985f,.99f); rect(0,68,220,652); color(.17f,.24f,.29f); text(font,22,108,"COMPONENTES");
        for(i=0;i<3;++i) { float y=140+i*84; color(placing==i?.79f:.92f,placing==i?.9f:.94f,.96f); rect(14,y,192,70); color(.16f,.22f,.26f); text(font,76,y+39,i==0?"Fonte":i==1?"Interruptor":"LED"); color(i==0?.2f:i==1?.38f:.84f,i==0?.54f:.42f,i==2?.28f:.6f); circle(42,y+35,13); }
        color(.85f,.88f,.89f); glLineWidth(1); for(i=240;i<1280;i+=24) line((float)i,69,(float)i,659); for(i=92;i<660;i+=24) line(220,(float)i,1280,(float)i);
        color(.25f,.45f,.57f); glLineWidth(3);
        for(i=0;i<bench.wire_count;++i) { float x,y,x2,y2; Wire *w=&bench.wires[i]; pin_xy(&bench.parts[w->a.component],w->a.pin,&x,&y); pin_xy(&bench.parts[w->b.component],w->b.pin,&x2,&y2); line(x,y,x2,y2); }
        glLineWidth(1);
        for(i=0;i<bench.part_count;++i) { int p; part_draw(&bench.parts[i],selected==i,leds[i]); for(p=0;p<2;++p) { float x,y; pin_xy(&bench.parts[i],p,&x,&y); color(.18f,.22f,.24f); circle(x,y,8); if(pending.component==i&&pending.pin==p) color(1,.7f,.2f); else color(.93f,.95f,.96f); circle(x,y,5); } }
        color(.97f,.98f,.98f); rect(220,660,1060,60); color(.2f,.26f,.3f); text(font,238,685,status); text(font,238,709,"Clique nos terminais para ligar | Duplo clique: alternar | Espaco: pausa | N: passo | Delete: excluir");
        SDL_GL_SwapWindow(window);
    }
    if(font) glDeleteLists(font,128);
    SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 0;
}
