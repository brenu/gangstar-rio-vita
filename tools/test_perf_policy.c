#include "gangstar_perf_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    GangstarPerfWindow window={0}, report={0};
    assert(!gangstar_perf_add(&window,50,100,&report));
    for (int i=0;i<300;++i) {
        uint64_t begin=window.last_end+10000, end=begin+6667;
        int ready=gangstar_perf_add(&window,begin,end,&report);
        assert(ready==(i==299));
    }
    assert(report.frames==300 && gangstar_perf_fps10(&report)==600);
    assert(report.work/report.frames==10000 && report.swap/report.frames==6667);
    assert(report.worst==16667 && report.last_end-report.start==5000100);
    assert(window.frames==0 && window.start==window.last_end);
    assert(!gangstar_perf_add(&window,window.last_end-1,window.last_end,&report));
    assert(window.last_end==0);
    assert(!gangstar_perf_add(&window,20,10,&report));
    assert(gangstar_perf_fps10(&window)==0);
    window=(GangstarPerfWindow){0};
    gangstar_perf_add(&window,1,100,&report);
    assert(gangstar_perf_add(&window,1000100,6000100,&report));
    assert(report.frames==1 && report.worst==6000000);
    puts("Frame timing: warmup, 60 FPS window, work/swap split, reset, invalid clocks and long stalls passed");
}
