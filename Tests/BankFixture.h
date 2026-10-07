#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
namespace bankfixture {
inline void be(std::vector<uint8_t>& b,size_t offset,uint32_t n){b[offset]=static_cast<uint8_t>(n>>24);b[offset+1]=static_cast<uint8_t>(n>>16);b[offset+2]=static_cast<uint8_t>(n>>8);b[offset+3]=static_cast<uint8_t>(n);}
inline void le(std::vector<uint8_t>& b,size_t offset,uint32_t n){for(size_t i=0;i<4;++i)b[offset+i]=static_cast<uint8_t>(n>>(8*i));}
inline void param(std::vector<uint8_t>& b,int index,float value){uint32_t bits;std::memcpy(&bits,&value,4);le(b,200+512+static_cast<size_t>(index)*4,bits);}
inline std::vector<uint8_t> make(){
    std::vector<uint8_t> b(200+2*12780,0);
    std::memcpy(b.data(),"CcnK",4);be(b,4,static_cast<uint32_t>(b.size()-8));std::memcpy(b.data()+8,"FBCh",4);
    be(b,12,1);std::memcpy(b.data()+16,"ZMPL",4);be(b,20,1);be(b,24,2);be(b,156,static_cast<uint32_t>(b.size()-160));
    le(b,160,337);le(b,164,2);uint32_t dimensions[]={12,2,2,20,0,20,0};for(size_t i=0;i<7;++i)le(b,172+4*i,dimensions[i]);
    std::memcpy(b.data()+200,"Synthetic preset",16);std::memcpy(b.data()+200+11560,"/old/machine/",13);
    std::memcpy(b.data()+200+12560,"Sine.sfz",8);std::memcpy(b.data()+200+12780,"Init Patch",10);
    param(b,28,0.5f);param(b,29,0.25f);param(b,126,1);param(b,117,1);param(b,87,0.4f);param(b,88,0.3f);param(b,89,0.5f);
    return b;
}
}
