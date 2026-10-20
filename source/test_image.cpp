#ifdef NDEBUG
#undef NDEBUG
#endif
#include "recorder.cpp"
#include <assert.h>
#include "../../IIDXLive/audio-layout.h"
int main(int argc,char** argv){
    assert(argc==2);HMODULE m=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(m);Configure();
    assert(NativeAuto::Install(m));assert(NativeAuto::metadataSupported);assert(NativeAuto::sessionsInstalled);
    AudioLayout::Layout live;assert(AudioLayout::Discover((BYTE*)m,live));
    printf("PASS: real PE image recorder eligibility, vtables, scene hooks and metadata; updated=%d latest=%d current=%d\n",GameBuild::updated,GameBuild::latest,GameBuild::current);
    return 0;
}
