#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct ZamplerPatch {
    std::string name,comment,originalDirectory,sampleFile;
    std::array<float,128> parameters {};
    std::vector<uint8_t> opaqueRecord;
    int slot=0;
    bool hasSample() const {return !sampleFile.empty();}
};
struct ZamplerBank {
    std::string name;
    int originalProgram=0;
    std::vector<ZamplerPatch> patches;
    static ZamplerBank parse(const void* data,size_t size);
};
