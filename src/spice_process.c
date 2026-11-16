#include "component_registry.h"
#include "spice_process.h"
#include "netlist.h"
#include "ui_text.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPICE_PROCESS_TIMEOUT_SECONDS 10
#define WINDOWS_DLL_NOT_FOUND 0xC0000135UL

int spice_wire_current_level(double amperes)
{
    double magnitude=fabs(amperes);
    if(!isfinite(magnitude) || magnitude<1e-6) return 0;
    if(magnitude<1e-4) return 1;
    if(magnitude<1e-2) return 2;
    return 3;
}

static int analyze(const Workbench *bench,const char *executable,double *current,double led_currents[WORKBENCH_MAX_COMPONENTS],double *wire_currents,SpiceTrace *trace,int trace_current,int instrument,Pin probe,double step,double stop,char *message,size_t capacity)
{
    char dir[MAX_PATH], input[MAX_PATH]="", output[MAX_PATH]="", exe[MAX_PATH], command[3*MAX_PATH+32], *buffer=NULL; size_t buffer_capacity=trace?16*1024*1024:32768;
    DWORD length,exit_code=1; FILE *file=NULL; HANDLE log=INVALID_HANDLE_VALUE; STARTUPINFOA si={0}; PROCESS_INFORMATION pi={0};
    SECURITY_ATTRIBUTES security={sizeof(SECURITY_ATTRIBUTES),NULL,TRUE}; NetlistResult result; int ok=0;
    if(!bench || !executable || !message || !capacity || (!trace && (!current || !led_currents))) return 0;
    if(trace && (!isfinite(step) || !isfinite(stop) || step<=0 || stop<=0 || stop/step>500.0*(1.0+1e-12))) { snprintf(message,capacity,"%s",tr(TXT_ENG_STEP_OR_DURATION_INVALID)); return 0; }
    snprintf(message,capacity,"%s",tr(TXT_ENG_CALCULATION_PREPARE_FAILED));
    buffer=malloc(buffer_capacity); if(!buffer) return 0;
    length=GetFullPathNameA(executable,MAX_PATH,exe,NULL);
    if(!length || length>=MAX_PATH || GetFileAttributesA(exe)==INVALID_FILE_ATTRIBUTES) { snprintf(message,capacity,"%s",tr(TXT_ENG_SOLVER_MISSING)); goto cleanup; }
    length=GetTempPathA(MAX_PATH,dir);
    if(!length || length>=MAX_PATH || !GetTempFileNameA(dir,"nci",0,input) || !GetTempFileNameA(dir,"nco",0,output)) goto cleanup;
    file=fopen(input,"w"); if(!file) goto cleanup;
    result=trace?(instrument?netlist_write_transient_instrumented(file,bench,trace_current+1,probe,step,stop):trace_current==1?netlist_write_transient_current(file,bench,probe.component,step,stop):netlist_write_transient(file,bench,probe,step,stop)):wire_currents?netlist_write_dc_instrumented(file,bench):netlist_write_dc(file,bench);
    if(fclose(file)!=0) result=NETLIST_IO_ERROR;
    file=NULL;
    if(result!=NETLIST_OK) { snprintf(message,capacity,"%s",netlist_result_message(result)); goto cleanup; }
    log=CreateFileA(output,GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
    if(log==INVALID_HANDLE_VALUE) goto cleanup;
    si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=log; si.hStdError=log; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    snprintf(command,sizeof(command),"\"%s\" -b \"%s\"",exe,input);
    if(!CreateProcessA(exe,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)) { snprintf(message,capacity,tr(TXT_ENG_SOLVER_START_FAILED),GetLastError()); goto cleanup; }
    if(WaitForSingleObject(pi.hProcess,SPICE_PROCESS_TIMEOUT_SECONDS*1000)!=WAIT_OBJECT_0) { TerminateProcess(pi.hProcess,1); WaitForSingleObject(pi.hProcess,5000); snprintf(message,capacity,tr(TXT_ENG_CALCULATION_TIMEOUT),SPICE_PROCESS_TIMEOUT_SECONDS); goto cleanup; }
    GetExitCodeProcess(pi.hProcess,&exit_code);
    CloseHandle(log); log=INVALID_HANDLE_VALUE;
    if(exit_code!=0) { snprintf(message,capacity,tr(exit_code==WINDOWS_DLL_NOT_FOUND?TXT_ENG_SOLVER_DLL_MISSING:TXT_ENG_SOLVER_FAILED),exit_code); goto cleanup; }
    file=fopen(output,"r"); if(!file) goto cleanup;
    length=(DWORD)fread(buffer,1,buffer_capacity-1,file); buffer[length]='\0';
    if(!feof(file) || ferror(file) || strstr(buffer,"Error:") || strstr(buffer,"error:") || strstr(buffer,"Warning:") || strstr(buffer,"warning:") || strstr(buffer,"couldn't be loaded") || strstr(buffer,"singular matrix") || strstr(buffer,"convergence failed")) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
    if(trace) {
        SpiceTrace local={0}; char *line=buffer; double previous=-1; int led_indices[WORKBENCH_MAX_COMPONENTS],led_count=0,section=0,section_count=0,i;
        for(i=0;i<bench->part_count;++i) if(bench->parts[i].type==PART_LED) led_indices[led_count++]=i;
        while(*line) {
            char *next=strchr(line,'\n'); int index,consumed=0; double time,value;
            if(next) *next='\0';
            if(sscanf(line," %d %lf %lf %n",&index,&time,&value,&consumed)==3 && consumed>0 && line[consumed]=='\0') {
                if(index==0 && section_count>0) {
                    if(section>0 && section_count!=local.count) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
                    ++section; section_count=0;
                }
                if(index!=section_count || !isfinite(time) || !isfinite(value) || time<0 || index>=SPICE_TRANSIENT_MAX_SAMPLES || section>led_count+(instrument?bench->wire_count:0)) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
                if(section==0) {
                    if(time<previous) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
                    local.samples[local.count++]=(SpiceSample){time,value}; previous=time;
                } else {
                    if(index>=local.count || time!=local.samples[index].time) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
                    if(section<=led_count) {
                        int part = led_indices[section-1];
                        if(value>=1e-6) local.leds_on[index]|=UINT64_C(1)<<part;
                        double fraction = fmin(1.0, fmax(0.0, value / SPICE_LED_VISUAL_FULL_SCALE_AMPERES));
                        local.led_brightness[index][part] = (unsigned char)(255.0 * sqrt(fraction));
                    }
                    else { int wire=section-led_count-1, level=spice_wire_current_level(value); uint64_t bit=UINT64_C(1)<<(wire%64);
                        if(level && value>0) local.wires_forward[index][wire/64]|=bit;
                        else if(level && value<0) local.wires_reverse[index][wire/64]|=bit;
                        local.wire_levels[index][wire/4]|=(unsigned char)(level<<((wire%4)*2));
                    }
                }
                ++section_count;
            }
            if(!next) break;
            line=next+1;
        }
        if(local.count<2 || previous<stop*(1-1e-6) || section!=led_count+(instrument?bench->wire_count:0) || section_count!=local.count) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
        local.measures_wires=instrument; *trace=local; snprintf(message,capacity,tr(TXT_ENG_GRAPH_CALCULATED),local.count); ok=1; goto cleanup;
    }
    {
        char *start=strstr(buffer,"@vsource[i] ="), *end;
        double value, local_leds[WORKBENCH_MAX_COMPONENTS]={0}, local_wires[WORKBENCH_MAX_WIRES]={0}; int i;
        if(!start) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
        value=strtod(start+strlen("@vsource[i] ="),&end);
        if(end==start+strlen("@vsource[i] =") || !isfinite(value)) goto cleanup;
        for(i=0;i<bench->part_count;++i) {
            char vector[48], key[64]; double led;
            if (!component_current_vector(bench->parts[i].type, i, vector, sizeof(vector))) continue;
            snprintf(key,sizeof(key),"%s =",vector);
            start=strstr(buffer,key);
            if(!start) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
            led=strtod(start+strlen(key),&end);
            if(end==start+strlen(key) || !isfinite(led)) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
            local_leds[i]=led;
        }
        if(wire_currents) for(i=0;i<bench->wire_count;++i) {
            char key[32]; double wire;
            snprintf(key,sizeof(key),"@vwire%d[i] =",i);
            start=strstr(buffer,key);
            if(!start) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
            wire=strtod(start+strlen(key),&end);
            if(end==start+strlen(key) || !isfinite(wire)) { snprintf(message,capacity,"%s",tr(TXT_ENG_RESULT_INVALID)); goto cleanup; }
            local_wires[i]=wire;
        }
        if(wire_currents) memcpy(wire_currents,local_wires,sizeof(local_wires));
        *current=value; memcpy(led_currents,local_leds,sizeof(local_leds)); snprintf(message,capacity,tr(TXT_ENG_FIRST_SOURCE_CURRENT),value); ok=1;
    }
cleanup:
    if(file) fclose(file);
    if(log!=INVALID_HANDLE_VALUE) CloseHandle(log);
    if(pi.hThread) CloseHandle(pi.hThread);
    if(pi.hProcess) CloseHandle(pi.hProcess);
    if(input[0]) DeleteFileA(input);
    if(output[0]) DeleteFileA(output);
    free(buffer);
    return ok;
}

int spice_dc_analyze(const Workbench *bench,const char *executable,double *current,double led_currents[WORKBENCH_MAX_COMPONENTS],char *message,size_t capacity)
{
    return analyze(bench,executable,current,led_currents,NULL,NULL,0,0,(Pin){0,0},0,0,message,capacity);
}

int spice_dc_analyze_wires(const Workbench *bench,const char *executable,double *current,double component_currents[WORKBENCH_MAX_COMPONENTS],double wire_currents[WORKBENCH_MAX_WIRES],char *message,size_t capacity)
{
    if(!wire_currents) return 0;
    return analyze(bench,executable,current,component_currents,wire_currents,NULL,0,0,(Pin){0,0},0,0,message,capacity);
}

int spice_transient_analyze(const Workbench *bench,const char *executable,Pin probe,double step,double stop,SpiceTrace *trace,char *message,size_t capacity)
{
    if(!trace) return 0;
    return analyze(bench,executable,NULL,NULL,NULL,trace,0,0,probe,step,stop,message,capacity);
}

int spice_transient_current(const Workbench *bench,const char *executable,int component,double step,double stop,SpiceTrace *trace,char *message,size_t capacity)
{
    if(!trace) return 0;
    return analyze(bench,executable,NULL,NULL,NULL,trace,1,0,(Pin){component,0},step,stop,message,capacity);
}

int spice_transient_wire(const Workbench *bench,const char *executable,int wire,double step,double stop,SpiceTrace *trace,char *message,size_t capacity)
{
    if(!trace) return 0;
    return analyze(bench,executable,NULL,NULL,NULL,trace,2,1,(Pin){wire,0},step,stop,message,capacity);
}

int spice_transient_instrumented(const Workbench *bench,const char *executable,int mode,Pin probe,double step,double stop,SpiceTrace *trace,char *message,size_t capacity)
{
    if(!trace || mode<1 || mode>3) return 0;
    return analyze(bench,executable,NULL,NULL,NULL,trace,mode-1,1,probe,step,stop,message,capacity);
}
