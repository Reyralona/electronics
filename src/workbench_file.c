#include "workbench_file.h"
#include <math.h>
#include <string.h>
#include <windows.h>

int workbench_write(FILE *f, const Workbench *b)
{
    int i, k;
    if (fprintf(f,"workbench 5\n%d %d\n",b->part_count,b->wire_count)<0) return 0;
    for (i=0;i<b->part_count;++i) {
        const Part *p=&b->parts[i];
        if (fprintf(f,"%d %.9g %.9g %d %.17g\n",(int)p->type,p->x,p->y,p->on,p->value)<0) return 0;
    }
    for (i=0;i<b->wire_count;++i) {
        const Wire *w=&b->wires[i];
        if (fprintf(f,"%d %d %d %d %d",w->a.component,w->a.pin,w->b.component,w->b.pin,w->knee_count)<0) return 0;
        for(k=0;k<w->knee_count;++k) if(fprintf(f," %.9g %.9g",w->knees[k].x,w->knees[k].y)<0) return 0;
        if(fputc('\n',f)==EOF) return 0;
    }
    return !ferror(f);
}

int workbench_read(FILE *f, Workbench *destination)
{
    Workbench b={0}; char magic[32], extra; int version, parts, wires, i, k;
    if(fscanf(f,"%31s %d %d %d",magic,&version,&parts,&wires)!=4 || strcmp(magic,"workbench") || (version<1 || version>5) || parts<0 || parts>WORKBENCH_MAX_COMPONENTS || wires<0 || wires>WORKBENCH_MAX_WIRES) return 0;
    for(i=0;i<parts;++i) {
        int type,on; float x,y;
        if(fscanf(f,"%d %f %f %d",&type,&x,&y,&on)!=4 || (version==1 && type>PART_BREADBOARD) || (version==2 && type>PART_CAPACITOR) || !isfinite(x) || !isfinite(y) || fabsf(x)>100000 || fabsf(y)>100000 || (on!=0 && on!=1) || bench_add(&b,(PartType)type,x,y)<0) return 0;
        b.parts[i].on=on;
        if(version>=4) {
            double value;
            if(fscanf(f,"%lf",&value)!=1 || !isfinite(value)) return 0;
            if(type==PART_SOURCE || type==PART_RESISTOR || type==PART_CAPACITOR || type==PART_INDUCTOR) { if(!bench_set_value(&b,i,value)) return 0; }
            else if(value!=0) return 0;
        }
    }
    for(i=0;i<wires;++i) {
        Pin a,c; int knees;
        if(fscanf(f,"%d %d %d %d %d",&a.component,&a.pin,&c.component,&c.pin,&knees)!=5 || knees<0 || knees>WIRE_MAX_KNEES || (version<5 && (a.pin>=100 || c.pin>=100)) || bench_toggle_wire(&b,a,c)!=1) return 0;
        b.wires[i].knee_count=knees;
        for(k=0;k<knees;++k) {
            WirePoint *p=&b.wires[i].knees[k];
            if(fscanf(f,"%f %f",&p->x,&p->y)!=2 || !isfinite(p->x) || !isfinite(p->y) || fabsf(p->x)>100000 || fabsf(p->y)>100000) return 0;
        }
    }
    if(fscanf(f," %c",&extra)!=EOF || ferror(f)) return 0;
    *destination=b;
    return 1;
}

int workbench_save(const char *path, const Workbench *bench)
{
    char directory[MAX_PATH], temporary[MAX_PATH];
    const char *slash=strrchr(path,'/'), *backslash=strrchr(path,'\\'), *end=slash;
    size_t length; FILE *file; int ok;
    if(backslash && (!end || backslash>end)) end=backslash;
    length=end?(size_t)(end-path+1):0;
    if(length>=sizeof(directory)) return 0;
    if(length) { memcpy(directory,path,length); directory[length]='\0'; } else strcpy(directory,".");
    if(!GetTempFileNameA(directory,"ele",0,temporary)) return 0;
    file=fopen(temporary,"w");
    if(!file) { DeleteFileA(temporary); return 0; }
    ok=workbench_write(file,bench);
    if(fclose(file)!=0) ok=0;
    if(ok) ok=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) DeleteFileA(temporary);
    return ok;
}

int workbench_load(const char *path, Workbench *bench)
{
    FILE *file=fopen(path,"r"); int ok;
    if(!file) return 0;
    ok=workbench_read(file,bench);
    fclose(file);
    return ok;
}
