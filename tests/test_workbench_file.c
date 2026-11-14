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
    source.wires[0].color=3;
    source.wires[0].knees[0]=(WirePoint){50,75};
    file=tmpfile(); assert(file);
    assert(workbench_write(file,&source)); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==2 && loaded.wire_count==1);
    assert(loaded.wires[0].b.pin==99 && loaded.wires[0].knees[0].x==50);
    assert(loaded.wires[0].color==3 && loaded.parts[1].board_layout==1);
    { float x0,y0,x1,y1; bench_pin_position(&loaded.parts[1],0,&x0,&y0); bench_pin_position(&loaded.parts[1],1,&x1,&y1); assert(x0==x1 && y1-y0==10); }
    file=tmpfile(); assert(file);
    fputs("workbench 1\n1 1\n0 0 0 1\n0 0 99 0 0\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==2 && loaded.wires[0].b.pin==99);
    {
        const char *invalid[]={"workbench 11\n0 0\n", "workbench 1\n65 0\n", "workbench 1\n1 0\n0 nan 0 1\n", "workbench 1\n0 0\nextra", "workbench 1\n2 2\n0 0 0 1\n2 0 0 0\n0 0 1 0 0\n1 0 0 0 0\n"};
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
    assert(bench_rotate_part(&source,4));
    assert(bench_set_value(&source,2,2200));
    assert(bench_set_value(&source,3,2.2e-6));
    assert(!bench_set_value(&source,1,1));
    assert(!bench_set_value(&source,2,-1));
    assert(!bench_boolean_supported(&source));
    file=tmpfile(); assert(file); assert(workbench_write(file,&source)); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==5 && loaded.parts[4].type==PART_INDUCTOR && loaded.parts[4].rotation==1);
    assert(loaded.parts[2].value==2200 && loaded.parts[3].value==2.2e-6);
    file=tmpfile(); assert(file); fputs("workbench 2\n1 0\n6 0 0 0\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==5);
    {
        Workbench extended={0};
        assert(bench_add(&extended,PART_SOURCE,0,0)==0);
        assert(bench_add(&extended,PART_BREADBOARD,0,0)==1);
        assert(bench_toggle_wire(&extended,(Pin){0,0},(Pin){1,179})==1);
        file=tmpfile(); assert(file); assert(workbench_write(file,&extended)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.wire_count==1 && loaded.wires[0].b.pin==179);
        file=tmpfile(); assert(file);
        fputs("workbench 4\n2 1\n0 0 0 1 5\n3 0 0 0 0\n0 0 1 179 0\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
        assert(loaded.wires[0].b.pin==179);
        assert(bench_toggle_wire(&extended,(Pin){0,1},(Pin){1,299})==1);
        file=tmpfile(); assert(file); assert(workbench_write(file,&extended)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.wire_count==2 && loaded.wires[1].b.pin==299);
        file=tmpfile(); assert(file);
        fputs("workbench 5\n2 1\n0 0 0 1 5\n3 0 0 0 0\n0 0 1 299 0\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
        assert(loaded.wire_count==2 && loaded.wires[1].b.pin==299);
        assert(bench_toggle_wire(&extended,(Pin){0,1},(Pin){1,419})==1);
        file=tmpfile(); assert(file); assert(workbench_write(file,&extended)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.wires[2].b.pin==419);
        file=tmpfile(); assert(file);
        fputs("workbench 6\n2 1\n0 0 0 1 5\n3 0 0 0 0\n0 0 1 419 0\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
        assert(loaded.wires[2].b.pin==419);
    }
    file=tmpfile(); assert(file); fputs("workbench 3\n2 0\n0 0 0 1\n4 0 0 0\n",file); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.parts[0].value==5 && loaded.parts[1].value==1000);
    assert(loaded.parts[0].rotation==0 && loaded.parts[1].rotation==0);
    file=tmpfile(); assert(file); fputs("workbench 7\n1 0\n0 0 0 1 5\n",file); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==1 && loaded.parts[0].rotation==0);
    file=tmpfile(); assert(file);
    fputs("workbench 8\n2 1\n0 0 0 1 5 0\n3 100 200 0 0 0\n0 0 1 0 0\n",file); rewind(file);
    assert(workbench_read(file,&loaded)); fclose(file);
    assert(loaded.parts[1].board_layout==0 && loaded.wires[0].color==0);
    { float x0,y0,x1,y1; bench_pin_position(&loaded.parts[1],0,&x0,&y0); bench_pin_position(&loaded.parts[1],1,&x1,&y1); assert(x0==x1 && y1-y0==7); }
    file=tmpfile(); assert(file);
    fputs("workbench 9\n2 1\n0 0 0 1 5 0 0\n3 100 200 0 0 0 1\n0 0 1 0 0 6\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.parts[1].board_layout==0 && loaded.wires[0].color==0);
    file=tmpfile(); assert(file); fputs("workbench 8\n1 0\n3 0 0 0 0 1\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.part_count==2 && loaded.parts[0].rotation==0);
    {
        Workbench snapped={0}; Pin contact;
        assert(bench_add(&snapped,PART_BREADBOARD,500,400)==0);
        assert(bench_add(&snapped,PART_RESISTOR,488,347)==1);
        assert(bench_snap_to_breadboard(&snapped,1));
        file=tmpfile(); assert(file); assert(workbench_write(file,&snapped)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(bench_find_contact(&loaded,1,0,&contact) && contact.pin==0);
        assert(bench_find_contact(&loaded,1,1,&contact) && contact.pin==35);
    }
    {
        WorkbenchSettings custom=workbench_settings_default(), restored=workbench_settings_default();
        custom.show_grid=0; custom.show_handles=0; custom.round_wires=0; custom.show_current=1;
        custom.wire_color=5; custom.probe_pin=1; custom.probe_current=1; custom.transient_step=2e-5; custom.transient_stop=.002;
        file=tmpfile(); assert(file); assert(workbench_write_project(file,&source,&custom)); rewind(file);
        assert(workbench_read_project(file,&loaded,&restored)); fclose(file);
        assert(!restored.show_grid && !restored.show_handles && !restored.round_wires && restored.show_current);
        assert(restored.wire_color==5 && restored.probe_pin==1 && restored.probe_current==1);
        assert(restored.transient_step==2e-5 && restored.transient_stop==.002);
        file=tmpfile(); assert(file); fputs("workbench 9\n0 0\n",file); rewind(file);
        assert(workbench_read_project(file,&loaded,&restored)); fclose(file);
        assert(restored.show_grid && restored.transient_step==1e-5 && restored.transient_stop==.005);
        restored.wire_color=4;
        file=tmpfile(); assert(file); fputs("workbench 10\n0 0\nsettings 1 1 1 0 2 0 0 1e-9 1\n",file); rewind(file);
        assert(!workbench_read_project(file,&loaded,&restored)); fclose(file);
        assert(restored.wire_color==4 && loaded.part_count==0);
        file=tmpfile(); assert(file); fputs("workbench 10\n0 0\nsettings 1 1 1 0 2 0 0 nan .005\n",file); rewind(file);
        assert(!workbench_read_project(file,&loaded,&restored)); fclose(file);
        custom.transient_step=1; custom.transient_stop=.001;
        file=tmpfile(); assert(file); assert(!workbench_write_project(file,&source,&custom)); fclose(file);
        assert(workbench_save_project("build/test-settings.elec",&source,&restored));
        restored.wire_color=0;
        assert(workbench_load_project("build/test-settings.elec",&loaded,&restored));
        assert(restored.wire_color==4); assert(remove("build/test-settings.elec")==0);
    }
    return 0;
}
