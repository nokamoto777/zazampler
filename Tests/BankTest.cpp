#include "ZamplerBank.h"
#include "BankFixture.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <limits>
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
bool rejects(const std::vector<uint8_t>& b){try{ZamplerBank::parse(b.data(),b.size());return false;}catch(const std::exception&){return true;}}
int main(int argc,char** argv){try{
    const auto fixture=bankfixture::make();auto bank=ZamplerBank::parse(fixture.data(),fixture.size());
    check(bank.patches.size()==2 && bank.patches[0].name=="Synthetic preset","synthetic names/count");
    check(bank.patches[0].sampleFile=="Sine.sfz" && !bank.patches[1].hasSample(),"sample vs empty slot");
    check(bank.patches[0].parameters[117]>0.99f,"parameter extraction");
    for(size_t length:{size_t(0),size_t(159),size_t(200),fixture.size()-1})check(rejects({fixture.begin(),fixture.begin()+static_cast<std::ptrdiff_t>(length)}),"truncation accepted");
    auto b=fixture;b[16]='X';check(rejects(b),"foreign plugin accepted");
    b=fixture;bankfixture::le(b,160,999);check(rejects(b),"unknown layout accepted");
    b=fixture;bankfixture::le(b,164,129);check(rejects(b),"bad count accepted");
    b=fixture;bankfixture::param(b,28,std::numeric_limits<float>::quiet_NaN());check(rejects(b),"NaN accepted");
    b=fixture;b.push_back(0);check(rejects(b),"trailing bytes accepted");
    // Same observed chunk layout in an FPCh single preset container.
    b.assign(60+40+12780,0);std::memcpy(b.data(),fixture.data(),28);std::memcpy(b.data()+8,"FPCh",4);
    bankfixture::be(b,4,static_cast<uint32_t>(b.size()-8));bankfixture::be(b,24,337);bankfixture::be(b,56,static_cast<uint32_t>(b.size()-60));
    std::memcpy(b.data()+60,fixture.data()+160,40+12780);bankfixture::le(b,64,1);
    check(ZamplerBank::parse(b.data(),b.size()).patches.size()==1,"FXP parse");
    if(argc>1){
        std::ifstream in(argv[1],std::ios::binary);check(bool(in),"cannot open real FXB");
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),{});auto actual=ZamplerBank::parse(data.data(),data.size());
        int samples=0;for(const auto& patch:actual.patches)if(patch.hasSample())++samples;
        check(actual.patches.size()==128 && samples==24,"Colours slot counts");
        check(actual.patches[0].name=="BS Destructive MS-20","Colours first name");
        check(actual.patches[23].name=="SY Piano Pinta","Colours last active name");
        check(actual.patches[0].sampleFile=="BS Destructive MS-20.sfz","Colours first reference");
        check(actual.patches[0].parameters[126]>0.99f && actual.patches[0].parameters[117]>0.99f,"Colours flags");
        std::cout<<"PASS: supplied Colours FXB: 128 slots / 24 SFZ references\n";
    }
    std::cout<<"PASS: FXB/FXP structure, extraction, bounds, foreign plugin, unknown layout, NaN, truncated/trailing data\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
