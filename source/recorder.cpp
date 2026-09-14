#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <string>
#include <cstdio>
#include <new>

// No audio device or graphics hook: retain the game's encoded audio/video.
using CreateSink = HRESULT (WINAPI*)(IMFByteStream*,IMFMediaType*,IMFMediaType*,IMFMediaSink**);
static CreateSink originalSink = nullptr, fragmentedSink = nullptr;
static std::wstring directory, ini, output;
static LONG serial = 0;
static bool fragmented = false;
static bool dateFolders = false;
static bool filenameTime = false;
static DWORD flushMs = 1000;
static SRWLOCK initLock = SRWLOCK_INIT;
static bool configured = false;
static bool nativeAutoActive = false;

static void Log(const wchar_t* message, HRESULT hr = S_OK) {
    auto path = directory + L"\\IIDXRecorder.log";
    HANDLE h=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return;
    SYSTEMTIME t;GetLocalTime(&t); wchar_t w[1200];
    swprintf(w,1200,L"%04u-%02u-%02u %02u:%02u:%02u %ls (0x%08lx)\r\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,message,(unsigned long)hr);
    char u[4096];int n=WideCharToMultiByte(CP_UTF8,0,w,-1,u,sizeof(u),nullptr,nullptr);DWORD done;
    if(n>1)WriteFile(h,u,n-1,&done,nullptr);CloseHandle(h);
}
static bool MakeDirectories(const std::wstring& p) {
    if(p.empty())return false;
    for(size_t i=3;i<p.size();++i)if(p[i]==L'\\'||p[i]==L'/')CreateDirectoryW(p.substr(0,i).c_str(),nullptr);
    return CreateDirectoryW(p.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS;
}
static void Configure() {
    AcquireSRWLockExclusive(&initLock);
    if(!configured){
        HMODULE mod=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&Configure,&mod);
        wchar_t path[32768];GetModuleFileNameW(mod,path,32768);directory=path;directory.resize(directory.find_last_of(L"\\/"));
        ini=directory+L"\\IIDXRecorder.ini";
        wchar_t val[32768];GetPrivateProfileStringW(L"Recorder",L"OutputPath",L"recordings",val,32768,ini.c_str());
        wchar_t expanded[32768];ExpandEnvironmentStringsW(val,expanded,32768);output=expanded;
        if(output.find(L':')==std::wstring::npos&&output.rfind(L"\\\\",0)!=0)output=directory+L"\\"+output;
        fragmented=GetPrivateProfileIntW(L"Recorder",L"FragmentedMP4",0,ini.c_str())!=0;
        dateFolders=GetPrivateProfileIntW(L"Recorder",L"DateFolders",0,ini.c_str())==1;
        filenameTime=GetPrivateProfileIntW(L"Recorder",L"FilenameTime",0,ini.c_str())==1;
        flushMs=GetPrivateProfileIntW(L"Recorder",L"FlushIntervalMs",1000,ini.c_str());if(flushMs<100)flushMs=100;if(flushMs>10000)flushMs=10000;
        HMODULE mf=LoadLibraryW(L"mf.dll");
        originalSink=(CreateSink)GetProcAddress(mf,"MFCreateMPEG4MediaSink");
        fragmentedSink=(CreateSink)GetProcAddress(mf,"MFCreateFMPEG4MediaSink");
        configured=true;
    }
    ReleaseSRWLockExclusive(&initLock);
}

// Mirror at the original stream's offset. The game's own byte stream remains
// the authority for return values and asynchronous callback semantics.
class MirrorStream final:public IMFByteStream {
    LONG refs=1;
    IMFByteStream* real;
    HANDLE file=INVALID_HANDLE_VALUE;
    CRITICAL_SECTION lock;
    ULONGLONG lastFlush=0;
    bool failed=false;
    void Mirror(const BYTE* data,ULONG bytes) {
        EnterCriticalSection(&lock);
        if(file!=INVALID_HANDLE_VALUE&&!failed){
            QWORD pos=0;HRESULT hr=real->GetCurrentPosition(&pos);
            LARGE_INTEGER offset;offset.QuadPart=pos;DWORD written=0;
            if(FAILED(hr)||!SetFilePointerEx(file,offset,nullptr,FILE_BEGIN)||!WriteFile(file,data,bytes,&written,nullptr)||written!=bytes){
                failed=true;Log(L"LOCAL WRITE FAILED: check free space and permissions",FAILED(hr)?hr:HRESULT_FROM_WIN32(GetLastError()));
            }
            if(GetTickCount64()-lastFlush>=flushMs){
                if(!FlushFileBuffers(file))Log(L"FlushFileBuffers failed",HRESULT_FROM_WIN32(GetLastError()));
                lastFlush=GetTickCount64();
            }
        }
        LeaveCriticalSection(&lock);
    }
    void CloseLocal(){
        EnterCriticalSection(&lock);
        if(file!=INVALID_HANDLE_VALUE){FlushFileBuffers(file);CloseHandle(file);file=INVALID_HANDLE_VALUE;Log(L"Local recording stream closed");}
        LeaveCriticalSection(&lock);
    }
public:
    MirrorStream(IMFByteStream* r,HANDLE f):real(r),file(f){real->AddRef();InitializeCriticalSection(&lock);}
    ~MirrorStream(){CloseLocal();DeleteCriticalSection(&lock);real->Release();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p) override {
        if(!p)return E_POINTER;*p=nullptr;
        if(IsEqualIID(id,IID_IUnknown)||IsEqualIID(id,__uuidof(IMFByteStream))){*p=static_cast<IMFByteStream*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetCapabilities(DWORD* p) override{return real->GetCapabilities(p);}
    HRESULT STDMETHODCALLTYPE GetLength(QWORD* p) override{return real->GetLength(p);}
    HRESULT STDMETHODCALLTYPE SetLength(QWORD n) override{
        HRESULT hr=real->SetLength(n);if(SUCCEEDED(hr)){
            EnterCriticalSection(&lock);if(file!=INVALID_HANDLE_VALUE){LARGE_INTEGER v;v.QuadPart=n;if(!SetFilePointerEx(file,v,nullptr,FILE_BEGIN)||!SetEndOfFile(file))Log(L"Local SetLength failed",HRESULT_FROM_WIN32(GetLastError()));}LeaveCriticalSection(&lock);
        }return hr;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(QWORD* p) override{return real->GetCurrentPosition(p);}
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(QWORD p) override{return real->SetCurrentPosition(p);}
    HRESULT STDMETHODCALLTYPE IsEndOfStream(BOOL* p) override{return real->IsEndOfStream(p);}
    HRESULT STDMETHODCALLTYPE Read(BYTE* p,ULONG n,ULONG* d) override{return real->Read(p,n,d);}
    HRESULT STDMETHODCALLTYPE BeginRead(BYTE* p,ULONG n,IMFAsyncCallback* cb,IUnknown* s) override{return real->BeginRead(p,n,cb,s);}
    HRESULT STDMETHODCALLTYPE EndRead(IMFAsyncResult* r,ULONG* d) override{return real->EndRead(r,d);}
    HRESULT STDMETHODCALLTYPE Write(const BYTE* p,ULONG n,ULONG* d) override{Mirror(p,n);return real->Write(p,n,d);}
    HRESULT STDMETHODCALLTYPE BeginWrite(const BYTE* p,ULONG n,IMFAsyncCallback* cb,IUnknown* s) override{Mirror(p,n);return real->BeginWrite(p,n,cb,s);}
    HRESULT STDMETHODCALLTYPE EndWrite(IMFAsyncResult* r,ULONG* d) override{return real->EndWrite(r,d);}
    HRESULT STDMETHODCALLTYPE Seek(MFBYTESTREAM_SEEK_ORIGIN o,LONGLONG off,DWORD f,QWORD* p) override{return real->Seek(o,off,f,p);}
    HRESULT STDMETHODCALLTYPE Flush() override{EnterCriticalSection(&lock);if(file!=INVALID_HANDLE_VALUE)FlushFileBuffers(file);LeaveCriticalSection(&lock);return real->Flush();}
    HRESULT STDMETHODCALLTYPE Close() override{CloseLocal();return real->Close();}
};

extern "C" __declspec(dllexport) HRESULT WINAPI RecorderCreateSink(IMFByteStream* stream,IMFMediaType* video,IMFMediaType* audio,IMFMediaSink** sink){
    Configure();
    if(!originalSink)return E_NOTIMPL;
    if(!stream||!sink)return E_POINTER;
    // Auto mode exports the game's final merged file. Its MF sinks can contain
    // separate intermediate tracks; do not mirror or alter these streams.
    if(nativeAutoActive)return originalSink(stream,video,audio,sink);
    SYSTEMTIME t;GetLocalTime(&t);wchar_t name[200];
    std::wstring target=output;
    if(dateFolders){wchar_t date[32];swprintf(date,32,L"\\%04u-%02u-%02u",t.wYear,t.wMonth,t.wDay);target+=date;}
    if(!MakeDirectories(target)){Log(L"Cannot create output directory",HRESULT_FROM_WIN32(GetLastError()));return originalSink(stream,video,audio,sink);}
    swprintf(name,200,L"\\IIDX_%04u%02u%02u_%02u%02u%02u_%03u_%lu_%ld.mp4",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetCurrentProcessId(),InterlockedIncrement(&serial));
    std::wstring path=target+name;
    HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE){Log(L"Cannot open local recording",HRESULT_FROM_WIN32(GetLastError()));return originalSink(stream,video,audio,sink);}
    auto mirror=new(std::nothrow) MirrorStream(stream,f);
    if(!mirror){CloseHandle(f);return originalSink(stream,video,audio,sink);}
    Log(path.c_str());
    UINT32 width=0,height=0,num=0,den=0;
    if(video){MFGetAttributeSize(video,MF_MT_FRAME_SIZE,&width,&height);MFGetAttributeRatio(video,MF_MT_FRAME_RATE,&num,&den);}
    wchar_t info[200];swprintf(info,200,L"Native mux: video=%ux%u fps=%u/%u audio=%s fragmented=%u",width,height,num,den,audio?L"yes":L"no",fragmented);Log(info);
    HRESULT hr;
    if(fragmented&&fragmentedSink)hr=fragmentedSink(mirror,video,audio,sink);else hr=originalSink(mirror,video,audio,sink);
    mirror->Release();
    if(FAILED(hr)){Log(L"Requested mux creation failed; falling back to game's original mux without local capture",hr);return originalSink(stream,video,audio,sink);}
    Log(L"Native recording mirror active",hr);return hr;
}

static bool Install(HMODULE game){
    auto base=(BYTE*)game;auto dos=(IMAGE_DOS_HEADER*)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
    auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
    auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!dir.VirtualAddress)return false;
    auto desc=(IMAGE_IMPORT_DESCRIPTOR*)(base+dir.VirtualAddress);
    for(;desc->Name;++desc){
        if(!desc->OriginalFirstThunk)continue;
        auto names=(IMAGE_THUNK_DATA64*)(base+desc->OriginalFirstThunk);auto slots=(IMAGE_THUNK_DATA64*)(base+desc->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots){
            if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
            auto imp=(IMAGE_IMPORT_BY_NAME*)(base+names->u1.AddressOfData);
            if(strcmp((char*)imp->Name,"MFCreateMPEG4MediaSink"))continue;
            DWORD old;if(!VirtualProtect(&slots->u1.Function,sizeof(void*),PAGE_READWRITE,&old))return false;
            InterlockedExchangePointer((PVOID volatile*)&slots->u1.Function,(PVOID)&RecorderCreateSink);
            DWORD unused;VirtualProtect(&slots->u1.Function,sizeof(void*),old,&unused);
            Log(L"Installed native MP4 sink hook; waiting for game's per-song recording");return true;
        }
    }return false;
}
#include "native_auto.h"
static DWORD WINAPI Worker(void*){
    Configure();if(!GetPrivateProfileIntW(L"Recorder",L"Enabled",1,ini.c_str()))return 0;
    wchar_t module[260];GetPrivateProfileStringW(L"Recorder",L"GameModule",L"bm2dx.dll",module,260,ini.c_str());
    for(int i=0;i<600;i++){HMODULE game=GetModuleHandleW(module);if(game){if(!Install(game))Log(L"Native MP4 import not found; no game modification made",E_NOINTERFACE);else NativeAuto::Install(game);return 0;}Sleep(100);}
    Log(L"Game module did not load within 60 seconds",HRESULT_FROM_WIN32(ERROR_TIMEOUT));return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);HANDLE h=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);if(h)CloseHandle(h);}return TRUE;
}
