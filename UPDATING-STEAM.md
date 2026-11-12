# Supporting a new Steam build (macOS)

The macOS runtime only activates for `steamclient.dylib` builds listed in `kBuilds`
(`src/platform/macos/native_profile.cpp`), matched by the image's `LC_UUID`. Unknown builds leave
the hooks inactive and log the UUID, so a Steam update never runs unvalidated code.

Each entry holds everything that differs per build:

| Field | What it is | How to carry it over |
| --- | --- | --- |
| `serialize`, `parse` | AOBs for the protobuf helpers | locate the old function, find the same function in the new binary, regenerate a unique AOB |
| `send`, `writer`, `manifest` | AOBs for BAsyncSend, the last-played writer, the depot dependency builder | same |
| `getAppInfo` … `kvSetString` | RVA + first 16 bytes of each native KV function | same function, new RVA and bytes |
| `globalEnginePtrPtr` | RVA of the global `CSteamEngine*` | read it from the same function's `adrp`/`ldr` in the new build |
| `appInfoCacheOffset` | where the app-info cache sits in `CSteamEngine` | read the `mov wN, #imm` / `add x0, engine, xN` that precedes each `getAppInfo` call |

Instruction-sequence matching works (mask PC-relative fields and immediates, compare functions). Struct
offsets are *not* visible that way: always read them from the new binary's disassembly, since Valve moves
them (`appInfoCacheOffset` changed from `0x1518` to `0x15c8` between 1792618285 and 1794424036).
The vtable slot numbers used in `runtime.cpp` (transport 5/7/8, remote storage 24) must also still point
at the same functions.

Then run, against the installed Steam:

```sh
cmake -S . -B build -G Ninja && ninja -C build macos_build_validation_tests macos_native_rpc_tests
build/macos_build_validation_tests   # per-build constants
build/macos_native_rpc_tests         # cloud RPC interception end to end
```
