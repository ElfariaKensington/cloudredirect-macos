// Explicit optional test against an installed Steam: every per-build constant must hold for the
// steamclient.dylib that is installed. Run it after a Steam update (it needs no other state).
//   - the build is recognised by its LC_UUID,
//   - the protobuf helpers resolve and round-trip,
//   - every native KV entry passes its byte check and the KV functions work,
//   - the stats, live-playtime and manifest-pinning entry points resolve uniquely,
//   - the cloud transport / remote-storage vtable slots we replace point at code.
#include "native_profile.h"
#include "native_kv.h"
#include "live_playtime.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
static int failures=0;
static void Check(bool ok,const char* what){printf("  %s %s\n",ok?"ok  ":"FAIL",what);if(!ok)++failures;}
int main(){
    MacProfile::Profile p;std::string error;
    if(!MacProfile::Resolve(p,error,true)){printf("FAIL: %s\n",error.c_str());return 1;}
    printf("Steam client build %s recognised\n",p.build->steamBuild);
    Check(MacProfile::TestProtobuf(p,error),"protobuf round trip");
    SteamKvInjector::ResolvedKvAddrs kv;
    bool kvOk=MacProfile::ResolveKv(p,kv,error);Check(kvOk,"native KV entries validate");
    if(kvOk){error.clear();Check(MacProfile::TestKv(kv,error,true),"native KV values and tree lookups work");}
    if(!error.empty())printf("       %s\n",error.c_str());
    Check(p.UniquePattern(p.build->send)!=nullptr,"BAsyncSend (stats) resolves uniquely");
    Check(p.UniquePattern(p.build->writer)!=nullptr,"last-played writer resolves uniquely");
    Check(p.UniquePattern(p.build->manifest)!=nullptr,"depot dependency builder resolves uniquely");
    Check(LivePlaytime::Resolve(0,0,nullptr),"live playtime entries and response layout");
    Check(p.transport&&p.Executable(p.transport[5])&&p.Executable(p.transport[7])&&p.Executable(p.transport[8]),"transport slots 5, 7, 8 are code");
    Check(p.remoteStorage&&p.Executable(p.remoteStorage[24]),"remote storage slot 24 is code");
    printf("%s\n",failures?"Steam build validation: FAIL":"Steam build validation: PASS");fflush(stdout);_Exit(failures?1:0);
}
