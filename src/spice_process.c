#include "spice_process.h"
#include "netlist.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int analyze(const Workbench *bench,const char *executable,double *current,double led_currents[WORKBENCH_MAX_COMPONENTS],double *wire_currents,char *message,size_t capacity)
{
    char dir[MAX_PATH], input[MAX_PATH]="", output[MAX_PATH]="", exe[MAX_PATH], command[3*MAX_PATH+32], buffer[32768];
    DWORD length,exit_code=1; FILE *file=NULL; HANDLE log=INVALID_HANDLE_VALUE; STARTUPINFOA si={0}; PROCESS_INFORMATION pi={0};
    SECURITY_ATTRIBUTES security={sizeof(SECURITY_ATTRIBUTES),NULL,TRUE}; NetlistResult result; int ok=0;
    if(!bench || !executable || !current || !led_currents || !message || !capacity) return 0;
    snprintf(message,capacity,"Falha ao preparar analise.");
    length=GetFullPathNameA(executable,MAX_PATH,exe,NULL);
    if(!length || length>=MAX_PATH || GetFileAttributesA(exe)==INVALID_FILE_ATTRIBUTES) { snprintf(message,capacity,"ngspice nao encontrado."); return 0; }
    length=GetTempPathA(MAX_PATH,dir);
    if(!length || length>=MAX_PATH || !GetTempFileNameA(dir,"nci",0,input) || !GetTempFileNameA(dir,"nco",0,output)) goto cleanup;
    file=fopen(input,"w"); if(!file) goto cleanup;
    result=wire_currents?netlist_write_dc_instrumented(file,bench):netlist_write_dc(file,bench);
    if(fclose(file)!=0) result=NETLIST_IO_ERROR;
    file=NULL;
    if(result!=NETLIST_OK) { snprintf(message,capacity,result==NETLIST_AMBIGUOUS_WIRES?"Corrente ambigua em fios ideais paralelos.":"Montagem invalida ou nao suportada."); goto cleanup; }
    log=CreateFileA(output,GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
    if(log==INVALID_HANDLE_VALUE) goto cleanup;
    si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=log; si.hStdError=log; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    snprintf(command,sizeof(command),"\"%s\" -b \"%s\"",exe,input);
    if(!CreateProcessA(exe,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)) { snprintf(message,capacity,"Falha ao iniciar ngspice."); goto cleanup; }
    if(WaitForSingleObject(pi.hProcess,10000)!=WAIT_OBJECT_0) { TerminateProcess(pi.hProcess,1); WaitForSingleObject(pi.hProcess,5000); snprintf(message,capacity,"Analise excedeu 10 segundos."); goto cleanup; }
    GetExitCodeProcess(pi.hProcess,&exit_code);
    CloseHandle(log); log=INVALID_HANDLE_VALUE;
    if(exit_code!=0) { snprintf(message,capacity,"ngspice falhou."); goto cleanup; }
    file=fopen(output,"r"); if(!file) goto cleanup;
    length=(DWORD)fread(buffer,1,sizeof(buffer)-1,file); buffer[length]='\0';
    if(!feof(file) || ferror(file) || strstr(buffer,"Error:") || strstr(buffer,"error:") || strstr(buffer,"Warning:") || strstr(buffer,"warning:") || strstr(buffer,"couldn't be loaded") || strstr(buffer,"singular matrix") || strstr(buffer,"convergence failed")) { snprintf(message,capacity,"Resultado SPICE invalido ou incompleto."); goto cleanup; }
    {
        char *start=strstr(buffer,"@vsource[i] ="), *end;
        double value, local_leds[WORKBENCH_MAX_COMPONENTS]={0}, local_wires[WORKBENCH_MAX_WIRES]={0}; int i;
        if(!start) { snprintf(message,capacity,"Corrente da fonte indisponivel."); goto cleanup; }
        value=strtod(start+strlen("@vsource[i] ="),&end);
        if(end==start+strlen("@vsource[i] =") || !isfinite(value)) goto cleanup;
        for(i=0;i<bench->part_count;++i) if(bench->parts[i].type==PART_LED || bench->parts[i].type==PART_RESISTOR) {
            char key[32]; double led;
            snprintf(key,sizeof(key),bench->parts[i].type==PART_LED?"@d%d[id] =":"@r%d[i] =",i);
            start=strstr(buffer,key);
            if(!start) { snprintf(message,capacity,"Corrente de componente indisponivel."); goto cleanup; }
            led=strtod(start+strlen(key),&end);
            if(end==start+strlen(key) || !isfinite(led)) { snprintf(message,capacity,"Resultado de LED invalido."); goto cleanup; }
            local_leds[i]=led;
        }
        if(wire_currents) for(i=0;i<bench->wire_count;++i) {
            char key[32]; double wire;
            snprintf(key,sizeof(key),"@vwire%d[i] =",i);
            start=strstr(buffer,key);
            if(!start) { snprintf(message,capacity,"Corrente de fio indisponivel."); goto cleanup; }
            wire=strtod(start+strlen(key),&end);
            if(end==start+strlen(key) || !isfinite(wire)) { snprintf(message,capacity,"Corrente de fio invalida."); goto cleanup; }
            local_wires[i]=wire;
        }
        if(wire_currents) memcpy(wire_currents,local_wires,sizeof(local_wires));
        *current=value; memcpy(led_currents,local_leds,sizeof(local_leds)); snprintf(message,capacity,"Corrente da fonte: %.6g A (convencao SPICE).",value); ok=1;
    }
cleanup:
    if(file) fclose(file);
    if(log!=INVALID_HANDLE_VALUE) CloseHandle(log);
    if(pi.hThread) CloseHandle(pi.hThread);
    if(pi.hProcess) CloseHandle(pi.hProcess);
    if(input[0]) DeleteFileA(input);
    if(output[0]) DeleteFileA(output);
    return ok;
}

int spice_dc_analyze(const Workbench *bench,const char *executable,double *current,double led_currents[WORKBENCH_MAX_COMPONENTS],char *message,size_t capacity)
{
    return analyze(bench,executable,current,led_currents,NULL,message,capacity);
}

int spice_dc_analyze_wires(const Workbench *bench,const char *executable,double *current,double component_currents[WORKBENCH_MAX_COMPONENTS],double wire_currents[WORKBENCH_MAX_WIRES],char *message,size_t capacity)
{
    if(!wire_currents) return 0;
    return analyze(bench,executable,current,component_currents,wire_currents,message,capacity);
}
