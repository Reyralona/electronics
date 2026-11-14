#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "netlist.h"

int main(void)
{
    Workbench b={0}; FILE *f; char text[4096]; size_t n;
    assert(bench_add(&b,PART_SOURCE,0,0)==0);
    assert(bench_add(&b,PART_RESISTOR,0,0)==1);
    assert(bench_add(&b,PART_BREADBOARD,0,0)==2);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
    assert(bench_toggle_wire(&b,(Pin){2,4},(Pin){1,0})==1);
    assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){0,1})==1);
    f=fopen("build/netlist-reference.cir","w+"); assert(f);
    assert(netlist_write_dc(f,&b)==NETLIST_OK); rewind(f);
    n=fread(text,1,sizeof(text)-1,f); text[n]='\0'; fclose(f);
    assert(strstr(text,"Vsource") && strstr(text,"R1 ") && strstr(text," 1000"));
    assert(bench_add(&b,PART_LED,0,0)==3);
    f=tmpfile(); assert(f); assert(netlist_write_dc(f,&b)==NETLIST_UNSUPPORTED); fclose(f);
    bench_remove(&b,3);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){0,1})==1);
    f=tmpfile(); assert(f); assert(netlist_write_dc(f,&b)==NETLIST_INVALID); fclose(f);
    return 0;
}
