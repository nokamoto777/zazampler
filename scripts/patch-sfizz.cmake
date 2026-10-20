# Small, checked extension to the pinned sfizz 1.2.3 sources. Original files retain
# their BSD-2-Clause license. Default/null input leaves upstream audio unchanged.
function(zazampler_patch file before after)
  file(READ "${file}" text)
  # Earlier extensions may split a previously inserted block; recognize the
  # unique field/handler before applying that older patch again.
  foreach(marker "const auto* zazamplerSettings = static_cast<const VoiceModSettings*>" "impl.zazamplerMod_.prepare(impl.sampleRate_);" "impl.zazamplerMod_.process(buffer.getSpan(0).data()" "const float* zazamplerPitch = nullptr;" "const void* zazamplerVoiceSettings = nullptr;" "MATCH(\"/zazampler/pitch\", \"bi\")" "unsigned zazamplerOffset_ = 0;" "pitchSpan[i] += cfg.zazamplerPitch" "pitchSpan[i] += zazamplerMod_.pitch")
    string(FIND "${after}" "${marker}" in_after)
    string(FIND "${text}" "${marker}" in_file)
    if(NOT in_after EQUAL -1 AND NOT in_file EQUAL -1)
      return()
    endif()
  endforeach()
  string(FIND "${text}" "${after}" done)
  if(done EQUAL -1)
    string(FIND "${text}" "${before}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "Unsupported sfizz source for ZaZampler pitch extension: ${file}")
    endif()
    string(REPLACE "${before}" "${after}" text "${text}")
    file(WRITE "${file}" "${text}")
  endif()
endfunction()
zazampler_patch("${zazampler_sfizz}/src/sfizz/SynthConfig.h" "    bool freeWheeling { false };" "    // ZaZampler: borrowed sample-accurate cents buffer, valid only during render.\n    const float* zazamplerPitch = nullptr;\n    unsigned zazamplerPitchCount = 0;\n    bool zazamplerAbsolutePitch = false;\n    bool freeWheeling { false };")
zazampler_patch("${zazampler_sfizz}/src/sfizz/SynthMessaging.cpp" "        MATCH(\"/hello\", \"\") {" "        MATCH(\"/zazampler/pitch\", \"bi\") {\n            auto& cfg = impl.resources_.getSynthConfig();\n            cfg.zazamplerPitch = reinterpret_cast<const float*>(args[0].b->data);\n            cfg.zazamplerPitchCount = args[0].b->size / sizeof(float);\n            cfg.zazamplerAbsolutePitch = args[1].i != 0;\n        } break;\n\n        MATCH(\"/hello\", \"\") {")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    int initialDelay_ { 0 };" "    unsigned zazamplerOffset_ = 0;\n    int initialDelay_ { 0 };")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    auto delayed_buffer = buffer.subspan(delay);" "    impl.zazamplerOffset_ = static_cast<unsigned>(delay);\n    auto delayed_buffer = buffer.subspan(delay);")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    bendSmoother_.process(pitchSpan, pitchSpan);" "    bendSmoother_.process(pitchSpan, pitchSpan);\n    const auto& cfg = resources_.getSynthConfig();\n    if (cfg.zazamplerPitch)\n        for (size_t i = 0; i < numFrames && i + zazamplerOffset_ < cfg.zazamplerPitchCount; ++i)\n            pitchSpan[i] += cfg.zazamplerPitch[i + zazamplerOffset_] - (cfg.zazamplerAbsolutePitch ? 100.0f * triggerEvent_.number : 0.0f);")
# Apple Clang 17+ requires <> after the dependent template disambiguator.
# Template arguments are still deduced, so queue semantics are unchanged.
zazampler_patch("${zazampler_sfizz}/external/atomic_queue/include/atomic_queue/atomic_queue.h" "Base::template do_pop_any(" "Base::template do_pop_any<>(")
zazampler_patch("${zazampler_sfizz}/external/atomic_queue/include/atomic_queue/atomic_queue.h" "Base::template do_push_any(" "Base::template do_push_any<>(")
# Offline rendering must also drain requests not yet picked up by the dispatcher.
# Its mutex prevents a request moving from the queue to loadingJobs between checks.
zazampler_patch("${zazampler_sfizz}/src/sfizz/FilePool.cpp" "void sfz::FilePool::waitForBackgroundLoading() noexcept\n{\n    std::lock_guard<std::mutex> guard { loadingJobsMutex };" "void sfz::FilePool::waitForBackgroundLoading() noexcept\n{\n    std::lock_guard<std::mutex> guard { loadingJobsMutex };\n\n    QueuedFileData queuedData;\n    while (filesToLoad->try_pop(queuedData))\n        loadingJobs.push_back(threadPool->enqueue([this](const QueuedFileData& data) { loadingJob(data); }, std::move(queuedData)));")

