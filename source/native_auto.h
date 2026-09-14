// IIDX33 build 0x69c347cc. Offsets are RVAs, checked against instructions and
// the entire recorder vtable BEFORE changing anything. Never patch on mismatch.
#include "recording_name.h"
namespace NativeAuto {
static BYTE* base=nullptr;
using InitFn=unsigned char(*)(void*);
using Method=void(*)(void*);
static InitFn initOriginal=nullptr;
static Method resetOriginal=nullptr,startOriginal=nullptr,stopOriginal=nullptr,pauseOriginal=nullptr;
static int fps=0,width=0,height=0,aac=0;
static void* pending=nullptr;
static int session=0,sessionMode=-1;
static bool sessionsInstalled=false;
static bool metadataSupported=false;
static RecordingName::Metadata recordingMetadata;
static void FinishSession(const wchar_t* reason);
static bool IsMain(void* p){return base && p && *(void**)(base+0xb247cb8)==p;}
static bool KeepContinuous(void* p){
    if(!session||!IsMain(p))return false;
    if(*(int*)(base+0xac85170)!=sessionMode){FinishSession(L"mode changed");return false;}
    return true;
}
static void Trace(const wchar_t* action,void* p){
    wchar_t message[320];auto r=(BYTE*)p;
    int mode=*(int*)(base+0xac85170);
    swprintf(message,320,L"AUTO %ls recorder=%ls mode_id=%d active=%u encoder=%p",action,IsMain(p)?L"main":L"preview",mode,r[8],*(void**)(r+0x10));Log(message);
}
static unsigned char Initialize(void* p){
    auto r=(BYTE*)p;
    if(KeepContinuous(p)&&*(void**)(r+0x10)){Trace(L"continuous reuse encoder",p);return 1;}
    if(IsMain(p)){
        if(width&&height){*(int*)(r+0x50)=width;*(int*)(r+0x54)=height;}
        if(fps){*(int*)(r+0x58)=fps;*(int*)(r+0x5c)=1;}
        if(aac)*(int*)(r+0x70)=aac;
    }
    Trace(L"initialize begin",p);
    auto ok=initOriginal(p);
    Trace(ok?L"initialize success":L"initialize failed",p);
    return ok;
}
struct OutputStream { void** vtable; HANDLE file; DWORD error; ULONGLONG total; };
static ULONGLONG WriteOutput(OutputStream* out,const void* data,ULONGLONG n){
    ULONGLONG done=0;
    while(done<n&&!out->error){DWORD want=(DWORD)((n-done)>1048576?1048576:n-done),written=0;
        if(!WriteFile(out->file,(const BYTE*)data+done,want,&written,nullptr)||written!=want){out->error=GetLastError();if(!out->error)out->error=ERROR_WRITE_FAULT;break;}done+=written;
    }
    out->total+=done;return done;
}
static void Export(void* p){
    if(p!=pending)return;
    pending=nullptr;
    auto r=(BYTE*)p;void* encoder=*(void**)(r+0x10);
    if(!encoder)return;
    auto vt=*(void***)encoder;
    if(vt!=(void**)(base+0xe376d0)||vt[5]!=base+0x4624b0||vt[6]!=base+0x4626b0){Log(L"AUTO final export skipped: unknown encoder ABI",E_FAIL);return;}
    if(r[8])stopOriginal(p);
    if(!recordingMetadata.started.wYear)recordingMetadata=RecordingName::Capture(base,metadataSupported);
    std::wstring final,temp;
    HANDLE file=RecordingName::Reserve(recordingMetadata,final,temp);
    if(file==INVALID_HANDLE_VALUE){Log(L"AUTO cannot create final recording file",HRESULT_FROM_WIN32(GetLastError()));return;}
    Log(L"AUTO draining native encoders and merging tracks");
    ((Method)vt[5])(encoder);
    // The checked native export routine (0x4626b0) calls only IPlayVideoStream's
    // Write slot (2), in 1 MiB blocks. No cross-runtime STL objects are passed.
    void* outputVtable[5]={nullptr,nullptr,(void*)&WriteOutput,nullptr,nullptr};
    OutputStream out{outputVtable,file,0,0};
    auto bytes=((ULONGLONG(*)(void*,void*))vt[6])(encoder,&out);
    if(!FlushFileBuffers(file)&&!out.error)out.error=GetLastError();CloseHandle(file);
    if(out.error||!bytes||out.total!=bytes){Log(L"AUTO final export failed; partial file retained",HRESULT_FROM_WIN32(out.error?out.error:ERROR_NO_DATA));return;}
    if(!MoveFileExW(temp.c_str(),final.c_str(),MOVEFILE_WRITE_THROUGH)){Log(L"AUTO final rename failed; partial file retained",HRESULT_FROM_WIN32(GetLastError()));return;}
    Log(final.c_str());wchar_t message[120];swprintf(message,120,L"AUTO FINAL SAVED bytes=%llu",bytes);Log(message);
}
static void FinishSession(const wchar_t* reason){
    if(!session)return;
    wchar_t msg[200];swprintf(msg,200,L"AUTO SESSION END kind=%d reason=%ls",session,reason);Log(msg);
    session=0;sessionMode=-1;
    void* p=*(void**)(base+0xb247cb8);
    if(p){Export(p);stopOriginal(p);resetOriginal(p);}
}
static void Reset(void* p){if(KeepContinuous(p)){Trace(L"continuous defer reset",p);return;}Trace(L"finalize/reset begin",p);if(IsMain(p))Export(p);resetOriginal(p);Trace(L"finalize/reset done",p);}
static void Start(void* p){startOriginal(p);if(IsMain(p)&&((BYTE*)p)[8]){if(pending!=p){recordingMetadata=RecordingName::Capture(base,metadataSupported);Log((L"AUTO recording name: "+RecordingName::Stem(recordingMetadata)).c_str());}pending=p;}Trace(L"start",p);}
static void Stop(void* p){if(KeepContinuous(p)){Trace(L"continuous keep running",p);return;}stopOriginal(p);Trace(L"stop",p);}
static void Pause(void* p){if(KeepContinuous(p)){Trace(L"continuous ignore pause",p);return;}pauseOriginal(p);}
// Scene lifecycle virtuals: slot 13 enters, slot 14 leaves. Use concrete RTTI
// vtables instead of guessing mode IDs. All calls remain on the game thread.
struct SceneHook {DWORD table;unsigned slot;DWORD original;void* replacement;};
static unsigned char EnterStage(void* p){
    int kind=*(void***)p==(void**)(base+0xd69d48)?1:2;
    if(session&&session!=kind)FinishSession(L"different course/match");
    if(!session){
        // Close any preceding single-song recording before taking ownership.
        void* main=*(void**)(base+0xb247cb8);
        if(main&&pending==main){Export(main);resetOriginal(main);}
        session=kind;sessionMode=*(int*)(base+0xac85170);
        wchar_t msg[160];swprintf(msg,160,L"AUTO SESSION BEGIN kind=%d mode_id=%d",kind,sessionMode);Log(msg);
    }
    return ((InitFn)(base+0x90f220))(p);
}
static void LeaveDanResult(void* p){FinishSession(L"Dan total result leave");((Method)(base+0x8a91c0))(p);}
static void LeaveArenaResult(void* p){FinishSession(L"Arena total result leave");((Method)(base+0x88b6b0))(p);}
static void LeaveDanFlow(void* p){FinishSession(L"Dan flow leave");((Method)(base+0x839f60))(p);}
static void LeaveArenaFlow(void* p){FinishSession(L"Arena flow leave");((Method)(base+0x839450))(p);}
static const SceneHook sceneHooks[]={
    {0xd69d48,13,0x90f220,(void*)&EnterStage},
    {0xd736d8,13,0x90f220,(void*)&EnterStage},
    {0xd437d0,14,0x8a91c0,(void*)&LeaveDanResult},
    {0xd3c7b0,14,0x88b6b0,(void*)&LeaveArenaResult},
    {0xd70c28,14,0x839f60,(void*)&LeaveDanFlow},
    {0xd70b68,14,0x839450,(void*)&LeaveArenaFlow},
};
static bool InstallSessions(){
    if(!GetPrivateProfileIntW(L"Native",L"ContinuousCourses",1,ini.c_str()))return false;
    for(const auto& h:sceneHooks)if(((void**)(base+h.table))[h.slot]!=base+h.original){Log(L"AUTO continuous disabled: scene vtable mismatch; single-song recording remains available");return false;}
    DWORD old[6]={},unused;unsigned count=0;
    for(const auto& h:sceneHooks){
        if(!VirtualProtect(base+h.table+h.slot*8,8,PAGE_READWRITE,&old[count])){
            while(count){--count;const auto& prev=sceneHooks[count];VirtualProtect(base+prev.table+prev.slot*8,8,old[count],&unused);}return false;
        }++count;
    }
    for(const auto& h:sceneHooks)InterlockedExchangePointer((PVOID volatile*)(base+h.table+h.slot*8),h.replacement);
    // Restore in reverse order: multiple slots can share a protected page.
    while(count){--count;const auto& h=sceneHooks[count];VirtualProtect(base+h.table+h.slot*8,8,old[count],&unused);}
    Log(L"AUTO continuous scene hooks installed: Dan/Arena first stage through total result; flow-exit fallback");return true;
}
struct Fingerprint {DWORD rva;const char* data;size_t size;};
static const Fingerprint fingerprints[]={
    {0x8eb876,"\xe8\x55\xb7\xf6\xff\x84\xc0",7},
    {0x8eca61,"\xe8\x6a\xa5\xf6\xff\x84\xc0",7},
    {0xa67ef0,"\x40\x55\x56\x57\x41\x56\x41\x57\x48\x8d\x6c\x24\xb0\x48\x81\xec",16},
    {0x925000,"\x8b\x05\x6a\x01\x36\x0a\xc3",7},
    {0xa67930,"\x48\x8b\xc4\x57\x48\x81\xec",7},
    {0xa677b0,"\x48\x8b\xc4\x55\x48\x8d\x68\xd8",8},
    {0x4626b0,"\x40\x55\x56\x57\x41\x56\x41\x57\x48\x83\xec\x50\x48\xc7\x44\x24",16},
};
static bool Install(HMODULE module){
    if(!GetPrivateProfileIntW(L"Native",L"AutoRecord",1,ini.c_str())){Log(L"Automatic native recording is disabled in INI");return false;}
    auto b=(BYTE*)module;auto dos=(IMAGE_DOS_HEADER*)b;
    auto nt=(IMAGE_NT_HEADERS64*)(b+dos->e_lfanew);
    if(nt->FileHeader.TimeDateStamp!=0x69c347cc||nt->OptionalHeader.SizeOfImage!=0xbb3e000){Log(L"AUTO disabled: unsupported game build (no native code changed)",E_NOTIMPL);return false;}
    for(const auto& f:fingerprints)if(memcmp(b+f.rva,f.data,f.size)){Log(L"AUTO disabled: instruction fingerprint mismatch (no native code changed)",E_FAIL);return false;}
    const DWORD expected[]={0xa67ef0,0xa67e50,0xa67930,0xa67920,0xa677b0,0xa68840,0xa67b30,0xa67ec0,0xa68230,0xa67740};
    auto vt=(void**)(b+0xd969e0);
    for(int i=0;i<10;i++)if(vt[i]!=(void*)(b+expected[i])){Log(L"AUTO disabled: recorder vtable mismatch (no native code changed)",E_FAIL);return false;}
    // Prepare all writable pages before the first mutation, avoiding half-install.
    DWORD oldVt=0,old1=0,old2=0,unused;
    if(!VirtualProtect(vt,80,PAGE_READWRITE,&oldVt)){Log(L"AUTO vtable protection failed");return false;}
    if(!VirtualProtect(b+0x8eb876,5,PAGE_EXECUTE_READWRITE,&old1)){VirtualProtect(vt,80,oldVt,&unused);return false;}
    if(!VirtualProtect(b+0x8eca61,5,PAGE_EXECUTE_READWRITE,&old2)){VirtualProtect(b+0x8eb876,5,old1,&unused);VirtualProtect(vt,80,oldVt,&unused);return false;}
    base=b;
    fps=GetPrivateProfileIntW(L"Native",L"FPS",0,ini.c_str());if(fps!=30&&fps!=60&&fps!=120)fps=0;
    width=GetPrivateProfileIntW(L"Native",L"Width",0,ini.c_str());height=GetPrivateProfileIntW(L"Native",L"Height",0,ini.c_str());
    if(width<640||width>3840||height<360||height>2160||(width%2)||(height%2)){width=height=0;}
    aac=GetPrivateProfileIntW(L"Native",L"AACBitrateKbps",0,ini.c_str());if(aac<64||aac>320||(aac%32))aac=0;
    initOriginal=(InitFn)vt[0];resetOriginal=(Method)vt[1];startOriginal=(Method)vt[2];pauseOriginal=(Method)vt[3];stopOriginal=(Method)vt[4];
    InterlockedExchangePointer((PVOID volatile*)&vt[0],(PVOID)&Initialize);
    InterlockedExchangePointer((PVOID volatile*)&vt[1],(PVOID)&Reset);
    InterlockedExchangePointer((PVOID volatile*)&vt[2],(PVOID)&Start);
    InterlockedExchangePointer((PVOID volatile*)&vt[3],(PVOID)&Pause);
    InterlockedExchangePointer((PVOID volatile*)&vt[4],(PVOID)&Stop);
    // Only the two StageInit/retry call sites: no region/account/server/menu patch.
    const BYTE localEligible[]={0xb0,1,0x90,0x90,0x90};
    memcpy(b+0x8eb876,localEligible,5);memcpy(b+0x8eca61,localEligible,5);
    FlushInstructionCache(GetCurrentProcess(),b+0x8eb876,5);FlushInstructionCache(GetCurrentProcess(),b+0x8eca61,5);
    VirtualProtect(b+0x8eca61,5,old2,&unused);VirtualProtect(b+0x8eb876,5,old1,&unused);VirtualProtect(vt,80,oldVt,&unused);
    nativeAutoActive=true;
    // Verify the small native readers and Dan-name call site before interpreting
    // their data. A mismatch omits detail instead of guessing or calling game code.
    metadataSupported=!memcmp(base+0x924ff0,"\x48\x8b\x05\xa9\x01\x36\x0a\xc3",8)
        &&!memcmp(base+0x924fe0,"\x8b\x05\x8e\x01\x36\x0a\xc3",7)
        &&!memcmp(base+0x839fc3,"\x8b\x15\x47\x6e\x22\x0a",6);
    if(!metadataSupported)Log(L"AUTO title/grade metadata unavailable: reader fingerprint mismatch");
    sessionsInstalled=InstallSessions();
    Log(L"AUTO v0.6 installed: optional filename time; dated mode/title filenames; continuous courses");return true;
}
}
