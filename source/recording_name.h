// Names are captured at the FIRST start, never from the result/menu state.
namespace RecordingName {
struct Metadata {SYSTEMTIME started{};int mode=-1;std::wstring detail;};
static std::wstring Date(const SYSTEMTIME& t){wchar_t s[32];swprintf(s,32,L"%04u-%02u-%02u",t.wYear,t.wMonth,t.wDay);return s;}
static std::wstring Safe(std::wstring s){
    for(auto& c:s)if(c<32||wcschr(L"<>:\"/\\|?*",c))c=L'_';
    if(s.size()>120)s.resize(120);
    if(!s.empty()&&s.back()>=0xd800&&s.back()<=0xdbff)s.pop_back();
    while(!s.empty()&&(s.back()==L'.'||s.back()==L' '))s.pop_back();
    return s;
}
static std::wstring Mode(int mode){
    switch(mode){case 0:return L"Standard";case 3:return L"段位";case 5:return L"Step Up";case 6:return L"PF";case 7:case 8:return L"Arena";default:return L"模式"+std::to_wstring(mode);}
}
static std::wstring Grade(int grade,int style){
    static const wchar_t* names[]={L"七级",L"六级",L"五级",L"四级",L"三级",L"二级",L"一级",L"初段",L"二段",L"三段",L"四段",L"五段",L"六段",L"七段",L"八段",L"九段",L"十段",L"中传",L"皆传"};
    if(grade<0||grade>=19||style<0||style>1)return L"";
    return std::wstring(style?L"DP":L"SP")+names[grade];
}
static bool Read(const void* address,void* dest,size_t bytes){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),address,dest,bytes,&got)&&got==bytes;}
static Metadata Capture(BYTE* base,bool metadataSupported){
    Metadata m;GetLocalTime(&m.started);
    if(!base)return m;
    Read(base+0xac85170,&m.mode,sizeof(m.mode));
    if(!metadataSupported)return m;
    if(m.mode==3){
        int grade=-1,style=-1;
        if(Read(base+0xaa60e10,&grade,4)&&Read(base+0xac85174,&style,4))m.detail=Grade(grade,style==0?0:style>0?1:-1);
    }else if(m.mode==0||m.mode==5||m.mode==6){
        BYTE* song=nullptr;char title[256]={};
        if(Read(base+0xac851a0,&song,sizeof(song))&&song&&Read(song+0x100,title,sizeof(title))){
            size_t n=0;while(n<sizeof(title)&&title[n])++n;
            if(n&&n<sizeof(title)){
                // Native music database strings use Windows Shift-JIS (CP932).
                int len=MultiByteToWideChar(932,MB_ERR_INVALID_CHARS,title,(int)n,nullptr,0);
                if(len>0){m.detail.resize(len);MultiByteToWideChar(932,MB_ERR_INVALID_CHARS,title,(int)n,m.detail.data(),len);}
            }
        }
    }
    m.detail=Safe(m.detail);return m;
}
static std::wstring Stem(const Metadata& m){
    std::wstring prefix=Date(m.started);
    if(filenameTime){wchar_t time[24];swprintf(time,24,L"_%02u-%02u-%02u",m.started.wHour,m.started.wMinute,m.started.wSecond);prefix+=time;}
    return prefix+L"_"+Mode(m.mode)+(m.detail.empty()?L"":L"_"+Safe(m.detail));
}
static std::wstring Directory(const Metadata& m){return output+(dateFolders?L"\\"+Date(m.started):L"");}
static std::wstring Candidate(const std::wstring& stem,unsigned index){return stem+(index==1?L"":L"_"+std::to_wstring(index))+L".mp4";}
// The partial file reserves a name across processes. Neither existing MP4s nor
// unfinished recordings are overwritten. Rename also refuses replacement.
static HANDLE Reserve(const Metadata& m,std::wstring& final,std::wstring& temp){
    auto dir=Directory(m);if(!MakeDirectories(dir))return INVALID_HANDLE_VALUE;
    auto stem=dir+L"\\"+Stem(m);
    for(unsigned i=1;i<100000;++i){
        final=Candidate(stem,i);temp=final+L".partial";
        if(GetFileAttributesW(final.c_str())!=INVALID_FILE_ATTRIBUTES)continue;
        HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f!=INVALID_HANDLE_VALUE)return f;
        DWORD error=GetLastError();if(error!=ERROR_FILE_EXISTS&&error!=ERROR_ALREADY_EXISTS)return f;
    }
    SetLastError(ERROR_FILE_EXISTS);return INVALID_HANDLE_VALUE;
}
}
