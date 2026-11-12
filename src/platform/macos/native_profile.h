#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace MacProfile {
using SerializeFn=void*(*)(void*,void*);
using ParseFn=int(*)(void*,const void*,int);
struct Range { uintptr_t begin,end; bool executable,writable; };
struct KvEntry { uintptr_t rva; uint8_t head[16]; };   // function RVA and its first 16 bytes (validated before use)
// Everything specific to one steamclient.dylib build, selected by its LC_UUID. Add a build only
// after checking every field against that binary (see UPDATING-STEAM.md).
struct BuildInfo {
    const char* steamBuild;
    uint8_t uuid[16];
    const char* serialize;        // CProtoBufMsg serialize-to-array
    const char* parse;            // CProtoBufMsg parse-from-array
    const char* send;             // BAsyncSend (stats hooks)
    const char* writer;           // last-played writer (live playtime)
    const char* manifest;         // depot dependency builder (manifest pinning)
    KvEntry getAppInfo,getSection,readConfigU64,kvFindKey,kvGetUint64,kvGetInt,kvSetUint64,kvSetInt,kvSetString;
    uintptr_t globalEnginePtrPtr; // RVA of the global CSteamEngine*
    uint32_t appInfoCacheOffset;  // offset of the app-info cache inside CSteamEngine
};
const BuildInfo* FindBuild(const uint8_t uuid[16]);
struct Profile {
    intptr_t slide=0;
    const BuildInfo* build=nullptr;
    std::vector<Range> ranges;
    void** transport=nullptr;
    void** remoteStorage=nullptr;
    SerializeFn serialize=nullptr;
    ParseFn parse=nullptr;
    bool Readable(const void* p,size_t size) const;
    bool Executable(const void* p) const;
    void** FindVtable(const char* name) const;
    void* UniquePattern(const char* hex) const;
};
bool Resolve(Profile& out,std::string& error,bool allowLoad=false);
bool TestProtobuf(const Profile& profile,std::string& error);
bool ReplaceSlots(void** vtable,const std::vector<std::pair<size_t,void*>>& slots);
}
