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
    if(!out.build){
        char text[48]="unknown";
        if(haveUuid)snprintf(text,sizeof(text),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",seen[0],seen[1],seen[2],seen[3],seen[4],seen[5],seen[6],seen[7],seen[8],seen[9],seen[10],seen[11],seen[12],seen[13],seen[14],seen[15]);
        error=std::string("This Steam build (steamclient UUID ")+text+") has not been validated for ARM64 cloud interception; hooks remain inactive";return false;
    }
    // UUID validation above remains mandatory; cache only successful resolution
    // for this loaded image so startup consumers do not rescan its entire text.
    static std::mutex profileMutex;
    static Profile cached;
    std::lock_guard<std::mutex> profileLock(profileMutex);
    if(cached.transport && !out.ranges.empty() && cached.slide==out.slide && cached.ranges.front().begin==out.ranges.front().begin){out=cached;return true;}
    out.transport=out.FindVtable("30CClientUnifiedServiceTransport");
    out.remoteStorage=out.FindVtable("18CUserRemoteStorage");
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
