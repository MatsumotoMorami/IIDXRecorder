#ifdef NDEBUG
#undef NDEBUG
#endif
#include "recorder.cpp"
#include <cassert>
int main(){
    Configure();
    output=directory+L"\\naming-test-"+std::to_wstring(GetCurrentProcessId());
    RecordingName::Metadata m;m.started.wYear=2026;m.started.wMonth=9;m.started.wDay=15;m.mode=3;m.detail=L"SP九段";
    assert(RecordingName::Stem(m)==L"2026-09-15_段位_SP九段");
    assert(RecordingName::Grade(15,0)==L"SP九段"&&RecordingName::Grade(18,1)==L"DP皆传");
    assert(RecordingName::Grade(-1,0).empty()&&RecordingName::Grade(19,0).empty());
    assert(RecordingName::Safe(L"A/B:C*D?E\"F<G>H|I\\J. ")==L"A_B_C_D_E_F_G_H_I_J");
    dateFolders=false;assert(RecordingName::Directory(m)==output);
    dateFolders=true;assert(RecordingName::Directory(m)==output+L"\\2026-09-15");
    std::wstring final,partial;HANDLE f=RecordingName::Reserve(m,final,partial);assert(f!=INVALID_HANDLE_VALUE);
    CloseHandle(f);assert(MoveFileExW(partial.c_str(),final.c_str(),0));auto first=final;
    f=RecordingName::Reserve(m,final,partial);assert(f!=INVALID_HANDLE_VALUE);CloseHandle(f);
    assert(final==RecordingName::Directory(m)+L"\\2026-09-15_段位_SP九段_2.mp4");
    std::wstring third,temp3;f=RecordingName::Reserve(m,third,temp3);assert(f!=INVALID_HANDLE_VALUE);CloseHandle(f);
    assert(third==RecordingName::Directory(m)+L"\\2026-09-15_段位_SP九段_3.mp4");
    assert(GetFileAttributesW(first.c_str())!=INVALID_FILE_ATTRIBUTES);
    auto b=(BYTE*)VirtualAlloc(nullptr,0xbb3e000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(b);
    *(int*)(b+0xac85170)=3;*(int*)(b+0xaa60e10)=15;*(int*)(b+0xac85174)=0;
    auto captured=RecordingName::Capture(b,true);assert(captured.detail==L"SP九段");
    *(int*)(b+0xac85174)=1;assert(RecordingName::Capture(b,true).detail==L"DP九段");
    *(int*)(b+0xaa60e10)=99;assert(RecordingName::Capture(b,true).detail.empty());
    BYTE song[0x300]={};*(void**)(b+0xac851a0)=song;
    const wchar_t* title=L"冥 / テスト";
    assert(WideCharToMultiByte(932,0,title,-1,(char*)song+0x100,256,nullptr,nullptr)>0);
    for(int mode:{0,5,6}){*(int*)(b+0xac85170)=mode;assert(RecordingName::Capture(b,true).detail==L"冥 _ テスト");}
    *(int*)(b+0xac85170)=7;auto arena=RecordingName::Capture(b,true);assert(arena.detail.empty()&&RecordingName::Stem(arena).find(L"_Arena")!=std::wstring::npos);
    *(int*)(b+0xac85170)=0;*(void**)(b+0xac851a0)=(void*)1;assert(RecordingName::Capture(b,true).detail.empty());
    assert(RecordingName::Capture(b,false).detail.empty());
    // Later mode/title/date changes cannot alter an already captured name.
    assert(captured.mode==3&&captured.detail==L"SP九段");
    VirtualFree(b,0,MEM_RELEASE);
    puts("PASS: Chinese/Japanese metadata, Dan/SP/DP mapping, root/date directories, collision and partial reservation, illegal characters, Arena omission, missing metadata and immutable snapshot");
}
