#pragma once
#include <sfizz.h>
#include <memory>
#include <string>

// One engine is accessed by one thread at a time. Construction/loading/configuration
// belong to the loader thread; render/event calls belong to the audio thread.
class Engine {
public:
    Engine(double sampleRate, int blockSize);
    void prepare(double sampleRate, int blockSize);
    bool load(const std::string& path);
    void midi(const unsigned char* data, int size, int offset);
    void render(float* left, float* right, int frames, const float* pitchCents = nullptr, bool absolutePitch = false);
    void panic();
    void tempo(float bpm);
    void offline(bool enabled);
    int regions() const;
    int samples() const;
    std::string unknownOpcodes() const;
private:
    struct Deleter { void operator()(sfizz_synth_t* s) const { sfizz_free(s); } };
    std::unique_ptr<sfizz_synth_t, Deleter> synth;
    struct ClientDeleter { void operator()(sfizz_client_t* c) const { sfizz_delete_client(c); } };
    std::unique_ptr<sfizz_client_t, ClientDeleter> client {sfizz_create_client(nullptr)};
};
