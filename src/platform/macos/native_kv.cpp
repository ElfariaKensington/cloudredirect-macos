#include "native_kv.h"
#include <cstring>
namespace MacProfile {
bool ResolveKv(const Profile& profile,SteamKvInjector::ResolvedKvAddrs& out,std::string& error){
    // The build table is only used after Resolve matched the exact ARM64 Mach-O UUID. Each
    // entry additionally verifies its audited instruction bytes before it can be used.
    const auto& b=*profile.build;
    auto entry=[&](const KvEntry& e,uintptr_t& dest,const char* name)->bool{
        auto address=profile.slide+e.rva;
        if(!profile.Executable((void*)address)||memcmp((void*)address,e.head,sizeof(e.head))){error=std::string("Native KV entry failed validation: ")+name;return false;}
        dest=address;return true;
    };
    if(!entry(b.getAppInfo,out.getAppInfo,"getAppInfo"))return false;
    if(!entry(b.getSection,out.getSection,"getSection"))return false;
    if(!entry(b.readConfigU64,out.readConfigU64,"readConfigU64"))return false;
    if(!entry(b.kvFindKey,out.kvFindKey,"kvFindKey"))return false;
    if(!entry(b.kvGetUint64,out.kvGetUint64,"kvGetUint64"))return false;
    if(!entry(b.kvGetInt,out.kvGetInt,"kvGetInt"))return false;
    if(!entry(b.kvSetUint64,out.kvSetUint64,"kvSetUint64"))return false;
    if(!entry(b.kvSetInt,out.kvSetInt,"kvSetInt"))return false;
    if(!entry(b.kvSetString,out.kvSetString,"kvSetString"))return false;
    out.globalEnginePtrPtr=(void**)(profile.slide+b.globalEnginePtrPtr);
    if(!profile.Readable(out.globalEnginePtrPtr,sizeof(void*))){error="Native engine global is outside the audited image";return false;}
    out.appInfoCacheOffset=b.appInfoCacheOffset;return true;
}
bool TestKv(const SteamKvInjector::ResolvedKvAddrs& a,std::string& error,bool includeTree){
    // A standalone value node: never references Steam's live app-info cache.
    alignas(16) unsigned char node[64]={};
    auto setInt=(void(*)(void*,int))a.kvSetInt;auto getInt=(int(*)(void*,int,void*))a.kvGetInt;
    auto setU64=(void(*)(void*,uint64_t))a.kvSetUint64;auto getU64=(uint64_t(*)(void*,uint64_t,void*))a.kvGetUint64;
    auto setString=(void(*)(void*,const char*))a.kvSetString;
    setInt(node,12345);bool ok=getInt(node,0,nullptr)==12345;
    setU64(node,0x123456789abcdefULL);ok=ok&&getU64(node,0,nullptr)==0x123456789abcdefULL;
    setString(node,"2468");ok=ok&&getInt(node,0,nullptr)==2468;
    setInt(node,0); // Release any native string allocation before returning.
    if(includeTree){
        alignas(16) unsigned char tree[64]={};
        auto find=(void*(*)(void*,const char*,uint8_t,void*))a.kvFindKey;
        auto* quota=find(tree,"ufs/quota",1,nullptr);
        ok=ok&&quota;
        if(quota){setU64(quota,1073741824ULL);ok=ok&&find(tree,"ufs/quota",0,nullptr)==quota&&getU64(quota,0,nullptr)==1073741824ULL;}
        // Tree nodes are allocated in this short-lived diagnostic process only;
        // the runtime self-test never allocates a tree or touches the live cache.
    }
    if(!ok)error="Native KV value round trip failed";return ok;
}
}
