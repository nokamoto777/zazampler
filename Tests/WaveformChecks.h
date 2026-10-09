#pragma once
#include "WaveformCapture.h"
namespace waveformchecks {
inline void run() {
    WaveformCapture capture,split;
    std::array<float,96> left {},right {};
    for(size_t i=0;i<left.size();++i) {left[i]=static_cast<float>(i%32)/16.f-1.f;right[i]=-left[i];}
    capture.push(left.data(),right.data(),96);
    split.push(left.data(),right.data(),17);split.push(left.data()+17,right.data()+17,79);
    std::array<WaveformCapture::Frame,WaveformCapture::capacity> frames {},other {};
    check(capture.read(frames.data(),10)==3 && split.read(other.data(),10)==3,"visualizer frame count and block split");
    for(int i=0;i<3;++i) {
        const auto& f=frames[static_cast<size_t>(i)];const auto& s=other[static_cast<size_t>(i)];
        check(f.lowL==-1.f && f.highL==0.9375f && f.lowR==-0.9375f && f.highR==1.f,"visualizer stereo extrema preserve antiphase");
        check(f.lowL==s.lowL && f.highL==s.highL && f.lowR==s.lowR && f.highR==s.highR,"visualizer split invariant");
    }
    check(capture.read(frames.data(),10)==0,"visualizer drain");
    for(int i=0;i<1000;++i)capture.push(left.data(),right.data(),96);
    check(capture.read(frames.data(),WaveformCapture::capacity)==WaveformCapture::capacity-1,"closed editor overflow is bounded");
    capture.push(left.data(),right.data(),32);check(capture.read(frames.data(),10)==1,"visualizer resumes after overflow");
}
}
