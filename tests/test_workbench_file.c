#include <assert.h>
#include <stdio.h>
#include "workbench_file.h"

/* A person reopens a circuit saved before the shared palette: every legacy wire color, black included,
 * keeps its hue, and the next save writes the shared palette index. */
static void legacy_wire_colors(void)
{
    Workbench loaded={0};
    WorkbenchSettings settings;
    FILE *file=tmpfile(); assert(file);
    fputs("workbench 32\n2 2\n0 0 0 1 5 0 0 1 0\n4 200 0 0 1000 0 0 1 0\n0 0 1 0 0 2\n0 1 1 1 0 1\n"
          "groups 0\nmembers 0\nports 0\nleads 0\nstates 0\ncode 0\nportnames 0\nlayers 2\n0\n0\n"
          "settings 1 1 1 0 5 0 0 1e-05 0.005\ngrid 24 1 1\ntheme 0 13\n",file);
    rewind(file);
    assert(workbench_read_project(file,&loaded,&settings)); fclose(file);
    assert(loaded.wires[0].color==BENCH_COLOR_BLACK && loaded.wires[1].color==BENCH_COLOR_RED);
    assert(settings.wire_color==BENCH_COLOR_WHITE);
    assert(bench_color_from_legacy_wire(3)==BENCH_COLOR_GREEN && bench_color_from_legacy_wire(6)==-1);
    /* Black exists for wires only: an LED refuses it and keeps its color. */
    int led=bench_add(&loaded,PART_LED,400,0);
    assert(led>=0 && bench_set_states(&loaded,led,BENCH_COLOR_LIME) && !bench_set_states(&loaded,led,BENCH_COLOR_BLACK));
    assert(loaded.parts[led].states==BENCH_COLOR_LIME && bench_set_wire_color(&loaded,0,BENCH_COLOR_BLACK));
    bench_snapshot_clear(&loaded);
}

/* A computer-sized assembly: more parts and wires than the former 256/512 ceilings survive a save and reopen,
 * and the bench refuses the part past its capacity. */
static void large_bench(void)
{
    static Workbench source, loaded;
    int i;
    for (i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
        assert(bench_add(&source, PART_RESISTOR, (float)(i % 40) * 120, (float)(i / 40) * 60) == i);
    assert(bench_add(&source, PART_RESISTOR, 0, 0) < 0);
    for (i = 0; i + 1 < WORKBENCH_MAX_COMPONENTS; ++i)
    {
        assert(bench_toggle_wire(&source, (Pin){i, 1}, (Pin){i + 1, 0}) >= 0);
        assert(bench_toggle_wire(&source, (Pin){i, 0}, (Pin){i + 1, 1}) >= 0);
    }
    assert(source.part_count == 1024 && source.wire_count == 2046);
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &source));
    rewind(file);
    assert(workbench_read(file, &loaded));
    fclose(file);
    assert(loaded.part_count == 1024 && loaded.wire_count == 2046);
    assert(loaded.wires[2045].a.component == 1022 && loaded.wires[2045].b.pin == 1);
    bench_snapshot_clear(&source);
    bench_snapshot_clear(&loaded);
}

