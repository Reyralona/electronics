/* Experimental process-isolated ABI probe; requires the official sharedspice.h. */
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <windows.h>
#include <ngspice/sharedspice.h>

static int error_seen;
static int message(char *text, int id, void *user)
{
    (void)id; (void)user;
    if (text && (strstr(text,"Error:") || strstr(text,"couldn't be loaded"))) error_seen=1;
    if (text && error_seen) fprintf(stderr,"%s\n",text);
    return 0;
}
static int exit_callback(int status, NG_BOOL immediate, NG_BOOL quit, int id, void *user)
{
    (void)status; (void)immediate; (void)quit; (void)id; (void)user;
    error_seen=1;
    return 0;
}

int main(int argc, char **argv)
{
    typedef int (*init_fn)(SendChar *, SendStat *, ControlledExit *, SendData *, SendInitData *, BGThreadRunning *, void *);
    typedef int (*circ_fn)(char **);
    typedef int (*command_fn)(char *);
    typedef pvector_info (*vector_fn)(char *);
    HMODULE module;
    init_fn init; circ_fn circ; command_fn command; vector_fn vector;
    pvector_info result;
    double voltage, current;
    double time_values[2048]; int time_count, index, best;
    char *netlist[]={"Shared reference", "V1 1 0 5", "R1 1 0 1000", ".end", NULL};
    char *rc_netlist[]={"Shared RC reference", "V1 1 0 5", "R1 1 2 1000", "C1 2 0 1u IC=0", ".end", NULL};
    if(argc!=2) { fprintf(stderr,"Usage: ngspice_shared_probe <dll path>\n"); return 2; }
    module=LoadLibraryA(argv[1]);
    if(!module) { fprintf(stderr,"Cannot load ngspice DLL (Windows error %lu)\n",GetLastError()); return 1; }
    init=(init_fn)(void *)GetProcAddress(module,"ngSpice_Init");
    circ=(circ_fn)(void *)GetProcAddress(module,"ngSpice_Circ");
    command=(command_fn)(void *)GetProcAddress(module,"ngSpice_Command");
    vector=(vector_fn)(void *)GetProcAddress(module,"ngGet_Vec_Info");
    if(!init || !circ || !command || !vector) { fprintf(stderr,"Missing ngspice API symbol\n"); return 1; }
    if(init(message,NULL,exit_callback,NULL,NULL,NULL,NULL)!=0 || error_seen) return 1;
    if(circ(netlist)!=0 || error_seen || command("op")!=0 || error_seen) return 1;
    result=vector("v(1)");
    if(!result || !result->v_realdata || result->v_length<1) return 1;
    voltage=result->v_realdata[0];
    result=vector("v1#branch");
    if(!result || !result->v_realdata || result->v_length<1) return 1;
    current=result->v_realdata[0];
    printf("voltage=%.9g current=%.9g\n",voltage,current);
    if(fabs(voltage-5.0)>1e-8 || fabs(current+0.005)>1e-8) return 1;
    if(circ(rc_netlist)!=0 || error_seen || command("tran 10u 5m uic")!=0 || error_seen) return 1;
    result=vector("time");
    if(!result || !result->v_realdata || result->v_length<2 || result->v_length>2048) return 1;
    time_count=result->v_length;
    memcpy(time_values,result->v_realdata,(size_t)time_count*sizeof(double));
    best=0;
    for(index=1;index<time_count;++index) {
        if(time_values[index]<time_values[index-1]) return 1;
        if(fabs(time_values[index]-.001)<fabs(time_values[best]-.001)) best=index;
    }
    result=vector("v(2)");
    if(!result || !result->v_realdata || result->v_length!=time_count) return 1;
    voltage=result->v_realdata[best];
    printf("RC time=%.9g voltage=%.9g\n",time_values[best],voltage);
    if(fabs(time_values[best]-.001)>0.000011 || fabs(voltage-5.0*(1.0-exp(-time_values[best]/.001)))>.01) return 1;
    return 0;
}
