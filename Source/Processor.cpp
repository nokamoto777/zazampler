#include "Processor.h"
#include "Editor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout ZaZamplerProcessor::layout() {
    return makeParameterLayout();
}
ZaZamplerProcessor::ZaZamplerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"QSamplerState",layout()), loader([this]{loadLoop();}) {}
ZaZamplerProcessor::~ZaZamplerProcessor() {
    { std::lock_guard<std::mutex> l(stateMutex); stop = true; }
    cv.notify_one();loadDone.notify_all();loader.join();
}
bool ZaZamplerProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && b.getMainInputChannelSet().isDisabled();
}
void ZaZamplerProcessor::prepareToPlay(double r, int b) {
    std::lock_guard<std::mutex> l(engineMutex);
    rate = r > 0 ? r : 44100.; maxBlock = juce::jmax(1,b);
    if (instrument) instrument->engine->prepare(rate,maxBlock);
    effects.prepare(rate);sequence.prepare(rate);monoGlide.prepare(rate);performance={};
    modulationFrames.resize(static_cast<size_t>(maxBlock));pitchFrames.resize(static_cast<size_t>(maxBlock));
    envelopeGate.reset();
}
void ZaZamplerProcessor::releaseResources() {
    std::lock_guard<std::mutex> l(engineMutex);
    if (instrument) instrument->engine->panic();
    effects.reset();resetPerformance();
}
void ZaZamplerProcessor::load(const juce::File& root, const juce::String& bookmark, const juce::String& relative) {
    { std::lock_guard<std::mutex> l(stateMutex);
      const auto serial=request.serial+1;request={};request.serial=serial;
      request.root=root.getFullPathName();request.bookmark=bookmark;request.relative=relative;
      bankNotes.clear();
      pending = true; loading.store(true); ready.store(false); status = "Loading " + relative + " ..."; }
    cv.notify_one();
}
juce::Result ZaZamplerProcessor::openBank(const juce::File& file) {
    try {
        if(file.getSize()>4*1024*1024)return juce::Result::fail("Bank is larger than 4 MiB");
        juce::MemoryBlock bytes;if(!file.loadFileAsData(bytes))return juce::Result::fail("Cannot read bank file");
        auto parsed=std::make_shared<ZamplerBank>(ZamplerBank::parse(bytes.getData(),bytes.getSize()));
        parsed->name=file.getFileNameWithoutExtension().toStdString();
        auto encoded=bytes.toBase64Encoding();
        std::lock_guard<std::mutex> lock(stateMutex);
        bankData=std::move(parsed);bankBase64=encoded;bankPath=file.getFullPathName();request.bankSlot=-1;
        status="Bank opened. Select its samples folder and a preset.";
        return juce::Result::ok();
    } catch(const std::exception& e){return juce::Result::fail(e.what());}
}
void ZaZamplerProcessor::clearBank() {
    std::lock_guard<std::mutex> lock(stateMutex);
    bankData.reset();bankBase64.clear();bankPath.clear();bankNotes.clear();
    // A queued instrument load keeps its immutable patch snapshot.
}
std::shared_ptr<const ZamplerBank> ZaZamplerProcessor::getBank() const {std::lock_guard<std::mutex> lock(stateMutex);return bankData;}
std::pair<juce::String,juce::String> ZaZamplerProcessor::libraryLocation() const {std::lock_guard<std::mutex> lock(stateMutex);return {request.root,request.bookmark};}
juce::String ZaZamplerProcessor::currentBankPath() const {std::lock_guard<std::mutex> lock(stateMutex);return bankPath;}
juce::String ZaZamplerProcessor::currentInstrumentPath() const {std::lock_guard<std::mutex> lock(stateMutex);return request.root.isNotEmpty() && request.relative.isNotEmpty()?juce::File(request.root).getChildFile(request.relative).getFullPathName():juce::String();}
juce::String ZaZamplerProcessor::importNotes() const {std::lock_guard<std::mutex> lock(stateMutex);return bankNotes;}
int ZaZamplerProcessor::selectedBankSlot() const {std::lock_guard<std::mutex> lock(stateMutex);return request.bankSlot;}
bool ZaZamplerProcessor::bankUsesApproximateFx() const {std::lock_guard<std::mutex> lock(stateMutex);return request.bankSlot<0 || request.approximate;}
void ZaZamplerProcessor::loadBankPatch(const juce::File& root,const juce::String& bookmark,int slot,bool approximate,bool applySettings) {
    std::shared_ptr<const ZamplerBank> bank;juce::String path;
    {std::lock_guard<std::mutex> lock(stateMutex);bank=bankData;path=bankPath;}
    if(!bank || slot<0 || static_cast<size_t>(slot)>=bank->patches.size())return;
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
        request.bankFile=juce::File(path);request.bankSlot=slot;request.approximate=approximate;
        request.relative=juce::String::fromUTF8(request.patch->sampleFile.c_str());
        bankNotes=approximate?translation.warnings.joinIntoString("\n"):
            "SFZ only: Zampler filter, FX, envelopes, modulation and sequencer settings are not applied. Manual FX remain available.";
        pending=true;loading.store(true);ready.store(false);status="Loading bank preset: "+juce::String::fromUTF8(request.patch->name.c_str());
    }
    cv.notify_one();
}
void ZaZamplerProcessor::loadLoop() {
    for (;;) {
        Request r;
        { std::unique_lock<std::mutex> l(stateMutex); cv.wait(l,[this]{return stop || pending;});
          if (stop) return;
          r = request; pending = false; }
        std::unique_ptr<Instrument> candidate;
        juce::String result;
        try {
            candidate = std::make_unique<Instrument>();
            candidate->access = std::make_unique<FolderAccess>(r.root.toStdString(),r.bookmark.toStdString());
            double sr; int bs;
            { std::lock_guard<std::mutex> l(engineMutex); sr = rate; bs = maxBlock; }
            candidate->engine = std::make_unique<Engine>(sr,bs);
            juce::File root(juce::String::fromUTF8(candidate->access->path.c_str()));
            auto file = root.getChildFile(r.relative);
            if(r.patch) {
                auto resolved=resolveBankSample(*r.patch,r.bankFile,root);
                if(resolved.error.isNotEmpty())throw std::runtime_error(resolved.error.toStdString());
                file=resolved.file;r.relative=file.getRelativePathFrom(root);
            }
            if (!file.hasFileExtension("sfz") || !file.existsAsFile() || !candidate->engine->load(file.getFullPathName().toStdString())) {
                result = "Load failed. Re-select the library folder and check SFZ/sample paths."; candidate.reset();
            } else {
                result = file.getFileName() + " | " + juce::String(candidate->engine->regions()) + " regions, "
                       + juce::String(candidate->engine->samples()) + " preloaded samples";
                const auto unknown = candidate->engine->unknownOpcodes();
                if (!unknown.empty()) result += " | Unsupported: " + juce::String::fromUTF8(unknown.c_str());
                if(r.patch)result=juce::String::fromUTF8(r.patch->name.c_str())+(r.approximate?" [approximate FX] | ":" [SFZ only] | ")+result;
                if (candidate->engine->samples() == 0) result += " | Check samples (or oscillator-only SFZ).";
            }
        } catch (const std::exception& e) { result = "Load failed: " + juce::String(e.what()); candidate.reset(); }
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
            instrument.swap(candidate); // old engine is destroyed on this loader thread
            ready.store(instrument != nullptr); loading.store(false); status = result;
            panicRequested.store(true);
        }
        loadDone.notify_all();
    }
}
juce::String ZaZamplerProcessor::statusText() const { std::lock_guard<std::mutex> l(stateMutex); return status; }
void ZaZamplerProcessor::processBlock(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) {
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
    if (!l.owns_lock() || !instrument || loading.load()) {
        panicRequested.store(true); midi.clear(); peak.store(0); return;
    }
    auto& engine = *instrument->engine;
    if (panicRequested.exchange(false)) { engine.panic(); effects.reset();resetPerformance(); }
    engine.offline(isNonRealtime());
    float bpm = 120.f;bool playing=false;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {if (auto tempo = pos->getBpm()) bpm = static_cast<float>(*tempo);playing=pos->getIsPlaying();}
    if(wasPlaying && !playing){engine.panic();effects.reset();resetPerformance();}wasPlaying=playing;
    const int channel = static_cast<int>(parameters.getRawParameterValue("channel")->load());
    if (channel != lastChannel) { engine.panic(); effects.reset();resetPerformance();lastChannel = channel; }
    const auto fx=readFxSettings(parameters);const auto lfoSettings=readLfoSettings(parameters);
    const auto envelopeSettings=readEnvelopeSettings(parameters);
    const auto matrixSettings=readMatrixSettings(parameters);const auto seq=readSequenceSettings(parameters);
    const int voiceMode=static_cast<int>(parameters.getRawParameterValue("voiceMode")->load());
    const float glide=parameters.getRawParameterValue("glide")->load();
    if(seq.mode!=lastSequenceMode || voiceMode!=lastVoiceMode){engine.panic();effects.reset();resetPerformance();lastSequenceMode=seq.mode;lastVoiceMode=voiceMode;}
    auto event = midi.cbegin();
    // Hosts may submit larger-than-advertised buffers; split without allocation.
    for (int start=0; start<frames; start+=maxBlock) {
        const int n = juce::jmin(maxBlock,frames-start);
        engine.tempo(bpm);
        for(int i=0;i<n;++i) {
            EnvelopeEvent gateEvent;bool triggered=false;
            auto emit=[&](const unsigned char* data,int size) {
                engine.midi(data,size,i);performance.midi(data,size);
                gateEvent.merge(envelopeGate.midi(data,size));
                if(size>=3 && (data[0]&0xf0)==0x90 && data[2])triggered=true;
            };
            auto generated=[&](const unsigned char* data,int size){monoGlide.midi(data,size,voiceMode!=0,glide,emit);};
            while(event!=midi.cend() && (*event).samplePosition<=start+i) {
                const auto m=*event;
                if(m.numBytes>0 && (channel==0 || ((m.data[0]&15)+1)==channel))sequence.midi(m.data,m.numBytes,seq,generated);
                ++event;
            }
            sequence.tick(seq,bpm,generated);
            modulationFrames[static_cast<size_t>(i)]=effects.modulationTick(fx,bpm,lfoSettings,triggered,envelopeSettings,gateEvent,matrixSettings,performance);
            pitchFrames[static_cast<size_t>(i)]=monoGlide.tick(voiceMode!=0,true)+modulationFrames[static_cast<size_t>(i)].pitchCents;
        }
        engine.render(out.getWritePointer(0,start),out.getWritePointer(1,start),n,pitchFrames.data(),voiceMode!=0);
        effects.process(out.getWritePointer(0,start),out.getWritePointer(1,start),n,fx,bpm,{},nullptr,{},nullptr,modulationFrames.data());
    }
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
      tree.setProperty("root",request.root,nullptr); tree.setProperty("bookmark",request.bookmark,nullptr);
      tree.setProperty("sfz",request.relative,nullptr); tree.setProperty("schema",7,nullptr);
      tree.setProperty("bankData",bankBase64,nullptr);tree.setProperty("bankPath",bankPath,nullptr);
      tree.setProperty("bankSlot",request.bankSlot,nullptr);tree.setProperty("bankApproximate",request.approximate,nullptr); }
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml,dest);
}
void ZaZamplerProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data,size);
    if (!xml || !xml->hasTagName("QSamplerState")) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    // Missing parameters use defaults, not values left over from another preset.
    const bool legacyState=static_cast<int>(tree.getProperty("schema",1))<2;
    for(const auto& spec:parameterSpecs()) {
        auto child=tree.getChildWithProperty("id",spec.id);
        if(!child.isValid()) {
            child=juce::ValueTree("PARAM");child.setProperty("id",spec.id,nullptr);
            child.setProperty("value",legacyState && juce::String(spec.id)=="filterMode" ? 7.f : spec.initial,nullptr);
            tree.addChild(child,-1,nullptr);
        }
    }
    libraries.scan(tree["libraryRoot"].toString(),tree["libraryBookmark"].toString());
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
        pending=false; ready.store(false); loading.store(false); instrument.reset(); effects.reset();resetPerformance();loadDone.notify_all();
        status="Choose a library folder, then select an SFZ instrument.";
    }
}
juce::AudioProcessorEditor* ZaZamplerProcessor::createEditor() { return new ZaZamplerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZaZamplerProcessor(); }

void ZaZamplerProcessor::resetPerformance() {
    envelopeGate.reset();sequence.reset();monoGlide.reset();performance={};
}
