# Small, checked extension to the pinned sfizz 1.2.3 sources. Original files retain
# their BSD-2-Clause license. Default/null input leaves upstream audio unchanged.
function(zazampler_patch file before after)
  file(READ "${file}" text)
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
