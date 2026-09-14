#ifdef NDEBUG
#undef NDEBUG
#endif
#include "recorder.cpp"
#include <cassert>
static int drains=0;
static BYTE payload[1048593];
static void Drain(void*){++drains;}
static ULONGLONG Serialize(void*,void* out){
    auto vt=*(void***)out;auto write=(ULONGLONG(*)(void*,const void*,ULONGLONG))vt[2];
    return write(out,payload,1048576)+write(out,payload+1048576,17);
}
static void Jump(BYTE* at,void* to){DWORD old;VirtualProtect(at,16,PAGE_EXECUTE_READWRITE,&old);at[0]=0x48;at[1]=0xb8;memcpy(at+2,&to,8);at[10]=0xff;at[11]=0xe0;FlushInstructionCache(GetCurrentProcess(),at,16);}
static int initCalls=0,stopCalls=0,resetCalls=0,pauseCalls=0,sceneCalls=0;
static void* mockEncoder=nullptr;
static unsigned char MockInit(void* p){++initCalls;*(void**)((BYTE*)p+0x10)=mockEncoder;return 1;}
static void MockStart(void* p){((BYTE*)p)[8]=1;}
static void MockStop(void* p){++stopCalls;((BYTE*)p)[8]=0;}
static void MockReset(void* p){++resetCalls;*(void**)((BYTE*)p+0x10)=nullptr;}
static void MockPause(void*){++pauseCalls;}
static unsigned char MockScene(void*){++sceneCalls;return 0;}
static void MockLeave(void*){++sceneCalls;}
int main(){
    Configure();
    const SIZE_T size=0xbb3e000;
    BYTE* b=(BYTE*)VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(b);
    auto dos=(IMAGE_DOS_HEADER*)b;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto nt=(IMAGE_NT_HEADERS64*)(b+0x100);nt->Signature=IMAGE_NT_SIGNATURE;nt->OptionalHeader.SizeOfImage=size;
    nt->FileHeader.TimeDateStamp=0x69c347cc;
    for(const auto& f:NativeAuto::fingerprints)memcpy(b+f.rva,f.data,f.size);
    const DWORD funcs[]={0xa67ef0,0xa67e50,0xa67930,0xa67920,0xa677b0,0xa68840,0xa67b30,0xa67ec0,0xa68230,0xa67740};
    auto vt=(void**)(b+0xd969e0);for(int i=0;i<10;i++)vt[i]=b+funcs[i];
    nt->FileHeader.TimeDateStamp=1;assert(!NativeAuto::Install((HMODULE)b));assert(b[0x8eb876]==0xe8);assert(vt[0]==b+funcs[0]);
    nt->FileHeader.TimeDateStamp=0x69c347cc;b[0x8eca61]=0;
    assert(!NativeAuto::Install((HMODULE)b));assert(b[0x8eb876]==0xe8);assert(vt[0]==b+funcs[0]);
    b[0x8eca61]=0xe8;vt[9]=nullptr;
    assert(!NativeAuto::Install((HMODULE)b));assert(b[0x8eb876]==0xe8);assert(vt[0]==b+funcs[0]);
    vt[9]=b+funcs[9];DWORD old;
    VirtualProtect(b+0x8eb876,5,PAGE_EXECUTE_READ,&old);VirtualProtect(b+0x8eca61,5,PAGE_EXECUTE_READ,&old);VirtualProtect(vt,80,PAGE_READONLY,&old);
    assert(NativeAuto::Install((HMODULE)b));assert(b[0x8eb876]==0xb0&&b[0x8eb877]==1);assert(b[0x8eca61]==0xb0&&b[0x8eca62]==1);
    assert(vt[0]==(void*)&NativeAuto::Initialize&&vt[1]==(void*)&NativeAuto::Reset&&vt[2]==(void*)&NativeAuto::Start&&vt[4]==(void*)&NativeAuto::Stop);
    assert(vt[3]==(void*)&NativeAuto::Pause&&vt[9]==b+funcs[9]);
    assert(!NativeAuto::sessionsInstalled); // Missing scene ABI must fail closed.
    for(const auto& h:NativeAuto::sceneHooks)((void**)(b+h.table))[h.slot]=b+h.original;
    assert(NativeAuto::InstallSessions());
    for(const auto& h:NativeAuto::sceneHooks)assert(((void**)(b+h.table))[h.slot]==h.replacement);
    MEMORY_BASIC_INFORMATION mbi;VirtualQuery(vt,&mbi,sizeof(mbi));assert(mbi.Protect==PAGE_READONLY);
    VirtualQuery(b+0x8eb876,&mbi,sizeof(mbi));assert(mbi.Protect==PAGE_EXECUTE_READ);
    puts("PASS: unsupported build, instruction mismatch and vtable mismatch leave code unchanged; valid install changes only selected sites and restores protections");
    // Exercise our final-export callback ABI without executing any game code.
    output=directory+L"\\auto-export-test";MakeDirectories(output);
    BYTE rec[0x108]={};void* enc[8]={};auto evt=(void**)(b+0xe376d0);
    evt[5]=b+0x4624b0;evt[6]=b+0x4626b0;enc[0]=evt;*(void**)(rec+0x10)=enc;
    Jump(b+0x4624b0,(void*)&Drain);Jump(b+0x4626b0,(void*)&Serialize);
    for(size_t i=0;i<sizeof(payload);i++)payload[i]=(BYTE)(i*17+3);
    NativeAuto::pending=rec;NativeAuto::Export(rec);assert(drains==1);assert(!NativeAuto::pending);
    NativeAuto::Export(rec);assert(drains==1);
    WIN32_FIND_DATAW find;HANDLE search=FindFirstFileW((output+L"\\*.mp4").c_str(),&find);assert(search!=INVALID_HANDLE_VALUE);FindClose(search);
    HANDLE file=CreateFileW((output+L"\\"+find.cFileName).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(file!=INVALID_HANDLE_VALUE);
    BYTE actual[1048593];DWORD got=0;assert(ReadFile(file,actual,sizeof(actual),&got,nullptr));assert(got==sizeof(payload));assert(!memcmp(actual,payload,sizeof(payload)));CloseHandle(file);
    NativeAuto::OutputStream bad{nullptr,INVALID_HANDLE_VALUE,0,0};assert(NativeAuto::WriteOutput(&bad,payload,100)==0);assert(bad.error!=0);
    puts("PASS: final export drains once, streams more than 1 MiB correctly, suppresses duplicate export, and reports write errors");
    NativeAuto::initOriginal=&MockInit;NativeAuto::startOriginal=&MockStart;NativeAuto::stopOriginal=&MockStop;
    NativeAuto::resetOriginal=&MockReset;NativeAuto::pauseOriginal=&MockPause;mockEncoder=enc;
    *(void**)(b+0xb247cb8)=rec;*(int*)(b+0xac85170)=3;
    Jump(b+0x90f220,(void*)&MockScene);
    Jump(b+0x8a91c0,(void*)&MockLeave);Jump(b+0x88b6b0,(void*)&MockLeave);
    Jump(b+0x839f60,(void*)&MockLeave);Jump(b+0x839450,(void*)&MockLeave);
    // Four stages share one live encoder; resets/stops/pauses must not truncate
    // the timeline. Only leaving the TOTAL result is allowed to export.
    for(int kind=1;kind<=2;++kind){
        void* sceneTable=b+(kind==1?0xd69d48:0xd736d8);void* scene=&sceneTable;
        *(void**)(rec+0x10)=nullptr;int before=drains,initializations=initCalls;
        for(int stage=0;stage<4;++stage){
            assert(!NativeAuto::EnterStage(scene));assert(NativeAuto::Initialize(rec));NativeAuto::Start(rec);
            int stops=stopCalls,resets=resetCalls,pauses=pauseCalls;
            NativeAuto::Stop(rec);NativeAuto::Pause(rec);NativeAuto::Reset(rec);
            assert(rec[8]==1&&NativeAuto::pending==rec&&drains==before);
            assert(stopCalls==stops&&resetCalls==resets&&pauseCalls==pauses);
        }
        assert(initCalls==initializations+1);
        if(kind==1)NativeAuto::LeaveDanResult(scene);else NativeAuto::LeaveArenaResult(scene);
        assert(drains==before+1&&!NativeAuto::session&&!NativeAuto::pending&&!rec[8]);
        // A second match/course must start with a fresh encoder; aborting the
        // mode saves the played portion even without a total-result scene.
        NativeAuto::EnterStage(scene);NativeAuto::Initialize(rec);NativeAuto::Start(rec);
        if(kind==1)NativeAuto::LeaveDanFlow(scene);else NativeAuto::LeaveArenaFlow(scene);
        assert(drains==before+2&&!NativeAuto::session&&!NativeAuto::pending);
    }
    BYTE preview[0x108]={};int stops=stopCalls;NativeAuto::Stop(preview);assert(stopCalls==stops+1);
    void* sceneTable=b+0xd69d48;NativeAuto::EnterStage(&sceneTable);NativeAuto::Initialize(rec);NativeAuto::Start(rec);
    int before=drains;*(int*)(b+0xac85170)=11;NativeAuto::Reset(rec);assert(drains==before+1&&!NativeAuto::session);
    *(int*)(b+0xac85170)=0;before=drains;
    NativeAuto::Initialize(rec);NativeAuto::Start(rec);NativeAuto::Stop(rec);NativeAuto::Reset(rec);
    assert(drains==before+1&&!NativeAuto::pending&&!rec[8]);
    VirtualFree(b,0,MEM_RELEASE);puts("PASS: Dan/Arena four-stage continuity, total-result export, second session, flow-abort fallback, mode-change fallback and preview passthrough");
}
