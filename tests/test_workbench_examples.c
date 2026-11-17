#include <assert.h>
#include <math.h>
#include <string.h>
#include "workbench_examples.h"
#include "spice_process.h"

int main(int argc,char **argv)
{
    Workbench b={0}, old; int selected=-1;
    assert(workbench_example_create(0,&b,&selected));
    assert(b.part_count==4 && b.wire_count==3 && selected==3);
    { Pin contact; assert(bench_find_contact(&b,2,0,&contact) && contact.component==1); }
    old=b; assert(!workbench_example_create(42,&b,&selected));
    assert(memcmp(&old,&b,sizeof(b))==0 && selected==3);
    if(argc>1) {
        double current, values[WORKBENCH_MAX_COMPONENTS]; char message[128];
        assert(spice_dc_analyze(&b,argv[1],&current,values,message,sizeof(message)));
        assert(values[3]>.003 && values[3]<.004);
    }
    uint64_t previous_revision = b.revision;
    assert(workbench_example_create(1,&b,&selected));
    assert(b.revision > previous_revision);
    assert(b.part_count==5 && b.wire_count==6 && selected==2 && b.parts[2].type==PART_DFF);
    if(argc>1) {
        SpiceTrace trace={0}; char message[128]; int i,hold=0,low=0;
        assert(spice_transient_analyze(&b,argv[1],(Pin){2,2},1e-5,.0035,&trace,message,sizeof(message)));
        for(i=1;i<trace.count;++i) {
            if(fabs(trace.samples[i].time-.0015)<fabs(trace.samples[hold].time-.0015)) hold=i;
            if(fabs(trace.samples[i].time-.00225)<fabs(trace.samples[low].time-.00225)) low=i;
        }
        assert(trace.samples[hold].value > 4.7 && trace.samples[hold].value < 5.01);
        assert(trace.samples[low].value < .01);
        assert((trace.leds_on[hold]&(UINT64_C(1)<<4)) && !(trace.leds_on[low]&(UINT64_C(1)<<4)));
    }
    previous_revision = b.revision;
    assert(workbench_example_create(2, &b, &selected));
    assert(b.revision > previous_revision);
    assert(b.part_count == 6 && selected == 2 && b.parts[2].type == PART_74HC00);
    for (int pin = 0; pin < 14; ++pin) {
        Pin contact;
        assert(bench_find_contact(&b, 2, pin, &contact) && contact.component == 1);
    }
    if (argc > 1) {
        static SpiceTrace trace;
        char message[128];
        int low = 0, high = 0;
        assert(spice_transient_instrumented(&b, argv[1], 1, (Pin){2, 2}, 1e-5, .002,
                                            &trace, message, sizeof(message)));
        for (int i = 1; i < trace.count; ++i) {
            if (fabs(trace.samples[i].time - .00025) < fabs(trace.samples[low].time - .00025)) low = i;
            if (fabs(trace.samples[i].time - .00075) < fabs(trace.samples[high].time - .00075)) high = i;
        }
        assert(trace.samples[low].value < .01 && trace.samples[high].value > 4.5);
        assert(!(trace.leds_on[low] & (UINT64_C(1) << 5)));
        assert(trace.leds_on[high] & (UINT64_C(1) << 5));
        assert(trace.measures_wires);
    }
    return 0;
}
