#include "ZamplerBank.h"
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <algorithm>
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
uint32_t be(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]);}
uint32_t le(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
std::string text(const uint8_t* p,size_t length) {
    const auto* end=std::find(p,p+length,uint8_t(0));
    std::string s(reinterpret_cast<const char*>(p),static_cast<size_t>(end-p));
    return s;
}
}
ZamplerBank ZamplerBank::parse(const void* input,size_t size) {
    require(input && size>=60 && size<=4*1024*1024,"Invalid Zampler bank size (limit 4 MiB)");
    const auto* bytes=static_cast<const uint8_t*>(input);
    require(std::memcmp(bytes,"CcnK",4)==0,"Not a VST FXB/FXP container");
    require(be(bytes+4)==size-8,"FXB/FXP container length mismatch");
    require(std::memcmp(bytes+16,"ZMPL",4)==0,"This is not a Zampler bank (expected ZMPL)");
    require(be(bytes+12)==1 || be(bytes+12)==2,"Unsupported VST file version");
    const bool bank=std::memcmp(bytes+8,"FBCh",4)==0;
    const bool patch=std::memcmp(bytes+8,"FPCh",4)==0;
    require(bank || patch,"Only Zampler chunk-based FBCh/FPCh is supported");
    const size_t chunkOffset=bank?160:60;
    require(size>=chunkOffset+40,"Truncated Zampler chunk");
    require(be(bytes+chunkOffset-4)==size-chunkOffset,"Zampler chunk length mismatch");
    const auto* chunk=bytes+chunkOffset;
    require(le(chunk)==337,"Unsupported Zampler layout (expected 337 parameters)");
    const uint32_t count=le(chunk+4);
    require(count>=1 && count<=128 && (bank || count==1),"Invalid Zampler program count");
    require(!bank || be(bytes+24)==count,"Bank program counts disagree");
    require(!patch || be(bytes+24)==337,"FXP parameter count mismatch");
    const uint32_t dimensions[]={12,2,2,20,0,20,0};
    for(size_t i=0;i<7;++i)require(le(chunk+12+i*4)==dimensions[i],"Unknown Zampler chunk dimensions");
    constexpr size_t recordSize=12780;
    require(size-chunkOffset==40+static_cast<size_t>(count)*recordSize,"Unsupported Zampler record size");
    ZamplerBank result;result.originalProgram=static_cast<int>(le(chunk+8));
    result.patches.reserve(count);
    for(uint32_t i=0;i<count;++i) {
        const auto* record=chunk+40+static_cast<size_t>(i)*recordSize;
        ZamplerPatch p;p.slot=static_cast<int>(i);p.name=text(record,32);p.comment=text(record+32,480);
        p.originalDirectory=text(record+11560,1000);p.sampleFile=text(record+12560,220);
        for(size_t j=0;j<128;++j) {
            const auto bits=le(record+512+j*4);float v;std::memcpy(&v,&bits,sizeof(v));
            // ArpMode (125) is stored as a raw enum in Colours (observed 5 and 6), not normalized.
            const float upper=j==125?6.f:1.00001f;
            require(std::isfinite(v) && v>=0.f && v<=upper,"Invalid Zampler parameter range");
            p.parameters[j]=j==125?v:std::min(v,1.f);
        }
        p.opaqueRecord.assign(record,record+recordSize);
        result.patches.push_back(std::move(p));
    }
    return result;
}
