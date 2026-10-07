#include "Engine.h"
#include <cstdlib>
#include <stdexcept>
Engine::Engine(double rate, int block) : synth(sfizz_create_synth()) {
    if (!synth || !client) throw std::runtime_error("Cannot create sfizz engine");
    sfizz_set_num_voices(synth.get(), 128);
    prepare(rate, block);
}
void Engine::prepare(double rate, int block) {
    sfizz_set_sample_rate(synth.get(), static_cast<float>(rate));
    sfizz_set_samples_per_block(synth.get(), block);
}
bool Engine::load(const std::string& path) { return sfizz_load_file(synth.get(), path.c_str()) && regions() > 0; }
void Engine::render(float* l, float* r, int n, const float* pitchCents, bool absolutePitch) {
    sfizz_blob_t blob {reinterpret_cast<const uint8_t*>(pitchCents), pitchCents ? static_cast<uint32_t>(n * sizeof(float)) : 0u};
    sfizz_arg_t args[2] {}; args[0].b = &blob;args[1].i=absolutePitch?1:0;
    sfizz_send_message(synth.get(),client.get(),0,"/zazampler/pitch","bi",args);
    float* channels[] = {l, r}; sfizz_render_block(synth.get(), channels, 2, n);
    blob = {nullptr,0};
    sfizz_send_message(synth.get(),client.get(),0,"/zazampler/pitch","bi",args);
}
void Engine::panic() { sfizz_all_sound_off(synth.get()); }
void Engine::tempo(float bpm) { sfizz_send_bpm_tempo(synth.get(), 0, bpm); }
void Engine::offline(bool enabled) {
    if (enabled) sfizz_enable_freewheeling(synth.get()); else sfizz_disable_freewheeling(synth.get());
}
int Engine::regions() const { return sfizz_get_num_regions(synth.get()); }
int Engine::samples() const { return static_cast<int>(sfizz_get_num_preloaded_samples(synth.get())); }
std::string Engine::unknownOpcodes() const {
    char* p = sfizz_get_unknown_opcodes(synth.get());
    std::string result = p ? p : ""; free(p); return result;
}
void Engine::midi(const unsigned char* d, int n, int t) {
    if (n < 2) return;
    const int type = d[0] & 0xf0, a = d[1] & 127, b = n > 2 ? d[2] & 127 : 0;
    switch (type) {
    case 0x80: if (n >= 3) sfizz_send_note_off(synth.get(), t, a, b); break;
    case 0x90: if (n >= 3) { if (b) sfizz_send_note_on(synth.get(), t, a, b); else sfizz_send_note_off(synth.get(), t, a, 0); } break;
    case 0xa0: if (n >= 3) sfizz_send_poly_aftertouch(synth.get(), t, a, b); break;
    case 0xb0: if (n >= 3) sfizz_send_cc(synth.get(), t, a, b); break;
    case 0xc0: sfizz_send_program_change(synth.get(), t, a); break;
    case 0xd0: sfizz_send_channel_aftertouch(synth.get(), t, a); break;
    case 0xe0: if (n >= 3) sfizz_send_pitch_wheel(synth.get(), t, (a | (b << 7)) - 8192); break;
    default: break;
    }
}
