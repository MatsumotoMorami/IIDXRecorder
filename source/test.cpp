#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <cstdio>
#include <cmath>
#include <cstdlib>
static void Check(HRESULT hr,const char* where){if(FAILED(hr)){printf("FAIL %s %08lx\n",where,(unsigned long)hr);fflush(stdout);ExitProcess(1);}}
static IMFSample* Sample(DWORD bytes,LONGLONG time,LONGLONG duration,BYTE** ptr,IMFMediaBuffer** buf){
    IMFSample* s=nullptr;Check(MFCreateSample(&s),"sample");Check(MFCreateMemoryBuffer(bytes,buf),"buffer");
    Check((*buf)->Lock(ptr,nullptr,nullptr),"lock");Check((*buf)->SetCurrentLength(bytes),"length");
    s->AddBuffer(*buf);s->SetSampleTime(time);s->SetSampleDuration(duration);return s;
}
int main(int argc,char** argv){
    const int frames=argc>1?atoi(argv[1]):360;const bool sound=argc<3||atoi(argv[2])!=0;
    Check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM");Check(MFStartup(MF_VERSION),"MF startup");
    if(!LoadLibraryW(L"IIDXRecorder.dll")){printf("load DLL failed %lu\n",GetLastError());return 1;}Sleep(500);
    IMFMediaType *v=nullptr,*a=nullptr,*input=nullptr;IMFByteStream* bs=nullptr;IMFMediaSink* sink=nullptr;IMFSinkWriter* writer=nullptr;
    MFCreateMediaType(&v);v->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);v->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264);
    v->SetUINT32(MF_MT_AVG_BITRATE,1500000);v->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);
    MFSetAttributeSize(v,MF_MT_FRAME_SIZE,640,360);MFSetAttributeRatio(v,MF_MT_FRAME_RATE,30,1);MFSetAttributeRatio(v,MF_MT_PIXEL_ASPECT_RATIO,1,1);
    if(sound){MFCreateMediaType(&a);a->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);a->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_AAC);a->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,2);a->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,48000);a->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16);a->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,24000);a->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE,0);}
    Check(MFCreateFile(MF_ACCESSMODE_READWRITE,MF_OPENMODE_DELETE_IF_EXIST,MF_FILEFLAGS_NONE,L"original.mp4",&bs),"file");
    Check(MFCreateMPEG4MediaSink(bs,v,a,&sink),"create sink (IAT hook)");
    Check(MFCreateSinkWriterFromMediaSink(sink,nullptr,&writer),"sink writer");
    MFCreateMediaType(&input);input->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);input->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32);
    input->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);MFSetAttributeSize(input,MF_MT_FRAME_SIZE,640,360);MFSetAttributeRatio(input,MF_MT_FRAME_RATE,30,1);MFSetAttributeRatio(input,MF_MT_PIXEL_ASPECT_RATIO,1,1);
    Check(writer->SetInputMediaType(0,input,nullptr),"video input");input->Release();input=nullptr;
    if(sound){MFCreateMediaType(&input);input->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);input->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM);input->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,2);input->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,48000);input->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16);input->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,4);input->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,192000);Check(writer->SetInputMediaType(1,input,nullptr),"audio input");input->Release();}
    Check(writer->BeginWriting(),"begin writing");printf("READY %lu\n",GetCurrentProcessId());fflush(stdout);
    ULONGLONG start=GetTickCount64();
    for(int f=0;f<frames;f++){
        BYTE* p;IMFMediaBuffer* b;LONGLONG time=(LONGLONG)f*10000000/30,next=(LONGLONG)(f+1)*10000000/30;
        auto s=Sample(640*360*4,time,next-time,&p,&b);
        for(int y=0;y<360;y++)for(int x=0;x<640;x++){int i=(y*640+x)*4;p[i]=(BYTE)(x+f*3);p[i+1]=(BYTE)(y+f);p[i+2]=(BYTE)(f*7);p[i+3]=255;}
        b->Unlock();Check(writer->WriteSample(0,s),"video sample");s->Release();b->Release();
        if(sound){s=Sample(1600*4,time,next-time,&p,&b);auto pcm=(short*)p;for(int j=0;j<1600;j++){short value=(short)(8000*sin((f*1600.0+j)*440.0*6.28318530718/48000));pcm[j*2]=value;pcm[j*2+1]=value;}b->Unlock();Check(writer->WriteSample(1,s),"audio sample");s->Release();b->Release();}
        auto target=start+(ULONGLONG)(f+1)*1000/30;auto now=GetTickCount64();if(target>now)Sleep((DWORD)(target-now));
    }
    Check(writer->Finalize(),"finalize");writer->Release();sink->Shutdown();sink->Release();bs->Close();bs->Release();v->Release();if(a)a->Release();MFShutdown();CoUninitialize();puts("PASS");return 0;
}
