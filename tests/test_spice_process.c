#include <assert.h>
#include <math.h>
#include <string.h>
#include "spice_process.h"
#include "ui_text.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    Workbench b={0}; char message[128]; double current=42, led[WORKBENCH_MAX_COMPONENTS]={0}, wire[WORKBENCH_MAX_WIRES]={0};
    assert(spice_wire_current_level(0)==0 && spice_wire_current_level(0.999e-6)==0);
    assert(spice_wire_current_level(1e-6)==1 && spice_wire_current_level(-99e-6)==1);
    assert(spice_wire_current_level(1e-4)==2 && spice_wire_current_level(-.009)==2);
    assert(spice_wire_current_level(.01)==3 && spice_wire_current_level(-1)==3);
    assert(spice_wire_current_level(NAN)==0);
    assert(bench_add(&b, PART_SOURCE, 250 * b.part_count, 0)==0);
    assert(bench_add(&b, PART_RESISTOR, 250 * b.part_count, 0)==1);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==1);
    assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==1);
    assert(!spice_dc_analyze(&b,"build/missing-ngspice.exe",&current,led,message,sizeof(message)));
    assert(current==42 && !strcmp(message,tr(TXT_ENG_SOLVER_MISSING)));
    if(argc>1) {
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(current+.005)<1e-7);
        assert(fabs(led[1]-.005)<1e-7);
        assert(spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(fabs(wire[0]-.005)<1e-7 && fabs(wire[1]+.005)<1e-7);
        assert(bench_add(&b, PART_RESISTOR, 250 * b.part_count, 0)==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){2,1})==1);
        assert(spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(fabs(current+.01)<1e-7 && fabs(wire[0]-.005)<1e-7 && fabs(wire[2]-.005)<1e-7);
        bench_remove(&b,2);
        assert(bench_add(&b, PART_BREADBOARD, 250 * b.part_count, 0)==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){2,1},(Pin){1,0})==1);
        assert(!spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(!strcmp(message,tr(TXT_ENG_AMBIGUOUS_WIRES)));
        assert(fabs(wire[0]-.005)<1e-7);
        { SpiceTrace ambiguous={0};
          assert(!spice_transient_wire(&b,argv[1],0,1e-5,.005,&ambiguous,message,sizeof(message)));
          assert(!strcmp(message,tr(TXT_ENG_AMBIGUOUS_WIRES)));
        }
        bench_remove(&b,2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==2);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,1})==1);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,0})==1);
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(led[1]+.005)<1e-7);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,1})==2);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,0})==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==1);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==1);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){0,1})==1);
        assert(!spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(current+.005)<1e-7);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){0,1})==2);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==2);
        assert(bench_add(&b, PART_LED, 250 * b.part_count, 0)==2);
        assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){2,1},(Pin){0,1})==1);
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(led[2]>.003 && led[2]<.004);
        assert(led[1]>.003 && led[1]<.004);
        { SpiceTrace led_trace={0};
          assert(spice_transient_current(&b,argv[1],2,1e-5,.001,&led_trace,message,sizeof(message)));
          assert(led_trace.samples[led_trace.count-1].value>.003 && led_trace.samples[led_trace.count-1].value<.004);
        }
        assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){2,0})==2);
        assert(bench_toggle_wire(&b,(Pin){2,1},(Pin){0,1})==2);
        assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){2,1})==1);
        assert(bench_toggle_wire(&b,(Pin){2,0},(Pin){0,1})==1);
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(led[2])<1e-8);
    }
    if(argc>1) {
        Workbench rc={0}; SpiceTrace trace={0}; int i, nearest=0;
        assert(bench_add(&rc, PART_SOURCE, 250 * rc.part_count, 0)==0);
        assert(bench_add(&rc, PART_RESISTOR, 250 * rc.part_count, 0)==1);
        assert(bench_add(&rc, PART_CAPACITOR, 250 * rc.part_count, 0)==2);
        assert(bench_toggle_wire(&rc,(Pin){0,0},(Pin){1,0})==1);
        assert(bench_toggle_wire(&rc,(Pin){1,1},(Pin){2,0})==1);
        assert(bench_toggle_wire(&rc,(Pin){2,1},(Pin){0,1})==1);
        assert(spice_transient_analyze(&rc,argv[1],(Pin){2,0},1e-5,.005,&trace,message,sizeof(message)));
        assert(trace.count>2);
        for(i=1;i<trace.count;++i) if(fabs(trace.samples[i].time-.001)<fabs(trace.samples[nearest].time-.001)) nearest=i;
        assert(fabs(trace.samples[nearest].value-5*(1-exp(-trace.samples[nearest].time/.001)))<.01);
        i=trace.count;
        assert(!spice_transient_analyze(&rc,argv[1],(Pin){2,0},0,.005,&trace,message,sizeof(message)));
        assert(trace.count==i);
        assert(spice_transient_wire(&rc,argv[1],0,1e-5,.005,&trace,message,sizeof(message)));
        nearest=0;
        for(i=1;i<trace.count;++i) if(fabs(trace.samples[i].time-.001)<fabs(trace.samples[nearest].time-.001)) nearest=i;
        assert(fabs(trace.samples[nearest].value-.005*exp(-trace.samples[nearest].time/.001))<1e-5);
        assert(spice_transient_wire(&rc,argv[1],2,1e-5,.005,&trace,message,sizeof(message)));
        assert(trace.samples[0].value>0);
        i=trace.count;
        assert(!spice_transient_wire(&rc,argv[1],99,1e-5,.005,&trace,message,sizeof(message)));
        assert(trace.count==i);
        assert(spice_transient_current(&rc,argv[1],1,1e-5,.005,&trace,message,sizeof(message)));
        nearest=0;
        for(i=1;i<trace.count;++i) if(fabs(trace.samples[i].time-.001)<fabs(trace.samples[nearest].time-.001)) nearest=i;
        assert(fabs(trace.samples[nearest].value-.005*exp(-trace.samples[nearest].time/.001))<1e-5);
        assert(spice_transient_current(&rc,argv[1],2,1e-5,.005,&trace,message,sizeof(message)));
        nearest=0;
        for(i=1;i<trace.count;++i) if(fabs(trace.samples[i].time-.001)<fabs(trace.samples[nearest].time-.001)) nearest=i;
        assert(fabs(trace.samples[nearest].value-.005*exp(-trace.samples[nearest].time/.001))<1e-5);
        assert(spice_transient_current(&rc,argv[1],0,1e-5,.005,&trace,message,sizeof(message)));
        assert(trace.samples[0].value<0);
        assert(spice_transient_analyze(&rc,argv[1],(Pin){2,1},2e-5,.005,&trace,message,sizeof(message)));
        for(i=0;i<trace.count;++i) assert(fabs(trace.samples[i].value)<1e-12);
        bench_remove(&rc,2);
        assert(bench_add(&rc, PART_INDUCTOR, 250 * rc.part_count, 0)==2);
        assert(bench_toggle_wire(&rc,(Pin){1,1},(Pin){2,0})==1);
        assert(bench_toggle_wire(&rc,(Pin){2,1},(Pin){0,1})==1);
        assert(spice_transient_analyze(&rc,argv[1],(Pin){2,0},1e-7,5e-5,&trace,message,sizeof(message)));
        nearest=0;
        for(i=1;i<trace.count;++i) if(fabs(trace.samples[i].time-1e-5)<fabs(trace.samples[nearest].time-1e-5)) nearest=i;
        assert(fabs(trace.samples[nearest].value-5*exp(-trace.samples[nearest].time/1e-5))<.01);
        assert(spice_transient_current(&rc,argv[1],2,1e-7,5e-5,&trace,message,sizeof(message)));
        nearest=0;
        for(i=1;i<trace.count;++i) if(fabs(trace.samples[i].time-1e-5)<fabs(trace.samples[nearest].time-1e-5)) nearest=i;
        assert(fabs(trace.samples[nearest].value-.005*(1-exp(-trace.samples[nearest].time/1e-5)))<1e-5);
        assert(bench_add(&rc, PART_SWITCH, 250 * rc.part_count, 0)==3);
        i=trace.count;
        assert(!spice_transient_current(&rc,argv[1],3,1e-7,5e-5,&trace,message,sizeof(message)));
        assert(trace.count==i);
    }
    if(argc>1) {
        PartType type;
        for(type=PART_AND;type<=PART_NOT;type++) {
            int bits;
            for(bits=0;bits<(type==PART_NOT?2:4);++bits) {
                Workbench logic={0}; SpiceTrace measured={0}; int i,high,output=type==PART_NOT?1:2;
                high=type==PART_AND?bits==3:type==PART_NAND?bits!=3:type==PART_OR?bits!=0:type==PART_XOR?(bits==1 || bits==2):bits==0;
                assert(bench_add(&logic, PART_SOURCE, 250 * logic.part_count, 0)==0);
                assert(bench_add(&logic,type,250,0)==1);
                assert(bench_add(&logic, PART_RESISTOR, 250 * logic.part_count, 0)==2);
                assert(bench_add(&logic, PART_LED, 250 * logic.part_count, 0)==3);
                assert(bench_toggle_wire(&logic,(Pin){0,(bits&1)?0:1},(Pin){1,0})==1);
                if(type!=PART_NOT) assert(bench_toggle_wire(&logic,(Pin){0,(bits&2)?0:1},(Pin){1,1})==1);
                assert(bench_toggle_wire(&logic,(Pin){1,output},(Pin){2,0})==1);
                assert(bench_toggle_wire(&logic,(Pin){2,1},(Pin){3,0})==1);
                assert(bench_toggle_wire(&logic,(Pin){3,1},(Pin){0,1})==1);
                int analyzed = spice_transient_analyze(&logic, argv[1], (Pin){1, output}, 1e-5, .001,
                                                       &measured, message, sizeof(message));
                if (!analyzed)
                    fprintf(stderr, "Gate %d, inputs %d: %s\n", type, bits, message);
                assert(analyzed);
                i=measured.count-1;
                if(high) assert(measured.samples[i].value>4.9); else assert(fabs(measured.samples[i].value)<.01);
                assert(spice_dc_analyze(&logic,argv[1],&current,led,message,sizeof(message)));
                if(high) assert(led[3]>.003 && led[3]<.004); else assert(fabs(led[3])<1e-8);
            }
        }
        {
            Workbench cascade={0}; SpiceTrace measured={0};
            assert(bench_add(&cascade, PART_SOURCE, 250 * cascade.part_count, 0)==0);
            assert(bench_add(&cascade, PART_AND, 250 * cascade.part_count, 0)==1);
            assert(bench_add(&cascade, PART_NOT, 250 * cascade.part_count, 0)==2);
            assert(bench_add(&cascade, PART_RESISTOR, 250 * cascade.part_count, 0)==3);
            assert(bench_add(&cascade, PART_LED, 250 * cascade.part_count, 0)==4);
            assert(bench_toggle_wire(&cascade,(Pin){0,0},(Pin){1,0})==1);
            assert(bench_toggle_wire(&cascade,(Pin){0,1},(Pin){1,1})==1);
            assert(bench_toggle_wire(&cascade,(Pin){1,2},(Pin){2,0})==1);
            assert(bench_toggle_wire(&cascade,(Pin){2,1},(Pin){3,0})==1);
            assert(bench_toggle_wire(&cascade,(Pin){3,1},(Pin){4,0})==1);
            assert(bench_toggle_wire(&cascade,(Pin){4,1},(Pin){0,1})==1);
            assert(spice_transient_analyze(&cascade,argv[1],(Pin){2,1},1e-5,.001,&measured,message,sizeof(message)));
            assert(measured.samples[measured.count-1].value>4.9);
            assert(spice_dc_analyze(&cascade,argv[1],&current,led,message,sizeof(message)));
            assert(led[4]>.003 && led[4]<.004);
        }
    }
    if(argc>1) {
        Workbench clock={0}; SpiceTrace pulse={0}, inverted={0}; int k,j;
        double times[]={.00025,.00075,.00125,.00175}, expected[]={5,0,5,0};
        assert(bench_add(&clock, PART_SOURCE, 250 * clock.part_count, 0)==0);
        assert(bench_add(&clock, PART_CLOCK, 250 * clock.part_count, 0)==1);
        assert(bench_add(&clock, PART_NOT, 250 * clock.part_count, 0)==2);
        assert(bench_toggle_wire(&clock,(Pin){0,1},(Pin){1,1})==1);
        assert(bench_toggle_wire(&clock,(Pin){1,0},(Pin){2,0})==1);
        assert(spice_transient_analyze(&clock,argv[1],(Pin){1,0},1e-5,.002,&pulse,message,sizeof(message)));
        assert(spice_transient_analyze(&clock,argv[1],(Pin){2,1},1e-5,.002,&inverted,message,sizeof(message)));
        for(k=0;k<4;++k) {
            int a=0,b=0;
            for(j=1;j<pulse.count;++j) if(fabs(pulse.samples[j].time-times[k])<fabs(pulse.samples[a].time-times[k])) a=j;
            for(j=1;j<inverted.count;++j) if(fabs(inverted.samples[j].time-times[k])<fabs(inverted.samples[b].time-times[k])) b=j;
            assert(fabs(pulse.samples[a].time-times[k])<1e-5 && fabs(pulse.samples[a].value-expected[k])<.01);
            assert(fabs(inverted.samples[b].value-(5-expected[k]))<.01);
        }
        clock.parts[1].on=0;
        assert(spice_transient_analyze(&clock,argv[1],(Pin){1,0},1e-5,.002,&pulse,message,sizeof(message)));
        for(j=0;j<pulse.count;++j) assert(fabs(pulse.samples[j].value)<1e-9);
        clock.parts[1].on=1; assert(bench_set_value(&clock,1,2000));
        assert(spice_transient_analyze(&clock,argv[1],(Pin){1,0},1e-5,.002,&pulse,message,sizeof(message)));
        k=0; for(j=1;j<pulse.count;++j) if(fabs(pulse.samples[j].time-.00075)<fabs(pulse.samples[k].time-.00075)) k=j;
        assert(pulse.samples[k].value>4.9);
        bench_remove(&clock,0);
        assert(bench_set_value(&clock,0,1000000));
        assert(spice_transient_analyze(&clock,argv[1],(Pin){0,0},1e-8,2e-6,&pulse,message,sizeof(message)));
        k=0; for(j=1;j<pulse.count;++j) if(fabs(pulse.samples[j].time-1.25e-6)<fabs(pulse.samples[k].time-1.25e-6)) k=j;
        assert(fabs(pulse.samples[k].time-1.25e-6)<1e-8 && pulse.samples[k].value>4.9);
        assert(spice_transient_analyze(&clock,argv[1],(Pin){1,1},1e-8,2e-6,&inverted,message,sizeof(message)));
        k=0; for(j=1;j<inverted.count;++j) if(fabs(inverted.samples[j].time-1.75e-6)<fabs(inverted.samples[k].time-1.75e-6)) k=j;
        assert(inverted.samples[k].value>4.9);
    }
    if(argc>1) {
        Workbench circuit={0}; SpiceTrace trace={0}; int i,high=0,low=0;
        assert(bench_add(&circuit, PART_CLOCK, 250 * circuit.part_count, 0)==0);
        assert(bench_add(&circuit, PART_NOT, 250 * circuit.part_count, 0)==1);
        assert(bench_add(&circuit, PART_RESISTOR, 250 * circuit.part_count, 0)==2);
        assert(bench_add(&circuit, PART_LED, 250 * circuit.part_count, 0)==3);
        assert(bench_add(&circuit, PART_RESISTOR, 250 * circuit.part_count, 0)==4);
        assert(bench_add(&circuit, PART_LED, 250 * circuit.part_count, 0)==5);
        assert(bench_toggle_wire(&circuit,(Pin){0,0},(Pin){1,0})==1);
        assert(bench_toggle_wire(&circuit,(Pin){0,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&circuit,(Pin){2,1},(Pin){3,0})==1);
        assert(bench_toggle_wire(&circuit,(Pin){3,1},(Pin){0,1})==1);
        assert(bench_toggle_wire(&circuit,(Pin){1,1},(Pin){4,0})==1);
        assert(bench_toggle_wire(&circuit,(Pin){4,1},(Pin){5,0})==1);
        assert(bench_toggle_wire(&circuit,(Pin){5,1},(Pin){0,1})==1);
        assert(spice_transient_analyze(&circuit,argv[1],(Pin){0,0},1e-5,.002,&trace,message,sizeof(message)));
        for(i=1;i<trace.count;++i) {
            if(fabs(trace.samples[i].time-.00025)<fabs(trace.samples[high].time-.00025)) high=i;
            if(fabs(trace.samples[i].time-.00075)<fabs(trace.samples[low].time-.00075)) low=i;
        }
        assert(trace.samples[high].value>4.9 && trace.samples[low].value<.01);
        assert((trace.leds_on[high] & (UINT64_C(1)<<3)) && !(trace.leds_on[high] & (UINT64_C(1)<<5)));
        assert(!(trace.leds_on[low] & (UINT64_C(1)<<3)) && (trace.leds_on[low] & (UINT64_C(1)<<5)));
        assert(bench_toggle_wire(&circuit,(Pin){0,0},(Pin){2,0})==2);
        assert(bench_toggle_wire(&circuit,(Pin){2,0},(Pin){0,0})==1);
        assert(spice_transient_instrumented(&circuit,argv[1],1,(Pin){0,0},1e-5,.002,&trace,message,sizeof(message)));
        assert(trace.measures_wires);
        high=low=0;
        for(i=1;i<trace.count;++i) {
            if(fabs(trace.samples[i].time-.00025)<fabs(trace.samples[high].time-.00025)) high=i;
            if(fabs(trace.samples[i].time-.00075)<fabs(trace.samples[low].time-.00075)) low=i;
        }
        /* Last wire was inserted with its endpoints reversed. */
        assert(trace.wires_reverse[high][0] & (UINT64_C(1)<<6));
        assert(((trace.wire_levels[high][6/4]>>((6%4)*2))&3)==2);
        assert(((trace.wire_levels[low][6/4]>>((6%4)*2))&3)==0);
        assert(!(trace.wires_reverse[low][0] & (UINT64_C(1)<<6)));
        assert(trace.leds_on[high] & (UINT64_C(1)<<3));
        assert(trace.leds_on[low] & (UINT64_C(1)<<5));
    }
    if(argc>1) {
        Workbench branches={0}; SpiceTrace levels={0}; int i;
        assert(bench_add(&branches, PART_SOURCE, 250 * branches.part_count, 0)==0);
        for(i=0;i<3;++i) {
            int r=bench_add(&branches, PART_RESISTOR, 250 * branches.part_count, 0); assert(r==i+1);
            assert(bench_set_value(&branches,r,i==0?1000000:i==1?1000:100));
            assert(bench_toggle_wire(&branches,(Pin){0,0},(Pin){r,0})==1);
            assert(bench_toggle_wire(&branches,(Pin){r,1},(Pin){0,1})==1);
        }
        assert(spice_transient_instrumented(&branches,argv[1],1,(Pin){0,0},1e-5,.0001,&levels,message,sizeof(message)));
        for(i=0;i<3;++i) { int wire=i*2; assert(((levels.wire_levels[0][wire/4]>>((wire%4)*2))&3)==i+1); }
        assert(spice_dc_analyze_wires(&branches,argv[1],&current,led,wire,message,sizeof(message)));
        for(i=0;i<3;++i) assert(spice_wire_current_level(wire[i*2])==i+1);
    }
    if(argc>1) {
        Workbench seq={0}; SpiceTrace trace={0}; double times[]={.0011,.0015,.00225,.00325}, expected[]={5,5,0,5}; int i,j;
        assert(bench_add(&seq, PART_CLOCK, 250 * seq.part_count, 0)==0);
        assert(bench_add(&seq, PART_CLOCK, 250 * seq.part_count, 0)==1);
        assert(bench_set_value(&seq,1,400));
        assert(bench_add(&seq, PART_DFF, 250 * seq.part_count, 0)==2);
        assert(bench_add(&seq, PART_RESISTOR, 250 * seq.part_count, 0)==3);
        assert(bench_add(&seq, PART_LED, 250 * seq.part_count, 0)==4);
        assert(bench_toggle_wire(&seq,(Pin){0,1},(Pin){1,1})==1);
        assert(bench_toggle_wire(&seq,(Pin){1,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&seq,(Pin){0,0},(Pin){2,1})==1);
        assert(bench_toggle_wire(&seq,(Pin){2,2},(Pin){3,0})==1);
        assert(bench_toggle_wire(&seq,(Pin){3,1},(Pin){4,0})==1);
        assert(bench_toggle_wire(&seq,(Pin){4,1},(Pin){0,1})==1);
        assert(spice_transient_instrumented(&seq,argv[1],1,(Pin){2,2},1e-5,.0035,&trace,message,sizeof(message)));
        for(i=0;i<4;++i) {
            int nearest=0; for(j=1;j<trace.count;++j) if(fabs(trace.samples[j].time-times[i])<fabs(trace.samples[nearest].time-times[i])) nearest=j;
            assert(fabs(trace.samples[nearest].time-times[i])<1e-5);
            assert(fabs(trace.samples[nearest].value-expected[i])<.01);
            assert(((trace.leds_on[nearest]>>4)&1)==(expected[i]>0));
        }
    }
    if(argc>1) {
        static const int pins[4][3]={{0,1,2},{3,4,5},{8,9,7},{11,12,10}};
        Workbench chip={0}; SpiceTrace trace={0}; int g;
        assert(bench_add(&chip, PART_SOURCE, 250 * chip.part_count, 0)==0);
        assert(bench_add(&chip, PART_74HC00, 250 * chip.part_count, 0)==1);
        assert(bench_toggle_wire(&chip,(Pin){0,0},(Pin){1,13})==1);
        assert(bench_toggle_wire(&chip,(Pin){0,1},(Pin){1,6})==1);
        for(g=0;g<4;++g) {
            int r=bench_add(&chip, PART_RESISTOR, 250 * chip.part_count, 0); assert(r==g+2);
            assert(bench_toggle_wire(&chip,(Pin){0,(g&1)?0:1},(Pin){1,pins[g][0]})==1);
            assert(bench_toggle_wire(&chip,(Pin){0,(g&2)?0:1},(Pin){1,pins[g][1]})==1);
            assert(bench_toggle_wire(&chip,(Pin){1,pins[g][2]},(Pin){r,0})==1);
            assert(bench_toggle_wire(&chip,(Pin){r,1},(Pin){0,1})==1);
        }
        assert(spice_dc_analyze(&chip,argv[1],&current,led,message,sizeof(message)));
        for(g=0;g<4;++g) { if(g<3) assert(fabs(led[g+2]-5.0/1050)<1e-6); else assert(fabs(led[g+2])<1e-8); }
        assert(fabs(current+3*5.0/1050)<1e-6);
        assert(spice_transient_instrumented(&chip,argv[1],1,(Pin){1,2},1e-5,.0001,&trace,message,sizeof(message)));
        assert(fabs(trace.samples[trace.count-1].value-5.0*1000/1050)<.01);
        assert(trace.wires_forward[0][0] & 1);
        assert(bench_toggle_wire(&chip,(Pin){0,0},(Pin){1,13})==2);
        assert(bench_toggle_wire(&chip,(Pin){0,1},(Pin){1,13})==1);
        assert(spice_dc_analyze(&chip,argv[1],&current,led,message,sizeof(message)));
        for(g=0;g<4;++g) assert(fabs(led[g+2])<1e-8);
        /* Four inputs stay tied to 5 V through the documented 100 MOhm input resistors. */
        assert(fabs(current + 4 * 5.0 / 1e8) < 1e-8);
        assert(bench_toggle_wire(&chip,(Pin){0,1},(Pin){1,13})==2);
        assert(bench_toggle_wire(&chip,(Pin){0,0},(Pin){1,13})==1);
        assert(bench_add(&chip, PART_SOURCE, 250 * chip.part_count, 0)==6 && bench_set_value(&chip,6,2.5));
        assert(bench_toggle_wire(&chip,(Pin){6,1},(Pin){0,1})==1);
        assert(bench_toggle_wire(&chip,(Pin){0,0},(Pin){1,11})==2);
        assert(bench_toggle_wire(&chip,(Pin){6,0},(Pin){1,11})==1);
        assert(bench_toggle_wire(&chip,(Pin){5,1},(Pin){0,1})==2);
        assert(bench_toggle_wire(&chip,(Pin){5,1},(Pin){0,0})==1);
        assert(spice_dc_analyze(&chip,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(led[5])<1e-8);
        assert(bench_toggle_wire(&chip,(Pin){6,0},(Pin){1,11})==2);
        assert(bench_toggle_wire(&chip,(Pin){0,0},(Pin){1,11})==1);
        assert(spice_dc_analyze(&chip,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(led[5]+5.0/1050)<1e-6);
        assert(bench_add(&chip, PART_CLOCK, 250 * chip.part_count, 0)==7);
        assert(bench_toggle_wire(&chip,(Pin){7,1},(Pin){0,1})==1);
        assert(bench_toggle_wire(&chip,(Pin){0,1},(Pin){1,0})==2);
        assert(bench_toggle_wire(&chip,(Pin){7,0},(Pin){1,0})==1);
        assert(bench_toggle_wire(&chip,(Pin){0,1},(Pin){1,1})==2);
        assert(bench_toggle_wire(&chip,(Pin){0,0},(Pin){1,1})==1);
        assert(spice_transient_analyze(&chip,argv[1],(Pin){1,2},1e-5,.001,&trace,message,sizeof(message)));
        { int i,hi=0,lo=0; for(i=1;i<trace.count;++i) {
            if(fabs(trace.samples[i].time-.00025)<fabs(trace.samples[hi].time-.00025)) hi=i;
            if(fabs(trace.samples[i].time-.00075)<fabs(trace.samples[lo].time-.00075)) lo=i;
          }
          assert(fabs(trace.samples[hi].value)<.01);
          assert(fabs(trace.samples[lo].value-5.0*1000/1050)<.01);
        }
    }
    return 0;
}