int main(void)
{
    legacy_wire_colors();
    large_bench();
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
        const char *invalid[]={"workbench 16\n0 0\n", "workbench 1\n65 0\n", "workbench 1\n1 0\n0 nan 0 1\n", "workbench 1\n0 0\nextra", "workbench 1\n2 2\n0 0 0 1\n2 0 0 0\n0 0 1 0 0\n1 0 0 0 0\n"};
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
    assert(loaded.parts[1].board_layout==0 && loaded.wires[0].color==BENCH_DEFAULT_WIRE_COLOR);
    { float x0,y0,x1,y1; bench_pin_position(&loaded.parts[1],0,&x0,&y0); bench_pin_position(&loaded.parts[1],1,&x1,&y1); assert(x0==x1 && y1-y0==7); }
    file=tmpfile(); assert(file);
    fputs("workbench 9\n2 1\n0 0 0 1 5 0 0\n3 100 200 0 0 0 1\n0 0 1 0 0 6\n",file); rewind(file);
    assert(!workbench_read(file,&loaded)); fclose(file);
    assert(loaded.parts[1].board_layout==0 && loaded.wires[0].color==BENCH_DEFAULT_WIRE_COLOR);
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
        custom.dark_theme=1; custom.grid_spacing=12.5; custom.snap_grid=1; custom.snap_board=0;
        custom.show_grid=0; custom.show_handles=0; custom.round_wires=0; custom.show_current=1;
        custom.wire_color=5; custom.probe_pin=1; custom.probe_current=1; custom.transient_step=2e-5; custom.transient_stop=.002;
        file=tmpfile(); assert(file); assert(workbench_write_project(file,&source,&custom)); rewind(file);
        assert(workbench_read_project(file,&loaded,&restored)); fclose(file);
        assert(!restored.show_grid && !restored.show_handles && !restored.round_wires && restored.show_current);
        assert(restored.wire_color==5 && restored.probe_pin==1 && restored.probe_current==1);
        assert(restored.transient_step==2e-5 && restored.transient_stop==.002);
        assert(restored.grid_spacing==12.5 && restored.snap_grid && !restored.snap_board && restored.dark_theme);
        file=tmpfile(); assert(file); fputs("workbench 9\n0 0\n",file); rewind(file);
        assert(workbench_read_project(file,&loaded,&restored)); fclose(file);
        assert(restored.show_grid && restored.transient_step==1e-5 && restored.transient_stop==.005);
        assert(restored.grid_spacing==24 && !restored.snap_grid && restored.snap_board);
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
    {
        Workbench gate={0}; float x,y;
        assert(bench_add(&gate,PART_AND,100,100)==0);
        assert(bench_pin_count(PART_AND)==3);
        bench_pin_position(&gate.parts[0],0,&x,&y); assert(x==51 && y==85);
        bench_pin_position(&gate.parts[0],2,&x,&y); assert(x==149 && y==100);
        assert(bench_rotate_part(&gate,0));
        bench_pin_position(&gate.parts[0],0,&x,&y); assert(x==115 && y==51);
        file=tmpfile(); assert(file); assert(workbench_write(file,&gate)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.parts[0].type==PART_AND && loaded.parts[0].rotation==1);
        file=tmpfile(); assert(file); fputs("workbench 10\n1 0\n7 100 100 0 0 0 0\nsettings 1 1 1 0 0 0 0 1e-5 .005\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
    }
    {
        Workbench gates={0}; int kind;
        for(kind=PART_NAND;kind<=PART_NOT;++kind) assert(bench_add(&gates,(PartType)kind,100+(kind-PART_NAND)*100,100)>=0);
        assert(bench_pin_count(PART_NOT)==2 && bench_pin_count(PART_XOR)==3);
        file=tmpfile(); assert(file); assert(workbench_write(file,&gates)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.part_count==4 && loaded.parts[0].type==PART_NAND && loaded.parts[3].type==PART_NOT);
        file=tmpfile(); assert(file); fputs("workbench 11\n1 0\n8 100 100 0 0 0 0\nsettings 1 1 1 0 0 0 0 1e-5 .005\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
        assert(loaded.part_count==4 && loaded.parts[0].type==PART_NAND);
    }
    {
        Workbench clock={0}; assert(bench_add(&clock,PART_CLOCK,100,100)==0);
        assert(clock.parts[0].on && clock.parts[0].value==1000 && bench_set_value(&clock,0,1000000));
        file=tmpfile(); assert(file); assert(workbench_write(file,&clock)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.parts[0].type==PART_CLOCK && loaded.parts[0].value==1000000 && loaded.parts[0].on);
        file=tmpfile(); assert(file); fputs("workbench 12\n1 0\n12 100 100 1 1000 0 0\nsettings 1 1 1 0 0 0 0 1e-5 .005\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
    }
    {
        Workbench gate={0}; float x,y;
        assert(bench_add(&gate,PART_DFF,100,100)==0 && bench_pin_count(PART_DFF)==3);
        bench_pin_position(&gate.parts[0],0,&x,&y); assert(x==51 && y==85);
        assert(bench_rotate_part(&gate,0));
        file=tmpfile(); assert(file); assert(workbench_write(file,&gate)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.parts[0].type==PART_DFF && loaded.parts[0].rotation==1);
        file=tmpfile(); assert(file); fputs("workbench 13\n1 0\n13 100 100 0 0 0 0\nsettings 1 1 1 0 0 0 0 1e-5 .005\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
    }
    {
        Workbench board={0}; Pin a,b; float x,y;
        assert(bench_add(&board,PART_BREADBOARD,500,400)==0);
        assert(bench_add(&board,PART_74HC00,500,400)==1);
        assert(bench_pin_count(PART_74HC00)==14);
        bench_pin_position(&board.parts[1],0,&x,&y); assert(x==458 && y==385);
        bench_pin_position(&board.parts[1],13,&x,&y); assert(x==458 && y==415);
        assert(bench_snap_to_breadboard(&board,1));
        assert(bench_find_contact(&board,1,0,&a) && bench_find_contact(&board,1,13,&b));
        assert(bench_board_group_first(a.pin)!=bench_board_group_first(b.pin));
        { int pin; for(pin=0;pin<14;++pin) { Pin contact; assert(bench_find_contact(&board,1,pin,&contact)); } }
        file=tmpfile(); assert(file); assert(workbench_write(file,&board)); rewind(file);
        assert(workbench_read(file,&loaded)); fclose(file);
        assert(loaded.parts[1].type==PART_74HC00);
        file=tmpfile(); assert(file); fputs("workbench 14\n1 0\n14 100 100 0 0 0 0\nsettings 1 1 1 0 0 0 0 1e-5 .005\n",file); rewind(file);
        assert(!workbench_read(file,&loaded)); fclose(file);
    }
    {
        WorkbenchSettings settings = workbench_settings_default();
        WorkbenchSettings before = settings;
        Workbench before_bench = loaded;
        file = tmpfile();
        assert(file);
        fputs("workbench 18\n0 0\ngroups 0\nmembers 0\nports 0\n"
              "settings 1 1 1 0 0 0 0 1e-5 .005\ngrid nan 1 1\n", file);
        rewind(file);
        assert(!workbench_read_project(file, &loaded, &settings));
        assert(settings.grid_spacing == before.grid_spacing && loaded.revision == before_bench.revision);
        fclose(file);
    }
    {
        Workbench scaled = {0};
        bench_add(&scaled, PART_RESISTOR, 300, 300);
        assert(bench_set_scale(&scaled, 0, .25f));
        file = tmpfile();
        assert(file && workbench_write(file, &scaled));
        rewind(file);
        assert(workbench_read(file, &loaded));
        assert(bench_part_scale(&loaded.parts[0]) == .25f);
        fclose(file);
        file = tmpfile();
        assert(file);
        fputs("workbench 19\n1 0\n4 300 300 0 1000 0 0 nan\n", file);
        rewind(file);
        uint64_t revision = loaded.revision;
        assert(!workbench_read(file, &loaded) && loaded.revision == revision);
        fclose(file);
    }
    return 0;
}
