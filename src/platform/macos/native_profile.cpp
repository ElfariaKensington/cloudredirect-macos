#include "native_profile.h"
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include "xdg.h"
#include "native_signatures.h"
#include "log.h"
namespace MacProfile {
static const BuildInfo kBuilds[]={
    {   // Steam client 1792618285
        "1792618285",
        {0x1d,0x28,0x26,0x16,0xb6,0x7e,0x30,0xa6,0xb0,0xaf,0xe4,0xd5,0x88,0x06,0x10,0xe7},
        "FF C3 02 D1 F6 57 08 A9 F4 4F 09 A9 FD 7B 0A A9 FD 83 02 91 F3 03 01 AA F4 03 00 AA ?? ?? ?? ?? ?? ?? ?? ?? 08 01 40 F9 A8 83 1D F8 08 00 40 F9 08 29 40 F9 00 01 3F D6",
        "F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 E2 01 F8 37 F4 03 01 AA F3 03 00 AA F5 03 02 2A 08 00 40 F9 08 15 40 F9 00 01 3F D6",
        "FF 03 02 D1 F6 57 05 A9 F4 4F 06 A9 FD 7B 07 A9 FD C3 01 91 F3 03 01 AA F4 03 00 AA 08 2B 00 F0 08 15 40 F9 08 01 40 B9 E8 00 00 34 40 27 00 B0 00 D0 13 91 A2 A3 00 D1 01 02 80 52 D2 6D 06 94",
        "FF 43 01 D1 F8 5F 01 A9 F6 57 02 A9 F4 4F 03 A9 FD 7B 04 A9 FD 03 01 91 5F 04 00 71 2B 05 00 54 F3 03 01 AA F4 03 00 AA F7 03 02 2A 55 01 80 52 78 02 40 F9 16 0B 43 29 5F 00 15 6B 55 80 95 1A",
        "FC 6F BA A9 FA 67 01 A9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91 FF C3 08 D1 FB 03 07 AA F9 03 06 AA E5 33 00 F9 F8 03 04 AA F4 03 03 AA F3 03 02 AA F5 03 01 AA F6 03 00 AA C8 87 00 90 08 B1 40 F9 08 01 40 F9 A8 03 1A F8",
        {0x59fd60,{0x0a,0x68,0x40,0xb9,0x5f,0x05,0x00,0x31,0xe0,0x01,0x00,0x54,0x08,0x40,0x40,0xf9}},
        {0x59c108,{0x3f,0x14,0x00,0x71,0x2d,0x02,0x00,0x54,0x3f,0x50,0x00,0x71,0xe8,0x03,0x00,0x54}},
        {0x59fefc,{0xf6,0x57,0xbd,0xa9,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9,0xfd,0x83,0x00,0x91}},
        {0x1096b60,{0xfc,0x6f,0xba,0xa9,0xfa,0x67,0x01,0xa9,0xf8,0x5f,0x02,0xa9,0xf6,0x57,0x03,0xa9}},
        {0x1097d50,{0xff,0xc3,0x00,0xd1,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9,0xfd,0x83,0x00,0x91}},
        {0x1097acc,{0xff,0xc3,0x00,0xd1,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9,0xfd,0x83,0x00,0x91}},
        {0x1098644,{0xc0,0x01,0x00,0xb4,0xf4,0x4f,0xbe,0xa9,0xfd,0x7b,0x01,0xa9,0xfd,0x43,0x00,0x91}},
        {0x1098608,{0xc0,0x01,0x00,0xb4,0xf4,0x4f,0xbe,0xa9,0xfd,0x7b,0x01,0xa9,0xfd,0x43,0x00,0x91}},
        {0x1098420,{0xa0,0x05,0x00,0xb4,0xf6,0x57,0xbd,0xa9,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9}},
        0x1876438,0x1518
    },
    {   // Steam client 1794424036
        "1794424036",
        {0x63,0x3e,0x66,0x24,0xa5,0x21,0x3f,0x89,0xad,0x7d,0xb3,0x32,0x0c,0x4e,0xe7,0x3b},
        "FF C3 02 D1 F6 57 08 A9 F4 4F 09 A9 FD 7B 0A A9 FD 83 02 91 F3 03 01 AA F4 03 00 AA ?? ?? ?? ?? 08 F9 47 F9 08 01 40 F9 A8 83 1D F8 08 00 40 F9 08 29 40 F9 00 01 3F D6",
        "F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 ?? ?? ?? ?? F4 03 01 AA F3 03 00 AA F5 03 02 2A 08 00 40 F9 08 15 40 F9 00 01 3F D6",
        "FF 03 02 D1 F6 57 05 A9 F4 4F 06 A9 FD 7B 07 A9 FD C3 01 91 F3 03 01 AA F4 03 00 AA ?? ?? ?? ?? 08 99 42 F9 08 01 40 B9 ?? ?? ?? ?? ?? ?? ?? ?? 00 D4 1F 91 A2 A3 00 D1 01 02 80 52 ?? ?? ?? ??",
        "FF 43 01 D1 F8 5F 01 A9 F6 57 02 A9 F4 4F 03 A9 FD 7B 04 A9 FD 03 01 91 5F 04 00 71 ?? ?? ?? ?? F3 03 01 AA F4 03 00 AA F7 03 02 2A 55 01 80 52 78 02 40 F9 16 0B 43 29 5F 00 15 6B 55 80 95 1A",
        "FC 6F BA A9 FA 67 01 A9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91 FF C3 08 D1 FB 03 07 AA F9 03 06 AA E5 33 00 F9 F8 03 04 AA F4 03 03 AA F3 03 02 AA F5 03 01 AA F6 03 00 AA ?? ?? ?? ?? 08 F9 47 F9 08 01 40 F9 A8 03 1A F8",
        {0x5b79e0,{0x0a,0x28,0x40,0xb9,0x5f,0x05,0x00,0x31,0xe0,0x01,0x00,0x54,0x08,0x20,0x40,0xf9}},
        {0x5b3e38,{0x3f,0x14,0x00,0x71,0x2d,0x02,0x00,0x54,0x3f,0x50,0x00,0x71,0xe8,0x03,0x00,0x54}},
        {0x5b7b7c,{0xf6,0x57,0xbd,0xa9,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9,0xfd,0x83,0x00,0x91}},
        {0x10c9b2c,{0xfc,0x6f,0xba,0xa9,0xfa,0x67,0x01,0xa9,0xf8,0x5f,0x02,0xa9,0xf6,0x57,0x03,0xa9}},
        {0x10cad1c,{0xff,0xc3,0x00,0xd1,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9,0xfd,0x83,0x00,0x91}},
        {0x10caa98,{0xff,0xc3,0x00,0xd1,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9,0xfd,0x83,0x00,0x91}},
        {0x10cb554,{0xc0,0x01,0x00,0xb4,0xf4,0x4f,0xbe,0xa9,0xfd,0x7b,0x01,0xa9,0xfd,0x43,0x00,0x91}},
        {0x10cb518,{0xc0,0x01,0x00,0xb4,0xf4,0x4f,0xbe,0xa9,0xfd,0x7b,0x01,0xa9,0xfd,0x43,0x00,0x91}},
        {0x10cb330,{0xa0,0x05,0x00,0xb4,0xf6,0x57,0xbd,0xa9,0xf4,0x4f,0x01,0xa9,0xfd,0x7b,0x02,0xa9}},
        0x18b3ca8,0x15c8
    },
};
const BuildInfo* FindBuild(const uint8_t u[16]){
    for(const auto& b:kBuilds)if(!memcmp(b.uuid,u,16))return &b;
    return nullptr;
}
// ---- Automatic matching for builds that are not in kBuilds ------------------------------------
// A new Steam build is accepted only when every function resolves through masked instruction
// signatures (all unique candidates must agree), the engine global and app-info cache offset are
// unanimous across the getAppInfo call sites, and the later protobuf/KV self-tests pass.
static BuildInfo gDerived;
static char gDerivedName[40];
static bool ResolveSig(const Profile& p,const char* name,void*& addr,const char*& chosen,std::string& error){
    addr=nullptr;chosen=nullptr;
    for(const auto& set:kSignatures){
        if(strcmp(set.name,name))continue;
        for(auto c:set.candidates){
            if(!c)continue;
            void* hit=p.UniquePattern(c);
            if(!hit)continue;
            if(addr&&addr!=hit){error=std::string("signatures disagree for ")+name;return false;}
            if(!addr){addr=hit;chosen=c;}
        }
    }
    if(!addr)error=std::string("no unique signature for ")+name;
    return addr!=nullptr;
}
// getAppInfo callers do: ldr xN,[global]; mov wM,#off; add x0,xN,xM; bl lock; add x0,xN,xM; ...; bl getAppInfo.
static bool DeriveEngine(const Profile& p,uintptr_t getAppInfo,uintptr_t& global,uint32_t& offset,std::string& error){
    struct Vote{uintptr_t global;uint32_t offset;int count;};
    std::vector<Vote> votes;int total=0;
    for(auto r:p.ranges){
        if(!r.executable)continue;
        for(uintptr_t a=r.begin+4*39;a+4<=r.end;a+=4){
            const uint32_t bl=*(const uint32_t*)a;
            if((bl&0xFC000000)!=0x94000000)continue;
            int64_t imm=bl&0x3FFFFFF;if(imm&0x2000000)imm-=0x4000000;
            if((uintptr_t)((int64_t)a+imm*4)!=getAppInfo)continue;
            const uint32_t* w=(const uint32_t*)(a-4*39);   // w[39] is the bl
            int addIndex=-1;uint32_t xN=0,xM=0;
            for(int i=38;i>=28;--i)if((w[i]&0xFFE0FC1F)==0x8B000000){addIndex=i;xN=(w[i]>>5)&31;xM=(w[i]>>16)&31;break;}
            if(addIndex<0)continue;
            int64_t off=-1;
            for(int i=addIndex-1;i>=0&&i>=addIndex-8;--i)if((w[i]&0xFFE0001F)==(0x52800000u|xM)){off=(w[i]>>5)&0xFFFF;break;}
            if(off<0)continue;
            for(int j=addIndex-1;j>=2;--j){
                const uint32_t ld=w[j];
                if((ld&0xFFC00000)!=0xF9400000||(ld&0x1F)!=xN||((ld>>10)&0xFFF)!=0)continue;
                const uint32_t rr=(ld>>5)&31,add=w[j-1],adrp=w[j-2];
                if((add&0xFFC00000)!=0x91000000||((add>>5)&31)!=rr||(add&0x1F)!=rr)break;
                if((adrp&0x9F00001F)!=(0x90000000u|rr))break;
                int64_t page=(int64_t)(((adrp>>5)&0x7FFFF)<<2|((adrp>>29)&3));if(page&0x100000)page-=0x200000;
                const uintptr_t pc=a-4*39+4*(j-2);
                const uintptr_t g=((pc&~(uintptr_t)0xFFF)+(uintptr_t)(page<<12))+((add>>10)&0xFFF);
                ++total;bool found=false;
                for(auto& v:votes)if(v.global==g&&v.offset==off){++v.count;found=true;break;}
                if(!found)votes.push_back({g,(uint32_t)off,1});
                break;
            }
        }
    }
    const Vote* best=nullptr;for(const auto& v:votes)if(!best||v.count>best->count)best=&v;
    if(!best||total<8||best->count*10<total*9){error="engine global/app-info cache offset not unanimous";return false;}
    if(best->offset>0x10000||best->offset%8){error="engine app-info cache offset implausible";return false;}
    bool data=false;for(auto r:p.ranges)if(!r.executable&&r.writable&&best->global>=r.begin&&best->global+8<=r.end)data=true;
    if(!data){error="engine global is not in writable data";return false;}
    global=best->global-p.slide;offset=best->offset;return true;
}
static bool DeriveBuild(Profile& p,const uint8_t uuid[16],std::string& error){
    BuildInfo b{};
    snprintf(gDerivedName,sizeof(gDerivedName),"auto-%02x%02x%02x%02x",uuid[0],uuid[1],uuid[2],uuid[3]);
    b.steamBuild=gDerivedName;memcpy(b.uuid,uuid,16);
    KvEntry* kv[]={&b.getAppInfo,&b.getSection,&b.readConfigU64,&b.kvFindKey,&b.kvGetUint64,&b.kvGetInt,&b.kvSetUint64,&b.kvSetInt,&b.kvSetString};
    const char* names[]={"getAppInfo","getSection","readConfigU64","kvFindKey","kvGetUint64","kvGetInt","kvSetUint64","kvSetInt","kvSetString"};
    void* addr;const char* chosen;
    for(size_t i=0;i<9;++i){
        if(!ResolveSig(p,names[i],addr,chosen,error))return false;
        kv[i]->rva=(uintptr_t)addr-p.slide;memcpy(kv[i]->head,addr,sizeof(kv[i]->head));
    }
    const char** aob[]={&b.serialize,&b.parse,&b.send,&b.writer,&b.manifest};
    const char* aobNames[]={"serialize","parse","send","writer","manifest"};
    for(size_t i=0;i<5;++i){if(!ResolveSig(p,aobNames[i],addr,chosen,error))return false;*aob[i]=chosen;}
    if(!DeriveEngine(p,p.slide+b.getAppInfo.rva,b.globalEnginePtrPtr,b.appInfoCacheOffset,error))return false;
    gDerived=b;p.build=&gDerived;p.derived=true;return true;
}
// Derived builds must also prove the vtable slots we replace still hold the expected functions
// (prologue + argument shuffling identical in every audited build; 0 mask = layout dependent word).
struct SlotWord{uint32_t value,mask;};
static bool SlotMatches(const Profile& p,void* fn,const SlotWord* words,size_t count){
    if(!p.Executable(fn)||!p.Executable((const char*)fn+4*count-4))return false;
    auto* code=(const uint32_t*)fn;
    for(size_t i=0;i<count;++i)if((code[i]&words[i].mask)!=words[i].value)return false;
    return true;
}
static bool DerivedSlotsValid(const Profile& p){
    static const SlotWord t5[]={{0xd10783ff,~0u},{0xa9186ffc,~0u},{0xa91967fa,~0u},{0xa91a5ff8,~0u},{0xa91b57f6,~0u},{0xa91c4ff4,~0u},{0xa91d7bfd,~0u},{0x910743fd,~0u},{0xaa0403f3,~0u},{0xaa0303f4,~0u},{0xaa0203f6,~0u},{0xaa0103f5,~0u}};
    static const SlotWord t7[]={{0xd10243ff,~0u},{0xa90657f6,~0u},{0xa9074ff4,~0u},{0xa9087bfd,~0u},{0x910203fd,~0u},{0xaa0303f3,~0u},{0xaa0203f5,~0u},{0xaa0103f6,~0u},{0xaa0003f4,~0u},{0x910003e0,~0u},{0x528012e1,~0u}};
    static const SlotWord t8[]={{0xd10783ff,~0u},{0xa9186ffc,~0u},{0xa91967fa,~0u},{0xa91a5ff8,~0u},{0xa91b57f6,~0u},{0xa91c4ff4,~0u},{0xa91d7bfd,~0u},{0x910743fd,~0u},{0xaa0503f3,~0u},{0xaa0403f4,~0u},{0xaa0303f6,~0u},{0xaa0203f7,~0u},{0xaa0103f5,~0u}};
    static const SlotWord rs24[]={{0xa9bd57f6,~0u},{0xa9014ff4,~0u},{0xa9027bfd,~0u},{0x910083fd,~0u},{0xaa0103f3,~0u},{0xaa0003f4,~0u},{0,0},{0,0},{0x340002a1,~0u},{0xb943028c,~0u},{0x3100059f,~0u},{0x540004e0,~0u},{0xf9418e88,~0u},{0x91001109,~0u}};
    return SlotMatches(p,p.transport[5],t5,sizeof(t5)/sizeof(*t5))&&SlotMatches(p,p.transport[7],t7,sizeof(t7)/sizeof(*t7))
        &&SlotMatches(p,p.transport[8],t8,sizeof(t8)/sizeof(*t8))&&SlotMatches(p,p.remoteStorage[24],rs24,sizeof(rs24)/sizeof(*rs24));
}
bool Profile::Readable(const void* p,size_t size) const {
    auto a=(uintptr_t)p;for(auto r:ranges)if(a>=r.begin&&a<=r.end&&size<=r.end-a)return true;return false;
}
bool Profile::Executable(const void* p) const {
    auto a=(uintptr_t)p;for(auto r:ranges)if(r.executable&&a>=r.begin&&a<r.end)return true;return false;
}
void** Profile::FindVtable(const char* name) const {
    static std::mutex cacheMutex;
    static std::unordered_map<std::string,void**> cache;
    const std::string key=std::to_string(ranges.empty()?0:ranges.front().begin)+":"+name;
    std::lock_guard<std::mutex> lock(cacheMutex);
    if(auto it=cache.find(key);it!=cache.end())return it->second;
    void** found=nullptr;
    for(auto r:ranges)for(auto p=r.begin;p+strlen(name)+1<=r.end;++p) {
        if(memcmp((void*)p,name,strlen(name)+1))continue;
        for(auto rr:ranges){if(rr.executable)continue;
            for(auto q=(rr.begin+7)&~(uintptr_t)7;q+8<=rr.end;q+=8) {
                if(*(uintptr_t*)q!=p||q<rr.begin+8)continue;auto ti=q-8;
                for(auto rv:ranges){if(rv.executable)continue;
                    for(auto v=((rv.begin+15)&~(uintptr_t)7);v+8*12<=rv.end;v+=8){
                        if(*(uintptr_t*)v!=ti||*(intptr_t*)(v-8)!=0)continue;
                        auto** vt=(void**)(v+8);bool valid=true;
                        for(int k=0;k<10;k++)if(!Executable(vt[k]))valid=false;
                        if(valid){if(found&&found!=vt)return nullptr;found=vt;}
                    }
                }
            }
        }
    }
    if(found)cache[key]=found;
    return found;
}
void* Profile::UniquePattern(const char* hex) const {
    std::istringstream stream(hex);std::string word;std::vector<int> bytes;
    while(stream>>word)bytes.push_back(word=="??"?-1:std::stoi(word,nullptr,16));
    if(bytes.empty())return nullptr;
    void* found=nullptr;
    for(auto r:ranges){if(!r.executable)continue;for(auto p=r.begin;p+bytes.size()<=r.end;p+=4){
        bool match=true;for(size_t i=0;i<bytes.size();i++)if(bytes[i]>=0&&((uint8_t*)p)[i]!=bytes[i]){match=false;break;}
        if(match){if(found)return nullptr;found=(void*)p;}
    }}return found;
}
bool Resolve(Profile& out,std::string& error,bool allowLoad) {
    const mach_header_64* mh=nullptr;
    auto locate=[&](){for(uint32_t i=0;i<_dyld_image_count();++i){auto n=_dyld_get_image_name(i);if(n&&strstr(n,"/steamclient.dylib")){mh=(const mach_header_64*)_dyld_get_image_header(i);out.slide=_dyld_get_image_vmaddr_slide(i);break;}}};
    locate();
    if(!mh&&allowLoad){std::string path=XdgHome()+"/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/steamclient.dylib";if(!dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL)){error=dlerror();return false;}locate();}
    if(!mh||mh->magic!=MH_MAGIC_64||mh->cputype!=CPU_TYPE_ARM64){error="ARM64 steamclient.dylib is not loaded";return false;}
    out.build=nullptr;uint8_t seen[16]={};bool haveUuid=false;
    out.ranges.clear();auto* c=(const load_command*)(mh+1);
    for(uint32_t i=0;i<mh->ncmds;++i){if(c->cmd==LC_UUID){memcpy(seen,((const uuid_command*)c)->uuid,16);haveUuid=true;out.build=FindBuild(seen);}
        if(c->cmd==LC_SEGMENT_64){auto*s=(const segment_command_64*)c;if((s->initprot&VM_PROT_READ)&&s->vmsize)out.ranges.push_back({s->vmaddr+out.slide,s->vmaddr+out.slide+s->vmsize,(bool)(s->initprot&VM_PROT_EXECUTE),(bool)(s->initprot&VM_PROT_WRITE)});}c=(const load_command*)((const char*)c+c->cmdsize);}
    // Cache only successful resolution for this loaded image so startup consumers do not rescan
    // its entire text.
    static std::mutex profileMutex;
    static Profile cached;
    std::lock_guard<std::mutex> profileLock(profileMutex);
    if(cached.transport && !out.ranges.empty() && cached.slide==out.slide && cached.ranges.front().begin==out.ranges.front().begin){out=cached;return true;}
    if(!out.build){
        // Not in kBuilds: locate every constant by signature instead of refusing the update.
        std::string why;
        if(haveUuid&&DeriveBuild(out,seen,why)){LOG("[macOS] Steam build not in table; matched by signature (%s)",out.build->steamBuild);}
        else{
            char text[48]="unknown";
            if(haveUuid)snprintf(text,sizeof(text),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",seen[0],seen[1],seen[2],seen[3],seen[4],seen[5],seen[6],seen[7],seen[8],seen[9],seen[10],seen[11],seen[12],seen[13],seen[14],seen[15]);
            error=std::string("This Steam build (steamclient UUID ")+text+") could not be matched for ARM64 cloud interception"+(why.empty()?"":" ("+why+")")+"; hooks remain inactive";return false;
        }
    }
    out.transport=out.FindVtable("30CClientUnifiedServiceTransport");
    out.remoteStorage=out.FindVtable("18CUserRemoteStorage");
    if(out.derived&&out.transport&&out.remoteStorage&&!DerivedSlotsValid(out)){error="Steam's cloud RPC vtable layout changed; hooks remain inactive";return false;}
    out.serialize=(SerializeFn)out.UniquePattern(out.build->serialize);
    out.parse=(ParseFn)out.UniquePattern(out.build->parse);
    if(!out.transport||!out.remoteStorage||!out.serialize||!out.parse){error=std::string("ARM64 profile missing:")+(out.transport?"":" transport")+(out.serialize?"":" serializer")+(out.parse?"":" parser");return false;}
    cached=out;
    return true;
}
bool TestProtobuf(const Profile& p,std::string& error){
    auto** vt=p.FindVtable("35CCloud_GetAppFileChangelist_Request");
    if(!vt||!p.Executable(vt[3])){error="Cloud request factory unavailable";return false;}
    void* object=((void*(*)(void*))vt[3])(nullptr);
    if(!object){error="Cloud request factory failed";return false;}
    const uint8_t input[]={0x08,0xec,0x04}; // AppID 620, no disk/network access.
    bool ok=p.parse(object,input,sizeof(input))!=0;
    auto** actual=*(void***)object;
    size_t size=((size_t(*)(void*))actual[9])(object);
    std::vector<uint8_t> result(size);
    if(size!=sizeof(input))ok=false;
    if(ok){auto end=p.serialize(object,result.data());ok=end==result.data()+result.size()&&!memcmp(result.data(),input,sizeof(input));}
    ((void(*)(void*))actual[1])(object);
    if(!ok)error="Native protobuf round trip failed; hooks remain inactive";
    return ok;
}
bool ReplaceSlots(void** table,const std::vector<std::pair<size_t,void*>>& slots){
    if(!table||slots.empty())return false;
    const auto page=(uintptr_t)&table[slots.front().first]&~(uintptr_t)(vm_page_size-1);
    const auto end=((uintptr_t)&table[slots.back().first]+sizeof(void*)+vm_page_size-1)&~(uintptr_t)(vm_page_size-1);
    mach_vm_address_t region=page;mach_vm_size_t size=0;vm_region_basic_info_data_64_t info{};mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=0;
    if(mach_vm_region(mach_task_self(),&region,&size,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object)!=KERN_SUCCESS)return false;
    if(object)mach_port_deallocate(mach_task_self(),object);
    if(region>page||region+size<end)return false;
    if(mach_vm_protect(mach_task_self(),page,end-page,false,VM_PROT_READ|VM_PROT_WRITE|VM_PROT_COPY)!=KERN_SUCCESS)return false;
    for(auto [slot,fn]:slots)__atomic_store_n(&table[slot],fn,__ATOMIC_RELEASE);
    return mach_vm_protect(mach_task_self(),page,end-page,false,info.protection)==KERN_SUCCESS;
}
}
