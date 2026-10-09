#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Parameters.h"
#include "BankImport.h"
#include "FolderAccess.h"
#include "LibraryCatalog.h"
#include "WaveformCapture.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ZaZamplerProcessor final : public juce::AudioProcessor {
public:
    ZaZamplerProcessor();
    ~ZaZamplerProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ZaZampler"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return tailSeconds.load(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "SFZ"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void load(const juce::File& root, const juce::String& bookmark, const juce::String& relative);
    juce::String statusText() const;
    juce::Result openBank(const juce::File&);
    void clearBank();
    std::shared_ptr<const ZamplerBank> getBank() const;
    std::pair<juce::String,juce::String> libraryLocation() const;
    juce::String importNotes() const;
    int selectedBankSlot() const;
    bool bankUsesApproximateFx() const;
    void loadBankPatch(const juce::File& root,const juce::String& bookmark,int slot,bool approximateFx,bool applySettings=true);
    bool isLoading() const { return loading.load(); }
    bool isReady() const { return ready.load(); }
    void panic() { panicRequested.store(true); }
    LibraryCatalog libraries;
    WaveformCapture waveform;
    juce::String currentBankPath() const;
    juce::String currentInstrumentPath() const;
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> peak {0};
    std::atomic<double> tailSeconds {10.};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    struct Request {
        juce::String root,bookmark,relative;uint64_t serial=0;
        std::shared_ptr<const ZamplerPatch> patch;
        juce::File bankFile;int bankSlot=-1;bool approximate=false;
    } request;
    std::shared_ptr<const ZamplerBank> bankData;
    juce::String bankBase64,bankPath,bankNotes;
    // Destruction order: free streaming synth before closing its directory scope.
    struct Instrument { std::unique_ptr<FolderAccess> access; std::unique_ptr<Engine> engine; };
    std::unique_ptr<Instrument> instrument;
    mutable std::mutex stateMutex;
    std::mutex engineMutex;
    std::condition_variable cv,loadDone;
    bool stop = false, pending = false;
    juce::String status = "Choose a library folder, then select an SFZ instrument.";
    std::atomic<bool> loading {false}, ready {false}, panicRequested {false};
    std::thread loader;
    double rate = 44100.;
    int maxBlock = 512;
    int lastChannel = 1;
    Effects effects;
    EnvelopeGate envelopeGate;
    NoteSequence sequence;
    MonoGlide monoGlide;
    PerformanceFrame performance;
    std::vector<LfoModulation> modulationFrames;
    std::vector<float> pitchFrames;
    int lastSequenceMode=0,lastVoiceMode=0;
    bool wasPlaying=false;
    void resetPerformance();
    void loadLoop();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZaZamplerProcessor)
};
