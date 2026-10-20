#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Parameters.h"
#include "BankImport.h"
#include "FolderAccess.h"
#include "LibraryCatalog.h"
#include "WaveformCapture.h"
#include "ModulationMeter.h"
#include "DiagnosticLog.h"
#include "RexImport.h"
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
    bool producesMidi() const override { return true; }
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
    juce::Result openLibrary(const LibraryCatalog::Entry&);
    void clearBank();
    std::shared_ptr<const ZamplerBank> getBank() const;
    std::pair<juce::String,juce::String> libraryLocation() const;
    juce::String importNotes() const;
    int selectedBankSlot() const;
    bool bankUsesApproximateFx() const;
    void loadBankPatch(const juce::File& root,const juce::String& bookmark,int slot,bool approximateFx,bool applySettings=true);
    bool isLoading() const { return loading.load(); }
    bool isReady() const { return ready.load(); }
    void panic() { panicRequested.store(true);fadePanic.store(true); }
    void setUiPitch(int value){value=juce::jlimit(0,16383,value);wheelPitch.store(value);pendingPitch.store(value);}
    void setUiMod(int value){value=juce::jlimit(0,127,value);wheelMod.store(value);pendingMod.store(value);}
    int pitchWheelValue() const{return wheelPitch.load();}
    int modWheelValue() const{return wheelMod.load();}
    DiagnosticLog log;
    juce::String debugReport();
    LibraryCatalog libraries;
    WaveformCapture waveform;
    ModulationMeter modulationMeter;
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
    } request,activeRequest;
    bool lastLoadSucceeded=false;
    std::shared_ptr<const ZamplerBank> bankData;
    juce::String bankBase64,bankPath,bankNotes;
    juce::ValueTree bankArchive {"BankArchive"};
    std::vector<juce::File> patchOrigins;
    juce::String loadStage="Idle";
    double loadStarted=0;
    void setLoadStage(uint64_t,const juce::String&);
    void restoreBankArchive(const juce::ValueTree&,const juce::String&);
    // Destruction order: free streaming synth before closing its directory scope.
    struct Instrument { std::unique_ptr<RexImport> rex; std::unique_ptr<FolderAccess> access; std::unique_ptr<Engine> engine; };
    std::unique_ptr<Instrument> instrument;
    mutable std::mutex stateMutex;
    std::mutex engineMutex;
    std::condition_variable cv,loadDone;
    bool stop = false, pending = false;
    juce::String status = "Choose a library folder, then select an SFZ instrument.";
    std::atomic<bool> loading {false}, ready {false}, panicRequested {false};
    std::thread loader;
    std::atomic<int> wheelPitch{8192},wheelMod{0},pendingPitch{-1},pendingMod{-1};
    bool replayControllers=true;
    std::atomic<int> diagnosticBlock{0},diagnosticOffset{-1},diagnosticOnset{-1},diagnosticOverruns{0};
    std::atomic<double> diagnosticRate{44100},diagnosticPeakMs{0};
    bool measuringOnset=false;int onsetSamples=0;
    double rate = 44100.;
    int maxBlock = 512;
    int lastChannel = 1;
    Effects effects;
    OutputLimiter outputLimiter;
    juce::MidiBuffer generatedMidi;
    void flushExternalMidi(juce::MidiBuffer&);
    bool midiOutputWasOn=false;
    std::array<bool,2048> outputNotes{};
    std::atomic<bool> fadePanic{false};
    std::unique_ptr<Instrument> fadingInstrument;
    juce::AudioBuffer<float> fadeBuffer;
    VoiceModSettings lastRenderSettings,fadeSettings;
    bool lastRenderPerVoice=false,fadePerVoice=false;
    int fadeRemaining=0,fadeLength=1,lastRexPlayback=0,rexLastNote=-1;

    EnvelopeGate envelopeGate;
    NoteSequence sequence;
    MonoGlide monoGlide;
    PerformanceFrame performance;
    std::vector<LfoModulation> modulationFrames;
    std::vector<float> pitchFrames;
    std::vector<PerformanceFrame> expressionFrames;
    int lastModScope=0;
    double expectedPpq=0;bool hadPosition=false;
    int lastSequenceMode=0,lastVoiceMode=0;
    bool wasPlaying=false,loggedUnavailable=false;
    void resetPerformance();
    void loadLoop();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZaZamplerProcessor)
};
