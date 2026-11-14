#include <assert.h>
#include <math.h>
#include <string.h>
#include "spice_process.h"

int main(int argc, char **argv)
{
    Workbench b={0}; char message[128]; double current=42, led[WORKBENCH_MAX_COMPONENTS]={0}, wire[WORKBENCH_MAX_WIRES]={0};
    assert(bench_add(&b,PART_SOURCE,0,0)==0);
    assert(bench_add(&b,PART_RESISTOR,0,0)==1);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==1);
    assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==1);
    assert(!spice_dc_analyze(&b,"build/missing-ngspice.exe",&current,led,message,sizeof(message)));
    assert(current==42 && strstr(message,"nao encontrado"));
    if(argc>1) {
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(current+.005)<1e-7);
        assert(fabs(led[1]-.005)<1e-7);
        assert(spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(fabs(wire[0]-.005)<1e-7 && fabs(wire[1]+.005)<1e-7);
        assert(bench_add(&b,PART_RESISTOR,0,0)==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){2,1})==1);
        assert(spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(fabs(current+.01)<1e-7 && fabs(wire[0]-.005)<1e-7 && fabs(wire[2]-.005)<1e-7);
        bench_remove(&b,2);
        assert(bench_add(&b,PART_BREADBOARD,0,0)==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){2,1},(Pin){1,0})==1);
        assert(!spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(strstr(message,"ambigua"));
        assert(fabs(wire[0]-.005)<1e-7);
        { SpiceTrace ambiguous={0};
          assert(!spice_transient_wire(&b,argv[1],0,1e-5,.005,&ambiguous,message,sizeof(message)));
          assert(strstr(message,"ambigua"));
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
        assert(bench_add(&b,PART_LED,0,0)==2);
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
        assert(bench_add(&rc,PART_SOURCE,0,0)==0);
        assert(bench_add(&rc,PART_RESISTOR,0,0)==1);
        assert(bench_add(&rc,PART_CAPACITOR,0,0)==2);
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
        assert(bench_add(&rc,PART_INDUCTOR,0,0)==2);
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
        assert(bench_add(&rc,PART_SWITCH,0,0)==3);
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
                assert(bench_add(&logic,PART_SOURCE,0,0)==0);
                assert(bench_add(&logic,type,0,0)==1);
                assert(bench_add(&logic,PART_RESISTOR,0,0)==2);
                assert(bench_add(&logic,PART_LED,0,0)==3);
                assert(bench_toggle_wire(&logic,(Pin){0,(bits&1)?0:1},(Pin){1,0})==1);
                if(type!=PART_NOT) assert(bench_toggle_wire(&logic,(Pin){0,(bits&2)?0:1},(Pin){1,1})==1);
                assert(bench_toggle_wire(&logic,(Pin){1,output},(Pin){2,0})==1);
                assert(bench_toggle_wire(&logic,(Pin){2,1},(Pin){3,0})==1);
                assert(bench_toggle_wire(&logic,(Pin){3,1},(Pin){0,1})==1);
                assert(spice_transient_analyze(&logic,argv[1],(Pin){1,output},1e-5,.001,&measured,message,sizeof(message)));
                i=measured.count-1;
                if(high) assert(measured.samples[i].value>4.9); else assert(fabs(measured.samples[i].value)<.01);
                assert(spice_dc_analyze(&logic,argv[1],&current,led,message,sizeof(message)));
                if(high) assert(led[3]>.003 && led[3]<.004); else assert(fabs(led[3])<1e-8);
            }
        }
        {
            Workbench cascade={0}; SpiceTrace measured={0};
            assert(bench_add(&cascade,PART_SOURCE,0,0)==0);
            assert(bench_add(&cascade,PART_AND,0,0)==1);
            assert(bench_add(&cascade,PART_NOT,0,0)==2);
            assert(bench_add(&cascade,PART_RESISTOR,0,0)==3);
            assert(bench_add(&cascade,PART_LED,0,0)==4);
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
    return 0;
}
