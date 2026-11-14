#include <assert.h>
#include <stdio.h>
#include "workbench_file.h"

int main(void)
{
    Workbench source={0}, loaded={0}; FILE *file;
    assert(bench_add(&source,PART_SOURCE,10,20)==0);
    assert(bench_add(&source,PART_BREADBOARD,100,200)==1);
    assert(bench_toggle_wire(&source,(Pin){0,0},(Pin){1,99})==1);
    source.wires[0].knee_count=1;
    source.wires[0].knees[0]=(WirePoint){50,75};
    file=tmpfile(); assert(file);
    assert(workbench_write(file,&source)); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==2 && loaded.wire_count==1);
    assert(loaded.wires[0].b.pin==99 && loaded.wires[0].knees[0].x==50);
    file=tmpfile(); assert(file);
    fputs("workbench 1\n1 1\n0 0 0 1\n0 0 99 0 0\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==2 && loaded.wires[0].b.pin==99);
    {
        const char *invalid[]={"workbench 9\n0 0\n", "workbench 1\n65 0\n", "workbench 1\n1 0\n0 nan 0 1\n", "workbench 1\n0 0\nextra", "workbench 1\n2 2\n0 0 0 1\n2 0 0 0\n0 0 1 0 0\n1 0 0 0 0\n"};
        unsigned int i;
        for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
            file=tmpfile(); assert(file); fputs(invalid[i],file); rewind(file);
            assert(!workbench_read(file,&loaded)); fclose(file);
            assert(loaded.part_count==2 && loaded.wires[0].b.pin==99);
        }
    }
    assert(workbench_save("build/test-bench.elec",&source));
    loaded.part_count=0; assert(workbench_load("build/test-bench.elec",&loaded));
    assert(loaded.part_count==2 && loaded.wires[0].knee_count==1);
    assert(remove("build/test-bench.elec")==0);
    assert(bench_add(&source,PART_RESISTOR,300,100)==2);
    assert(bench_add(&source,PART_CAPACITOR,400,100)==3);
    assert(!bench_boolean_supported(&source));
    file=tmpfile(); assert(file); assert(workbench_write(file,&source)); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==4 && loaded.parts[2].type==PART_RESISTOR && loaded.parts[3].type==PART_CAPACITOR);
    file=tmpfile(); assert(file); fputs("workbench 1\n1 0\n4 0 0 0\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==4);
    assert(bench_add(&source,PART_INDUCTOR,500,100)==4);
    assert(!bench_boolean_supported(&source));
    file=tmpfile(); assert(file); assert(workbench_write(file,&source)); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==5 && loaded.parts[4].type==PART_INDUCTOR);
    file=tmpfile(); assert(file); fputs("workbench 2\n1 0\n6 0 0 0\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==5);
    return 0;
}
