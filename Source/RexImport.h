#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <velociloops.h>
#include <stdexcept>

// Loader-thread only. The cache is owned by the instrument and outlives sfizz's
// streaming engine; source libraries are never modified.
class RexImport {
public:
    juce::File directory,sfz;
    juce::String description;
    explicit RexImport(const juce::File& source) {
        if(source.getSize()<=0 || source.getSize()>64*1024*1024)throw std::runtime_error("REX input must be between 1 byte and 64 MiB");
        VLError error=VL_OK;auto* raw=vl_open(source.getFullPathName().toRawUTF8(),&error);
        if(!raw)throw std::runtime_error(std::string("REX decode: ")+vl_error_string(error));
        std::unique_ptr<VLFile_s,decltype(&vl_close)> file(raw,vl_close);
        VLFileInfo info{};check(vl_get_info(raw,&info));
        if(info.slice_count<=0 || info.slice_count>4096 || info.sample_rate<8000 || info.sample_rate>192000 || info.channels<1 || info.channels>2)
            throw std::runtime_error("Unsupported REX slice count or audio format");
        directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ZaZampler-REX-"+juce::Uuid().toString());
        if(directory.createDirectory().failed())throw std::runtime_error("Cannot create REX decode cache");
        try {
            juce::String instrument="// REX slices: MIDI 0..127; CC20 selects groups of 128.\n<control> set_cc20=0\n<global> ampeg_attack=0 ampeg_release=0.01 loop_mode=one_shot pitch_keytrack=0\n";
            int64_t total=0;juce::WavAudioFormat wav;
            for(int slice=0;slice<info.slice_count;++slice) {
                const int frames=vl_get_slice_frame_count(raw,slice);
                if(frames<=0)continue;
                total+=static_cast<int64_t>(frames)*info.channels;
                if(frames>10000000 || total>64000000)throw std::runtime_error("REX decoded audio exceeds 256 MiB limit");
                juce::AudioBuffer<float> audio(info.channels,frames);int written=0;
                check(vl_decode_slice(raw,slice,audio.getWritePointer(0),info.channels==2?audio.getWritePointer(1):nullptr,0,frames,&written));
                if(written!=frames)throw std::runtime_error("REX slice decoding was incomplete");
                const auto name="slice-"+juce::String(slice)+".wav";
                auto output=directory.getChildFile(name).createOutputStream();
                if(!output)throw std::runtime_error("Cannot write REX sample cache");
                std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(output.get(),info.sample_rate,static_cast<unsigned>(info.channels),24,{},0));
                if(!writer)throw std::runtime_error("Cannot encode REX sample cache");
                output.release();if(!writer->writeFromAudioSampleBuffer(audio,0,frames))throw std::runtime_error("Cannot finish REX sample cache");
                const int key=slice%128,page=slice/128;
                instrument+="<region> sample="+name+" key="+juce::String(key)+" locc20="+juce::String(page)+" hicc20="+juce::String(page)+"\n";
            }
            sfz=directory.getChildFile("slices.sfz");if(!sfz.replaceWithText(instrument))throw std::runtime_error("Cannot write REX instrument cache");
            description="REX | "+juce::String(info.slice_count)+" slices | "+juce::String(info.tempo/1000.,2)+" BPM | MIDI 0-127 | Slice page / CC20 selects next 128";
        } catch(...) {directory.deleteRecursively();throw;}
    }
    ~RexImport(){if(directory!=juce::File())directory.deleteRecursively();}
private:
    static void check(VLError error){if(error!=VL_OK)throw std::runtime_error(std::string("REX: ")+vl_error_string(error));}
};
