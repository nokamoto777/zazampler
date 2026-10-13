#include "Processor.h"
#include "Editor.h"
#include "AboutPanel.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout ZaZamplerProcessor::layout() {
    return makeParameterLayout();
}
ZaZamplerProcessor::ZaZamplerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"QSamplerState",layout()), loader([this]{loadLoop();}) {log.add("Started "+zazamplerBuildInfo().replace("\n"," | "));}
ZaZamplerProcessor::~ZaZamplerProcessor() {
    { std::lock_guard<std::mutex> l(stateMutex); stop = true; }
    cv.notify_one();loadDone.notify_all();loader.join();
}
bool ZaZamplerProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && b.getMainInputChannelSet().isDisabled();
}
void ZaZamplerProcessor::prepareToPlay(double r, int b) {
    std::lock_guard<std::mutex> l(engineMutex);
    log.add("Prepare: sample rate="+juce::String(r)+", max block="+juce::String(b)+", wrapper="+juce::AudioProcessor::getWrapperTypeDescription(wrapperType));
    diagnosticPeakMs.store(0);diagnosticOverruns.store(0);diagnosticOffset.store(-1);diagnosticOnset.store(-1);
    setLatencySamples(0);diagnosticRate.store(r>0?r:44100.);
    rate = r > 0 ? r : 44100.; maxBlock = juce::jmax(1,b);
    if (instrument) instrument->engine->prepare(rate,maxBlock);
    effects.prepare(rate);sequence.prepare(rate);monoGlide.prepare(rate);performance={};
    modulationFrames.resize(static_cast<size_t>(maxBlock));pitchFrames.resize(static_cast<size_t>(maxBlock));expressionFrames.resize(static_cast<size_t>(maxBlock));
    envelopeGate.reset();
}
void ZaZamplerProcessor::releaseResources() {
    log.add("Release audio resources");
    std::lock_guard<std::mutex> l(engineMutex);
    if (instrument) instrument->engine->panic();
    effects.reset();resetPerformance();
}
void ZaZamplerProcessor::load(const juce::File& root, const juce::String& bookmark, const juce::String& relative) {
    { std::lock_guard<std::mutex> l(stateMutex);
      const auto serial=request.serial+1;request={};request.serial=serial;
      request.root=root.getFullPathName();request.bookmark=bookmark;request.relative=relative;
      bankNotes.clear();
      pending = true; loading.store(true); status = "Loading " + relative + " ..."; }
    log.add("Load requested: "+relative);
    cv.notify_one();
}
juce::Result ZaZamplerProcessor::openBank(const juce::File& file) {
    log.add("Open bank: "+file.getFileName());
    try {
        if(file.getSize()>4*1024*1024)return juce::Result::fail("Bank is larger than 4 MiB");
        juce::MemoryBlock bytes;if(!file.loadFileAsData(bytes))return juce::Result::fail("Cannot read bank file");
        auto parsed=std::make_shared<ZamplerBank>(ZamplerBank::parse(bytes.getData(),bytes.getSize()));
        parsed->name=file.getFileNameWithoutExtension().toStdString();
        auto encoded=bytes.toBase64Encoding();
        std::lock_guard<std::mutex> lock(stateMutex);
        bankData=std::move(parsed);bankBase64=encoded;bankPath=file.getFullPathName();request.bankSlot=-1;
        bankArchive=juce::ValueTree("BankArchive");patchOrigins.assign(bankData->patches.size(),file);
        status="Bank opened. Select its samples folder and a preset.";
        return juce::Result::ok();
    } catch(const std::exception& e){return juce::Result::fail(e.what());}
}
void ZaZamplerProcessor::clearBank() {
    std::lock_guard<std::mutex> lock(stateMutex);
    bankData.reset();bankBase64.clear();bankPath.clear();bankNotes.clear();patchOrigins.clear();bankArchive=juce::ValueTree("BankArchive");
    // A queued instrument load keeps its immutable patch snapshot.
}
std::shared_ptr<const ZamplerBank> ZaZamplerProcessor::getBank() const {std::lock_guard<std::mutex> lock(stateMutex);return bankData;}
std::pair<juce::String,juce::String> ZaZamplerProcessor::libraryLocation() const {std::lock_guard<std::mutex> lock(stateMutex);return {request.root,request.bookmark};}
juce::String ZaZamplerProcessor::currentBankPath() const {std::lock_guard<std::mutex> lock(stateMutex);return bankPath;}
juce::String ZaZamplerProcessor::currentInstrumentPath() const {std::lock_guard<std::mutex> lock(stateMutex);return activeRequest.root.isNotEmpty() && activeRequest.relative.isNotEmpty()?juce::File(activeRequest.root).getChildFile(activeRequest.relative).getFullPathName():juce::String();}
juce::String ZaZamplerProcessor::importNotes() const {std::lock_guard<std::mutex> lock(stateMutex);return bankNotes;}
int ZaZamplerProcessor::selectedBankSlot() const {std::lock_guard<std::mutex> lock(stateMutex);return request.bankSlot;}
bool ZaZamplerProcessor::bankUsesApproximateFx() const {std::lock_guard<std::mutex> lock(stateMutex);return request.bankSlot<0 || request.approximate;}
void ZaZamplerProcessor::loadBankPatch(const juce::File& root,const juce::String& bookmark,int slot,bool approximate,bool applySettings) {
    std::shared_ptr<const ZamplerBank> bank;juce::String path;
    {std::lock_guard<std::mutex> lock(stateMutex);bank=bankData;path=bankPath;}
    if(!bank || slot<0 || static_cast<size_t>(slot)>=bank->patches.size())return;
    log.add("Select bank slot "+juce::String(slot)+"; approximate="+juce::String(approximate?1:0));
    auto patch=std::make_shared<ZamplerPatch>(bank->patches[static_cast<size_t>(slot)]);
    auto translation=translateZamplerPatch(*patch);
    if(applySettings) {
        // This is called by the UI, before queuing disk work; do not notify a host while holding our locks.
        for(const auto& spec:parameterSpecs()) {
            if(juce::String(spec.id)=="channel")continue;
            float value=spec.initial;
            if(approximate)for(const auto& setting:translation.settings)if(setting.first==spec.id)value=setting.second;
            if(auto* parameter=parameters.getParameter(spec.id)) {
                parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(value));parameter->endChangeGesture();
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        const auto serial=request.serial+1;request={};request.serial=serial;
        request.root=root.getFullPathName();request.bookmark=bookmark;request.patch=std::move(patch);
        request.bankFile=static_cast<size_t>(slot)<patchOrigins.size()?patchOrigins[static_cast<size_t>(slot)]:juce::File(path);request.bankSlot=slot;request.approximate=approximate;
        request.relative=juce::String::fromUTF8(request.patch->sampleFile.c_str());
        bankNotes=approximate?translation.warnings.joinIntoString("\n"):
            "SFZ only: Zampler filter, FX, envelopes, modulation and sequencer settings are not applied. Manual FX remain available.";
        pending=true;loading.store(true);status="Loading bank preset: "+juce::String::fromUTF8(request.patch->name.c_str());
    }
    cv.notify_one();
}
void ZaZamplerProcessor::loadLoop() {
    for (;;) {
        Request r;
        { std::unique_lock<std::mutex> l(stateMutex); cv.wait(l,[this]{return stop || pending;});
          if (stop) return;
          r = request; pending = false; loadStarted=juce::Time::getMillisecondCounterHiRes(); }
        auto cancelled=[this,&r]{std::lock_guard<std::mutex> lock(stateMutex);return stop || r.serial!=request.serial;};
        setLoadStage(r.serial,"Opening folder access");
        std::unique_ptr<Instrument> candidate;
        juce::String result;
        try {
            candidate = std::make_unique<Instrument>();
            candidate->access = std::make_unique<FolderAccess>(r.root.toStdString(),r.bookmark.toStdString());

            juce::File root(juce::String::fromUTF8(candidate->access->path.c_str()));
            auto file = root.getChildFile(r.relative);
            if(r.patch) {
                setLoadStage(r.serial,"Resolving sample reference: "+r.relative);
                const auto catalog=libraries.snapshot();
                const auto* index=catalog->root==root.getFullPathName() && !catalog->scanning?&catalog->instruments:nullptr;
                auto resolved=resolveBankSample(*r.patch,r.bankFile,root,index,cancelled);
                if(resolved.error.isNotEmpty())throw std::runtime_error(resolved.error.toStdString());
                file=resolved.file;r.relative=file.getRelativePathFrom(root);
            }
            if(cancelled())continue;
            setLoadStage(r.serial,"Creating sampler engine");
            double sr;int bs;{std::lock_guard<std::mutex> lock(engineMutex);sr=rate;bs=maxBlock;}
            candidate->engine=std::make_unique<Engine>(sr,bs);
            setLoadStage(r.serial,"Loading sample data: "+file.getFileName());
            if(file.hasFileExtension("rex;rx2;rcy")){candidate->rex=std::make_unique<RexImport>(file);file=candidate->rex->sfz;}
            if (!file.hasFileExtension("sfz") || !file.existsAsFile() || !candidate->engine->load(file.getFullPathName().toStdString())) {
                result = "Load failed. Re-select the library folder and check SFZ/sample paths."; candidate.reset();
            } else {
                result = file.getFileName() + " | " + juce::String(candidate->engine->regions()) + " regions, "
                       + juce::String(candidate->engine->samples()) + " preloaded samples";
                if(candidate->rex)result=candidate->rex->description;
                const auto unknown = candidate->engine->unknownOpcodes();
                if (!unknown.empty()) result += " | Unsupported: " + juce::String::fromUTF8(unknown.c_str());
                if(r.patch)result=juce::String::fromUTF8(r.patch->name.c_str())+(r.approximate?" [approximate FX] | ":" [SFZ only] | ")+result;
                if (candidate->engine->samples() == 0) result += " | Check samples (or oscillator-only SFZ).";
            }
        } catch (const std::exception& e) { result = "Load failed: " + juce::String(e.what()); candidate.reset(); }
        setLoadStage(r.serial,"Installing instrument");
        {
            std::lock_guard<std::mutex> e(engineMutex);
            std::lock_guard<std::mutex> s(stateMutex);
            if (stop) return;
            if (r.serial != request.serial) continue;
            if (candidate) {
                candidate->engine->prepare(rate,maxBlock);
                request.relative=r.relative;
                request.root = juce::String::fromUTF8(candidate->access->path.c_str());
                request.bookmark = juce::String::fromUTF8(candidate->access->bookmark.c_str());
            }
            lastLoadSucceeded=candidate!=nullptr;
            if(candidate){activeRequest=request;instrument.swap(candidate);panicRequested.store(true);}
            else if(instrument)result+=" | Previous instrument retained.";
            ready.store(instrument != nullptr); loading.store(false); status = result;loadStage="Idle";
        }
        log.add("Load #"+juce::String(r.serial)+": "+result);
        loadDone.notify_all();
        if(candidate){setLoadStage(r.serial,"Releasing previous instrument");candidate.reset();setLoadStage(r.serial,"Idle");}
    }
}
juce::String ZaZamplerProcessor::statusText() const { std::lock_guard<std::mutex> l(stateMutex); return status; }
void ZaZamplerProcessor::processBlock(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) {
    const double callbackStarted=juce::Time::getMillisecondCounterHiRes();
    struct Measure {
        ZaZamplerProcessor& p;double start;int frames;
        ~Measure(){const double ms=juce::Time::getMillisecondCounterHiRes()-start;
            p.diagnosticBlock.store(frames);if(!p.isNonRealtime() && ms>p.diagnosticPeakMs.load())p.diagnosticPeakMs.store(ms);
            if(!p.isNonRealtime() && frames>0 && ms>1000.*frames/p.diagnosticRate.load())++p.diagnosticOverruns;}
    } measure{*this,callbackStarted,out.getNumSamples()};
    juce::ScopedNoDenormals noDenormals;
    out.clear(); const int frames = out.getNumSamples();
    if (frames == 0 || out.getNumChannels() < 2) { midi.clear(); return; }
    keyboard.processNextMidiBuffer(midi,0,frames,true);
    // Offline export may start immediately after state restoration. Only the
    // non-realtime path waits; the live audio callback never waits for file I/O.
    if(isNonRealtime() && loading.load()) {
        std::unique_lock<std::mutex> stateLock(stateMutex);
        loadDone.wait(stateLock,[this]{return stop || !loading.load();});
    }
    std::unique_lock<std::mutex> l(engineMutex,std::defer_lock);
    if(isNonRealtime())l.lock();else l.try_lock();
    if (!l.owns_lock() || !instrument) {
        if(!loggedUnavailable){log.audio(DiagnosticLog::unavailable);loggedUnavailable=true;}
        panicRequested.store(true); midi.clear(); peak.store(0);
        waveform.push(out.getReadPointer(0),out.getReadPointer(1),frames);return;
    }
    loggedUnavailable=false;
    auto& engine = *instrument->engine;
    if (panicRequested.exchange(false)) { log.audio(DiagnosticLog::panic); engine.panic(); effects.reset();resetPerformance(); }
    engine.offline(isNonRealtime());
    float bpm = 120.f;bool playing=false,hasPosition=false;double ppq=0,barStart=0,barBeats=4;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {if (auto tempo = pos->getBpm()) bpm = static_cast<float>(*tempo);playing=pos->getIsPlaying();
        if(auto beat=pos->getPpqPosition()){ppq=*beat;hasPosition=std::isfinite(ppq);}
        if(auto ts=pos->getTimeSignature())if(ts->numerator>0 && ts->denominator>0)barBeats=ts->numerator*4.0/ts->denominator;
        if(auto bar=pos->getPpqPositionOfLastBarStart())barStart=*bar;
        else barStart=std::floor(ppq/barBeats)*barBeats;
    }
    bpm=std::isfinite(bpm)?juce::jlimit(20.f,400.f,bpm):120.f;
    const bool seek=playing && hasPosition && (!hadPosition || !wasPlaying || std::abs(ppq-expectedPpq)>std::max(1e-6,2*bpm/(60*rate)));
    hadPosition=playing && hasPosition;expectedPpq=ppq+frames*bpm/(60*rate);
    if(playing!=wasPlaying)log.audio(DiagnosticLog::transport,playing?1:0);
    if(seek)log.audio(DiagnosticLog::seek,static_cast<int>(juce::jlimit(-2000000.,2000000.,ppq)*1000));
    if(wasPlaying && !playing){engine.panic();effects.reset();resetPerformance();}wasPlaying=playing;
    const int channel = static_cast<int>(parameters.getRawParameterValue("channel")->load());
    if (channel != lastChannel) {log.audio(DiagnosticLog::channel,channel); engine.panic(); effects.reset();resetPerformance();lastChannel = channel; }
    auto fx=readFxSettings(parameters);const auto lfoSettings=readLfoSettings(parameters);
    const auto envelopeSettings=readEnvelopeSettings(parameters);
    const auto matrixSettings=readMatrixSettings(parameters);const auto seq=readSequenceSettings(parameters);
    const int voiceMode=static_cast<int>(parameters.getRawParameterValue("voiceMode")->load());
    const float glide=parameters.getRawParameterValue("glide")->load();
    const int scope=static_cast<int>(parameters.getRawParameterValue("modScope")->load());
    if(seq.mode!=lastSequenceMode || voiceMode!=lastVoiceMode || scope!=lastModScope){log.audio(DiagnosticLog::modes,seq.mode,voiceMode+2*scope);engine.panic();effects.reset();resetPerformance();lastSequenceMode=seq.mode;lastVoiceMode=voiceMode;lastModScope=scope;}
    VoiceModSettings voiceSettings;voiceSettings.lfos=lfoSettings;voiceSettings.envelopes=envelopeSettings;
    voiceSettings.matrix=matrixSettings;voiceSettings.filterMode=fx.filterMode;voiceSettings.cutoff=fx.cutoff;
    voiceSettings.resonance=fx.resonance;voiceSettings.keytrack=fx.keytrack;voiceSettings.bpm=bpm;
    if(scope)fx.filterMode=0;
    if(seq.hostSync && playing && hasPosition)sequence.synchronize(seq,bpm,ppq,barStart,seek);
    else sequence.freeClock();
    int noteOns=0,noteOffs=0;
    auto event = midi.cbegin();
    const int uiPitch=pendingPitch.exchange(-1),uiMod=pendingMod.exchange(-1);
    int onsetOffset=-1;
    // Audio-thread delivery; the GUI never touches sfizz or the host MIDI buffer.
    const bool replay=replayControllers;replayControllers=false;
    // Hosts may submit larger-than-advertised buffers; split without allocation.
    for (int start=0; start<frames; start+=maxBlock) {
        const int n = juce::jmin(maxBlock,frames-start);
        engine.tempo(bpm);
        if(instrument->rex){const unsigned char cc[]={0xb0,20,static_cast<unsigned char>(parameters.getRawParameterValue("rexPage")->load())};engine.midi(cc,3,0);}
        for(int i=0;i<n;++i) {
            EnvelopeEvent gateEvent;bool triggered=false;
            auto emit=[&](const unsigned char* data,int size) {
                if(size>=3){if((data[0]&0xf0)==0x90 && data[2])++noteOns;else if((data[0]&0xf0)==0x80 || ((data[0]&0xf0)==0x90 && !data[2]))++noteOffs;}
                engine.midi(data,size,i);performance.midi(data,size);
                gateEvent.merge(envelopeGate.midi(data,size));
                if(size>=3 && (data[0]&0xf0)==0x90 && data[2])triggered=true;
            };
            auto generated=[&](const unsigned char* data,int size){monoGlide.midi(data,size,voiceMode!=0,glide,emit);};
            if(start==0 && i==0) {
                if(uiPitch>=0 || replay){const int v=uiPitch>=0?uiPitch:wheelPitch.load();const unsigned char d[]={0xe0,static_cast<unsigned char>(v&127),static_cast<unsigned char>(v>>7)};sequence.midi(d,3,seq,generated);}
                if(uiMod>=0 || replay){const unsigned char d[]={0xb0,1,static_cast<unsigned char>(uiMod>=0?uiMod:wheelMod.load())};sequence.midi(d,3,seq,generated);}
            }
            while(event!=midi.cend() && (*event).samplePosition<=start+i) {
                const auto m=*event;
                if(m.numBytes>0 && (channel==0 || ((m.data[0]&15)+1)==channel)) {
                    if(m.numBytes>=3) {
                        const int type=m.data[0]&0xf0;
                        if(type==0xe0)wheelPitch.store((m.data[1]&127)|((m.data[2]&127)<<7));
                        if(type==0xb0 && m.data[1]==1)wheelMod.store(m.data[2]&127);
                        if(type==0xb0 && m.data[1]==121){wheelPitch.store(8192);wheelMod.store(0);}
                        if(type==0x90 && m.data[2]){diagnosticOffset.store(m.samplePosition);if(!measuringOnset && onsetOffset<0 && peak.load()<1e-5f){onsetOffset=start+i;diagnosticOnset.store(-1);}}
                    }
                    sequence.midi(m.data,m.numBytes,seq,generated);
                }
                ++event;
            }
            if(seq.hostSync && playing && hasPosition) {
                const double position=ppq+(start+i)*bpm/(60*rate);
                if(position>=barStart+barBeats-0.5*bpm/(60*rate)) {
                    barStart+=barBeats;sequence.synchronize(seq,bpm,position,barStart,false);
                }
            }
            sequence.tick(seq,bpm,generated);
            modulationFrames[static_cast<size_t>(i)]=scope?LfoModulation{}:effects.modulationTick(fx,bpm,lfoSettings,triggered,envelopeSettings,gateEvent,matrixSettings,performance);
            expressionFrames[static_cast<size_t>(i)]=performance;
            pitchFrames[static_cast<size_t>(i)]=monoGlide.tick(voiceMode!=0,true)+modulationFrames[static_cast<size_t>(i)].pitchCents;
        }
        voiceSettings.expression=expressionFrames.data();voiceSettings.frames=n;
        engine.render(out.getWritePointer(0,start),out.getWritePointer(1,start),n,pitchFrames.data(),voiceMode!=0,scope?&voiceSettings:nullptr);
        effects.process(out.getWritePointer(0,start),out.getWritePointer(1,start),n,fx,bpm,{},nullptr,{},nullptr,modulationFrames.data());
    }
    if(onsetOffset>=0){measuringOnset=true;onsetSamples=-onsetOffset;}
    if(measuringOnset) {
        for(int i=juce::jmax(0,onsetOffset);i<frames;++i)
            if(std::max(std::abs(out.getSample(0,i)),std::abs(out.getSample(1,i)))>1e-5f){diagnosticOnset.store(onsetSamples+i);measuringOnset=false;break;}
        if(measuringOnset){onsetSamples+=frames;if(onsetSamples>rate*5)measuringOnset=false;}
    }
    if(noteOns || noteOffs)log.audio(DiagnosticLog::midi,noteOns,noteOffs);
    waveform.push(out.getReadPointer(0),out.getReadPointer(1),frames);
    midi.clear();
    const double repeats=fx.delayFeedback>0 ? 1.+std::ceil(std::log(0.0001)/std::log(juce::jlimit(0.0001f,0.95f,fx.delayFeedback))) : 1.;
    const double delayTail=fx.delayMix>0 ? Effects::delayMilliseconds(fx,bpm)*0.001*repeats : 0.;
    tailSeconds.store(juce::jmax(10.,delayTail+(fx.reverb>0 ? 30. : 0.)));
    peak.store(juce::jmax(out.getMagnitude(0,0,frames),out.getMagnitude(1,0,frames)));
}
void ZaZamplerProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = parameters.copyState();
    const auto catalog=libraries.snapshot();
    tree.setProperty("libraryRoot",catalog->root,nullptr);tree.setProperty("libraryBookmark",catalog->bookmark,nullptr);
    { std::lock_guard<std::mutex> l(stateMutex);
      const bool fallback=!lastLoadSucceeded && ready.load();
      const auto& saved=fallback?activeRequest:request;
      tree.setProperty("root",saved.root,nullptr); tree.setProperty("bookmark",saved.bookmark,nullptr);
      tree.setProperty("sfz",saved.relative,nullptr); tree.setProperty("schema",10,nullptr);
      tree.setProperty("bankData",bankBase64,nullptr);tree.setProperty("bankPath",bankPath,nullptr);
      tree.removeChild(tree.getChildWithName("BankArchive"),nullptr);tree.addChild(bankArchive.createCopy(),-1,nullptr);
      tree.setProperty("bankSlot",fallback?-1:request.bankSlot,nullptr);tree.setProperty("bankApproximate",request.approximate,nullptr); }
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml,dest);
}
void ZaZamplerProcessor::setStateInformation(const void* data, int size) {
    log.add("Restore DAW state: "+juce::String(size)+" bytes");
    auto xml = getXmlFromBinary(data,size);
    if (!xml || !xml->hasTagName("QSamplerState")) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    // Missing parameters use defaults, not values left over from another preset.
    const bool legacyState=static_cast<int>(tree.getProperty("schema",1))<2;
    for(const auto& spec:parameterSpecs()) {
        auto child=tree.getChildWithProperty("id",spec.id);
        if(!child.isValid()) {
            child=juce::ValueTree("PARAM");child.setProperty("id",spec.id,nullptr);
            const float fallback=juce::String(spec.id)=="seqRhythm" && static_cast<int>(tree.getProperty("schema",1))<8?0.f:spec.initial;
            child.setProperty("value",legacyState && juce::String(spec.id)=="filterMode" ? 7.f : fallback,nullptr);
            tree.addChild(child,-1,nullptr);
        }
    }
    libraries.restore(tree["libraryRoot"].toString(),tree["libraryBookmark"].toString());
    parameters.replaceState(tree);
    clearBank();
    const auto encoded=tree["bankData"].toString();
    if(encoded.isNotEmpty() && encoded.length()<6*1024*1024) {
        try {
            juce::MemoryBlock bankBytes;
            if(!bankBytes.fromBase64Encoding(encoded))throw std::runtime_error("Invalid saved bank encoding");
            auto parsed=std::make_shared<ZamplerBank>(ZamplerBank::parse(bankBytes.getData(),bankBytes.getSize()));
            const auto path=tree["bankPath"].toString();
            parsed->name=path.isNotEmpty()?juce::File(path).getFileNameWithoutExtension().toStdString():"Saved bank";
            std::lock_guard<std::mutex> lock(stateMutex);bankData=std::move(parsed);bankBase64=encoded;bankPath=path;
        } catch(const std::exception&) { /* Keep saved SFZ/FX state usable even if bank browser data is damaged. */ }
    }
    if(auto archive=tree.getChildWithName("BankArchive");archive.isValid() && archive.getNumChildren()>0) {
        try{restoreBankArchive(archive,tree["bankPath"].toString());}catch(const std::exception& e){log.add("Bank archive restore failed: "+juce::String(e.what()));}
    }
    auto root=tree["root"].toString(), relative=tree["sfz"].toString();
    const int bankSlot=static_cast<int>(tree.getProperty("bankSlot",-1));
    if(root.isNotEmpty() && getBank() && bankSlot>=0 && static_cast<size_t>(bankSlot)<getBank()->patches.size()) {
        loadBankPatch(juce::File(root),tree["bookmark"].toString(),bankSlot,static_cast<bool>(tree["bankApproximate"]),false);
        return;
    }
    if (root.isNotEmpty() && relative.isNotEmpty()) load(juce::File(root),tree["bookmark"].toString(),relative);
    else {
        // Restoring an empty initial preset must stop an already-loaded instrument.
        std::lock_guard<std::mutex> e(engineMutex);
        std::lock_guard<std::mutex> s(stateMutex);
        const auto serial=request.serial+1;request={};request.serial=serial;
        pending=false; activeRequest={};lastLoadSucceeded=false;ready.store(false); loading.store(false); instrument.reset(); effects.reset();resetPerformance();loadDone.notify_all();
        status="Choose a library folder, then select an SFZ instrument.";
    }
}
juce::AudioProcessorEditor* ZaZamplerProcessor::createEditor() { return new ZaZamplerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZaZamplerProcessor(); }

