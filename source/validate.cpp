#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <cstdio>
int wmain(int argc,wchar_t** argv){
 if(argc<2)return 2;CoInitializeEx(nullptr,COINIT_MULTITHREADED);MFStartup(MF_VERSION);
 IMFSourceReader* r=nullptr;HRESULT hr=MFCreateSourceReaderFromURL(argv[1],nullptr,&r);if(FAILED(hr)){printf("OPEN %08lx\n",(unsigned long)hr);return 1;}
 for(int stream=0;stream<2;stream++){
  IMFMediaType* t=nullptr;MFCreateMediaType(&t);t->SetGUID(MF_MT_MAJOR_TYPE,stream==0?MFMediaType_Video:MFMediaType_Audio);t->SetGUID(MF_MT_SUBTYPE,stream==0?MFVideoFormat_NV12:MFAudioFormat_PCM);
  DWORD id=stream==0?MF_SOURCE_READER_FIRST_VIDEO_STREAM:MF_SOURCE_READER_FIRST_AUDIO_STREAM;
  hr=r->SetCurrentMediaType(id,nullptr,t);t->Release();if(FAILED(hr)){printf("TYPE %d %08lx\n",stream,(unsigned long)hr);continue;}
  int count=0;LONGLONG last=0;bool eos=false;
  for(;;){DWORD actual,flags;LONGLONG ts;IMFSample* sample=nullptr;hr=r->ReadSample(id,0,&actual,&flags,&ts,&sample);if(FAILED(hr)){printf("READ %d %08lx\n",stream,(unsigned long)hr);break;}if(sample){++count;last=ts;sample->Release();}if(flags&MF_SOURCE_READERF_ENDOFSTREAM){eos=true;break;}if(count>1000000)break;}
  printf("DECODE stream=%d samples=%d last_seconds=%.3f eos=%d\n",stream,count,last/10000000.0,eos);
 }
 r->Release();MFShutdown();CoUninitialize();return 0;
}

