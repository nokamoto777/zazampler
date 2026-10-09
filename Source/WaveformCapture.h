#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

// Single audio producer / single editor consumer. Fixed storage, no locks,
// allocation or UI calls in push(). Preserve extrema rather than skipping peaks.
class WaveformCapture {
public:
    struct Frame { float lowL=0,highL=0,lowR=0,highR=0; };
    static constexpr int capacity=2048, samplesPerFrame=32;
    void push(const float* left,const float* right,int count) noexcept {
        for(int i=0;i<count;++i) {
            const float l=std::isfinite(left[i])?left[i]:0.f,r=std::isfinite(right[i])?right[i]:0.f;
            if(accumulated==0)frame={l,l,r,r};
            else {
                frame.lowL=std::min(frame.lowL,l);frame.highL=std::max(frame.highL,l);
                frame.lowR=std::min(frame.lowR,r);frame.highR=std::max(frame.highR,r);
            }
            if(++accumulated==samplesPerFrame) {
                int start1,size1,start2,size2;fifo.prepareToWrite(1,start1,size1,start2,size2);
                if(size1>0) {data[static_cast<size_t>(start1)]=frame;fifo.finishedWrite(1);}
                accumulated=0; // A slow/closed editor drops display data, never audio.
            }
        }
    }
    int read(Frame* destination,int maximum) noexcept {
        int start1,size1,start2,size2;fifo.prepareToRead(maximum,start1,size1,start2,size2);
        for(int i=0;i<size1;++i)destination[i]=data[static_cast<size_t>(start1+i)];
        for(int i=0;i<size2;++i)destination[size1+i]=data[static_cast<size_t>(start2+i)];
        fifo.finishedRead(size1+size2);return size1+size2;
    }
private:
    std::array<Frame,capacity> data {};
    juce::AbstractFifo fifo {capacity};
    Frame frame;
    int accumulated=0;
};