void ZaZamplerProcessor::resetPerformance() {
    envelopeGate.reset();sequence.reset();monoGlide.reset();performance={};replayControllers=true;measuringOnset=false;
}

juce::String ZaZamplerProcessor::debugReport() {
    juce::String stage;{std::lock_guard<std::mutex> lock(stateMutex);stage=loadStage+"; elapsed="+juce::String((juce::Time::getMillisecondCounterHiRes()-loadStarted)/1000.,2)+" s; request="+juce::String(request.serial)+"; pending="+juce::String(pending?1:0);}
    const auto catalog=libraries.snapshot();
    juce::String result=zazamplerBuildInfo()+"\nStatus: "+statusText()+"\nLoader: "+stage+"\nLibrary: "+catalog->root+"; cached="+juce::String(catalog->cached?1:0)+"; scanning="+juce::String(catalog->scanning?1:0)+"; instruments="+juce::String(catalog->instruments.size())+"\n\n"+log.snapshot()+"\nCurrent parameters:\n";
    for(const auto& spec:parameterSpecs())result+=juce::String(spec.id)+"="+juce::String(parameters.getRawParameterValue(spec.id)->load())+"\n";
    result+="\nTiming diagnostics (inside plugin only):\nReported latency="+juce::String(getLatencySamples())+" samples; actual block="+juce::String(diagnosticBlock.load())+" samples; sample rate="+juce::String(diagnosticRate.load())+" Hz\nLast note offset="+juce::String(diagnosticOffset.load())+" samples; quiet-note onset="+juce::String(diagnosticOnset.load())+" samples (-1 = no measurement)\nPeak realtime callback="+juce::String(diagnosticPeakMs.load(),3)+" ms; realtime deadline overruns="+juce::String(diagnosticOverruns.load())+"\nOnset includes patch attack, sample silence, sequencer and effects; excludes host/device latency.\n";
    result+="\nImport details:\n"+importNotes();return result;
}

