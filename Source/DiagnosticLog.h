#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <deque>
#include <juce_events/juce_events.h>
#include <mutex>

class DiagnosticLog : private juce::Timer {
public:
    DiagnosticLog(){startTimer(250);}
    ~DiagnosticLog() override {stopTimer();}
    enum Code {panic=1,transport,seek,channel,modes,midi,unavailable};
    void add(const juce::String& message){std::lock_guard<std::mutex> lock(mutex);append({elapsed(),message,serial.fetch_add(1)});}
    // Single producer: audio thread. No strings, allocation or locks here.
    void audio(Code code,int a=0,int b=0) noexcept {
        int first,count,second,rest;fifo.prepareToWrite(1,first,count,second,rest);
        if(count){events[static_cast<size_t>(first)]={elapsed(),code,a,b,serial.fetch_add(1)};fifo.finishedWrite(1);}else ++dropped;
    }
    juce::String snapshot() {
        std::lock_guard<std::mutex> lock(mutex);
        drainLocked();
        juce::String text="Session log (newest first); retained "+juce::String(static_cast<int>(entries.size()))+" / 4096; older entries removed="+juce::String(static_cast<int64_t>(removed))+"; realtime queue drops="+juce::String(dropped.load())+"\n";
        auto sorted=entries;std::stable_sort(sorted.begin(),sorted.end(),[](const Entry& a,const Entry& b){return a.time==b.time?a.id>b.id:a.time>b.time;});
        for(const auto& e:sorted)text+="[+"+juce::String(e.time/1000.,3)+" s] "+e.message+"\n";
        return text;
    }
private:
    struct Entry {double time;juce::String message;uint64_t id;};
    struct Event {double time=0;Code code=panic;int a=0,b=0;uint64_t id=0;};
    void timerCallback() override {std::lock_guard<std::mutex> lock(mutex);drainLocked();}
    void drainLocked() {
        int first,count,second,rest;fifo.prepareToRead(fifo.getNumReady(),first,count,second,rest);
        auto consume=[&](int offset,int size){for(int i=0;i<size;++i){const auto& e=events[static_cast<size_t>(offset+i)];juce::String message;
            switch(e.code){case panic:message="Panic / voices reset";break;
                case transport:message=e.a?"DAW playing":"DAW stopped";break;
                case seek:message="DAW position resync, PPQ="+juce::String(e.a/1000.,3);break;
                case channel:message="MIDI channel="+juce::String(e.a);break;
                case modes:message="Sequence mode="+juce::String(e.a)+", voice/scope="+juce::String(e.b);break;
                case midi:message="MIDI block: note-ons="+juce::String(e.a)+", note-offs="+juce::String(e.b);break;
                case unavailable:message="Audio skipped: no instrument available or engine busy";break;}
            append({e.time,message,e.id});}};
        consume(first,count);consume(second,rest);fifo.finishedRead(count+rest);
    }
    double started=juce::Time::getMillisecondCounterHiRes();
    double elapsed() const noexcept{return juce::Time::getMillisecondCounterHiRes()-started;}
    void append(Entry entry){if(entries.size()==4096){entries.pop_front();++removed;}entries.push_back(std::move(entry));}
    std::mutex mutex;std::deque<Entry> entries;uint64_t removed=0;
    juce::AbstractFifo fifo{2048};std::array<Event,2048> events{};std::atomic<int> dropped{0};std::atomic<uint64_t> serial{0};
};