# Keep the JUCE-free per-voice DSP identical in the wrapper and sfizz translation units.
foreach(header VoiceModulation ModMatrix Performance Lfo Envelope)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Source/${header}.h" "${zazampler_sfizz}/src/sfizz/zazampler/${header}.h" COPYONLY)
endforeach()
zazampler_patch("${zazampler_sfizz}/src/sfizz/SynthConfig.h" "    bool freeWheeling { false };" "    const void* zazamplerVoiceSettings = nullptr;\n    bool freeWheeling { false };")
zazampler_patch("${zazampler_sfizz}/src/sfizz/SynthMessaging.cpp" "        MATCH(\"/hello\", \"\") {" "        MATCH(\"/zazampler/voice\", \"b\") {\n            impl.resources_.getSynthConfig().zazamplerVoiceSettings = args[0].b->data;\n        } break;\n\n        MATCH(\"/hello\", \"\") {")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "#include \"Voice.h\"" "#include \"Voice.h\"\n#include \"zazampler/VoiceModulation.h\"")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    int initialDelay_ { 0 };" "    VoiceModulation zazamplerMod_;\n    int initialDelay_ { 0 };")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    impl.triggerDelay_ = delay;" "    impl.zazamplerMod_.prepare(impl.sampleRate_);\n    impl.zazamplerMod_.start(impl.triggerEvent_.number, impl.triggerEvent_.value);\n    impl.triggerDelay_ = delay;")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    modMatrix.releaseVoice(id_, region_->getId(), delay);" "    zazamplerMod_.release(delay);\n    modMatrix.releaseVoice(id_, region_->getId(), delay);")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    impl.samplesPerBlock_ = samplesPerBlock;" "    impl.samplesPerBlock_ = samplesPerBlock;\n    impl.zazamplerMod_.resize(samplesPerBlock);")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    impl.initialDelay_ -= static_cast<int>(delay);" "    impl.initialDelay_ -= static_cast<int>(delay);\n    const auto* zazamplerSettings = static_cast<const VoiceModSettings*>(impl.resources_.getSynthConfig().zazamplerVoiceSettings);\n    impl.zazamplerMod_.generate(zazamplerSettings, buffer.getNumFrames(), delay);")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    impl.powerFollower_.process(buffer);" "    impl.zazamplerMod_.process(buffer.getSpan(0).data(), buffer.getSpan(1).data(), buffer.getNumFrames(), zazamplerSettings);\n    impl.powerFollower_.process(buffer);")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    bendSmoother_.process(pitchSpan, pitchSpan);" "    bendSmoother_.process(pitchSpan, pitchSpan);\n    for (size_t i = 0; i < numFrames; ++i)\n        pitchSpan[i] += zazamplerMod_.pitch(i + zazamplerOffset_);")

# An imported amplitude envelope owns note-off decay only for SFZ's near-zero
# default release. Explicit longer SFZ releases and off-group/steal fades survive.
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    bool offed_" "    bool zazamplerOwnRelease_ = false;\n    bool offed_")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    impl.zazamplerMod_.start(impl.triggerEvent_.number, impl.triggerEvent_.value);" "    impl.zazamplerMod_.start(impl.triggerEvent_.number, impl.triggerEvent_.value);\n    impl.zazamplerOwnRelease_ = false;")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    zazamplerMod_.release(delay);" "    if (!offed_ && zazamplerOwnRelease_) {\n        egAmplitude_.setReleaseTime(1000.f);\n        egAmplitude_.stopDynamicUpdates();\n        zazamplerOwnRelease_ = true;\n    }\n    zazamplerMod_.release(delay);")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    impl.powerFollower_.process(buffer);" "    if (impl.zazamplerOwnRelease_ && (!zazamplerSettings || !zazamplerSettings->envelopes.ampEnabled || impl.zazamplerMod_.ampFinished()))\n        impl.switchState(State::cleanMeUp);\n    impl.powerFollower_.process(buffer);")

zazampler_patch("${zazampler_sfizz}/src/sfizz/Voice.cpp" "    impl.zazamplerMod_.generate(zazamplerSettings, buffer.getNumFrames(), delay);" "    if (!impl.zazamplerOwnRelease_ && !impl.offed_ && !region->flexAmpEG && region->amplitudeEG.release <= 0.001f && zazamplerSettings && zazamplerSettings->envelopes.ampEnabled && zazamplerSettings->envelopes.ownRelease) {\n        impl.zazamplerOwnRelease_ = true;\n        if (impl.released()) { impl.egAmplitude_.setReleaseTime(1000.f); impl.egAmplitude_.stopDynamicUpdates(); }\n    }\n    impl.zazamplerMod_.generate(zazamplerSettings, buffer.getNumFrames(), delay);")
# Preserve unavailable references before upstream removes unplayable regions.
# Loader-thread diagnostics only; does not change region/audio behavior.
zazampler_patch("${zazampler_sfizz}/src/sfizz/SynthPrivate.h" "    std::string rootPath_;" "    std::vector<std::string> zazamplerUnavailable_;\n    std::string rootPath_;")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Synth.cpp" "    filePool.setRootDirectory(rootDirectory);" "    filePool.setRootDirectory(rootDirectory);\n    zazamplerUnavailable_.clear();")
zazampler_patch("${zazampler_sfizz}/src/sfizz/Synth.cpp" "        layers_.erase(layers_.begin() + currentRegionIndex);" "        if (!region.isGenerator()) zazamplerUnavailable_.push_back(region.sampleId->filename());\n        layers_.erase(layers_.begin() + currentRegionIndex);")
zazampler_patch("${zazampler_sfizz}/src/sfizz/SynthMessaging.cpp" "        MATCH(\"/hello\", \"\") {" "        MATCH(\"/zazampler/unavailable\", \"\") {\n            for (const auto& file : impl.zazamplerUnavailable_) client.receive<'s'>(delay, path, file.c_str());\n        } break;\n\n        MATCH(\"/hello\", \"\") {")
