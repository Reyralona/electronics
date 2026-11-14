#include <assert.h>
#include "spice_trace.h"
int main(void)
{
    SpiceTrace trace={0}; int done;
    trace.count=4; trace.samples[0]=(SpiceSample){.001,0}; trace.samples[1]=(SpiceSample){.002,1};
    trace.samples[2]=(SpiceSample){.006,2}; trace.samples[3]=(SpiceSample){.011,3};
    assert(spice_trace_replay_index(&trace,0,0,&done)==0 && !done);
    assert(spice_trace_replay_index(&trace,0,1000,&done)==1 && !done);
    assert(spice_trace_replay_index(&trace,0,2500,&done)==2 && !done);
    assert(spice_trace_replay_index(&trace,0,5000,&done)==3 && done);
    assert(spice_trace_replay_index(&trace,1,2000,&done)==2 && !done);
    assert(spice_trace_replay_index(&trace,1,4500,&done)==3 && done);
    assert(spice_trace_replay_index(&trace,4,0,&done)==-1);
    trace.count=1; assert(spice_trace_replay_index(&trace,0,0,&done)==-1);
    return 0;
}