void ZaZamplerProcessor::setLoadStage(uint64_t serial,const juce::String& stage) {
    {std::lock_guard<std::mutex> lock(stateMutex);loadStage=stage;}
    log.add("Loader #"+juce::String(serial)+": "+stage);
}
void ZaZamplerProcessor::restoreBankArchive(const juce::ValueTree& archive,const juce::String& path) {
    auto merged=std::make_shared<ZamplerBank>();std::vector<juce::File> origins;
    merged->name=juce::File(path).getFileNameWithoutExtension().toStdString();
    size_t total=0;if(archive.getNumChildren()>4096)throw std::runtime_error("Too many bank archive files");
    for(const auto& child:archive) {
        juce::MemoryBlock bytes;const auto data=child["data"].toString();
        if(data.length()>6*1024*1024 || !bytes.fromBase64Encoding(data))throw std::runtime_error("Invalid bank archive");
        total+=bytes.getSize();if(total>32*1024*1024)throw std::runtime_error("Bank archive exceeds 32 MiB");
        const auto parsed=ZamplerBank::parse(bytes.getData(),bytes.getSize());
        const juce::File source(child["path"].toString());
        for(auto patch:parsed.patches) {
            if(!patch.hasSample())continue;
            bool duplicate=false;
            for(const auto& previous:merged->patches)
                if(previous.name==patch.name && previous.sampleFile==patch.sampleFile && previous.parameters==patch.parameters){duplicate=true;break;}
            if(duplicate)continue;
            if(merged->patches.size()>=4096)throw std::runtime_error("Library exceeds 4096 presets");
            patch.slot=static_cast<int>(merged->patches.size());merged->patches.push_back(std::move(patch));origins.push_back(source);
        }
    }
    std::lock_guard<std::mutex> lock(stateMutex);
    bankData=merged;bankPath=path;bankBase64.clear();bankArchive=archive.createCopy();patchOrigins=std::move(origins);request.bankSlot=-1;
}
juce::Result ZaZamplerProcessor::openLibrary(const LibraryCatalog::Entry& entry) {
    if(entry.patches.isEmpty())return openBank(entry.file);
    try {
        juce::ValueTree archive("BankArchive");juce::Array<juce::File> sources;
        if(entry.file.hasFileExtension("fxb"))sources.add(entry.file);
        sources.addArray(entry.patches);size_t total=0;
        for(const auto& f:sources) {
            if(f.getSize()>4*1024*1024)throw std::runtime_error("Bank exceeds 4 MiB");
            juce::MemoryBlock bytes;if(!f.loadFileAsData(bytes))throw std::runtime_error("Cannot read "+f.getFileName().toStdString()+". RESCAN the library.");
            total+=bytes.getSize();if(total>32*1024*1024)throw std::runtime_error("Library metadata exceeds 32 MiB");
            juce::ValueTree item("File");item.setProperty("path",f.getFullPathName(),nullptr);item.setProperty("data",bytes.toBase64Encoding(),nullptr);archive.addChild(item,-1,nullptr);
        }
        restoreBankArchive(archive,entry.file.getFullPathName());
        log.add("Opened library: "+entry.name+"; presets="+juce::String(static_cast<int>(getBank()->patches.size())));
        return juce::Result::ok();
    }catch(const std::exception& e){log.add("Library open failed: "+juce::String(e.what()));return juce::Result::fail(e.what());}
}
