/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   18th Dec 2014
Authors     :   Tom Heath
Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/
/// A simple sample to show how to select the VR audio device. In this
/// sample the device is selected and a event-based non-exclusive WASAPI
/// session plays back a sine wave.

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

#include "OVR_CAPI_Audio.h"

#include <MMDeviceAPI.h>
#include <AudioPolicy.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <avrt.h>

struct AudioDevice : BasicVR
{
    AudioDevice(HINSTANCE hinst) : BasicVR(hinst, L"Using BasicVR") {}

    void MainLoop()
    {
		Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

		    for (int eye = 0; eye < 2; ++eye)
		    {
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

class CWASAPIRenderer;
int AudioCreateRenderer(CWASAPIRenderer **renderer);
int AudioShutDown(CWASAPIRenderer *renderer);

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    AudioDevice app(hinst);

    CWASAPIRenderer *renderer;
    int result = AudioCreateRenderer(&renderer);
    if (result != 0)
    {
        return result;
    }

    result = app.Run();
    if (result != 0)
    {
        return result;
    }
    
    return AudioShutDown(renderer);
}

class CWASAPIRenderer : IMMNotificationClient, IAudioSessionEvents
{
public:
    //  Public interface to CWASAPIRenderer.
    enum RenderSampleType
    {
        SampleTypeFloat,
        SampleType16BitPCM,
    };

    CWASAPIRenderer(IMMDevice *Device);
    bool Initialize(UINT32 EngineLatency);
    void Shutdown();
    bool Start();
    void Stop();
    WORD ChannelCount() { return _MixFormat->nChannels; }
    UINT32 SamplesPerSecond() { return _MixFormat->nSamplesPerSec; }
    UINT32 FrameSize() { return _FrameSize; }
    UINT32 BufferSizePerPeriod();
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

private:
    LONG    _RefCount;

    IMMDevice * _Device;
    IAudioClient *_AudioClient;
    IAudioRenderClient *_RenderClient;

    HANDLE      _RenderThread;
    HANDLE      _ShutdownEvent;
    HANDLE      _AudioSamplesReadyEvent;
    WAVEFORMATEX *_MixFormat;
    UINT32      _FrameSize;
    RenderSampleType _RenderSampleType;

    static DWORD __stdcall WASAPIRenderThread(LPVOID Context);
    DWORD CWASAPIRenderer::DoRenderThread();
    //
    //  Stream switch related members and methods.
    //
    HANDLE                  _StreamSwitchEvent;          // Set when the current session is disconnected or the default device changes.
    HANDLE                  _StreamSwitchCompleteEvent;  // Set when the default device changed.
    IAudioSessionControl *  _AudioSessionControl;
    IMMDeviceEnumerator *   _DeviceEnumerator;
    bool                    _InStreamSwitch;

    double                  _Theta; // Current angle for sine wave oscillator.

    STDMETHOD(OnDisplayNameChanged) (LPCWSTR /*NewDisplayName*/, LPCGUID /*EventContext*/) { return S_OK; };
    STDMETHOD(OnIconPathChanged) (LPCWSTR /*NewIconPath*/, LPCGUID /*EventContext*/) { return S_OK; };
    STDMETHOD(OnSimpleVolumeChanged) (float /*NewSimpleVolume*/, BOOL /*NewMute*/, LPCGUID /*EventContext*/) { return S_OK; }
    STDMETHOD(OnChannelVolumeChanged) (DWORD /*ChannelCount*/, float /*NewChannelVolumes*/[], DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/) { return S_OK; };
    STDMETHOD(OnGroupingParamChanged) (LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/) { return S_OK; };
    STDMETHOD(OnStateChanged) (AudioSessionState /*NewState*/) { return S_OK; };
    STDMETHOD(OnSessionDisconnected) (AudioSessionDisconnectReason DisconnectReason);
    STDMETHOD(OnDeviceStateChanged) (LPCWSTR /*DeviceId*/, DWORD /*NewState*/) { return S_OK; }
    STDMETHOD(OnDeviceAdded) (LPCWSTR /*DeviceId*/) { return S_OK; };
    STDMETHOD(OnDeviceRemoved) (LPCWSTR /*DeviceId(*/) { return S_OK; };
    STDMETHOD(OnDefaultDeviceChanged) (EDataFlow Flow, ERole Role, LPCWSTR NewDefaultDeviceId);
    STDMETHOD(OnPropertyValueChanged) (LPCWSTR /*DeviceId*/, const PROPERTYKEY /*Key*/){ return S_OK; };

    //
    //  IUnknown
    //
    STDMETHOD(QueryInterface)(REFIID iid, void **pvObject);

    bool CalculateMixFormatType();
    bool InitializeAudioEngine(UINT32 EngineLatency);
    bool LoadFormat();
};

template<typename T> T Convert(double Value);

template <typename T>
void GenerateSineSamples(BYTE *Buffer, size_t BufferLength, DWORD Frequency, WORD ChannelCount, DWORD SamplesPerSecond, double *InitialTheta)
{
    double sampleIncrement = (Frequency * (M_PI * 2)) / (double)SamplesPerSecond;
    T *dataBuffer = reinterpret_cast<T *>(Buffer);
    double theta = (InitialTheta != NULL ? *InitialTheta : 0);

    for (size_t i = 0; i < BufferLength / sizeof(T); i += ChannelCount)
    {
        const double SINE_VOLUME = 0.25f; // -12dB
        double sinValue = sin(theta);
        for (size_t j = 0; j < ChannelCount; j++)
        {
            dataBuffer[i + j] = Convert<T>(sinValue * SINE_VOLUME);
        }
        theta += sampleIncrement;
    }

    if (InitialTheta != NULL)
    {
        *InitialTheta = theta;
    }
}

template<>
float Convert<float>(double Value)
{
    return (float)(Value);
};

template<>
short Convert<short>(double Value)
{
    return (short)(Value * _I16_MAX);
};

int TargetFrequency = 440;
int TargetLatency = 30;

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

#define CHECK_HRESULT(hres, ret_val) if (!FAILED(hr)); else return ret_val

template<class T> class AutoSafeRelease
{   
public:
    AutoSafeRelease() 
        : ptr(NULL)
    {}
    
    ~AutoSafeRelease()
    { 
        SafeRelease<T>(&ptr);
    }

    T** operator &() { return &ptr; }
    T* operator ->() const { return ptr; }
    operator T*() const { return ptr; }

private:
    T* ptr;
};

#include <FunctionDiscoveryKeys_devpkey.h>

bool PickDevice(IMMDevice **DeviceToUse)
{
    HRESULT hr;
    bool retValue = true;
    AutoSafeRelease<IMMDeviceEnumerator> deviceEnumerator;
    AutoSafeRelease<IMMDeviceCollection> deviceCollection;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&deviceEnumerator));
    CHECK_HRESULT(hr, false);

    UINT deviceIndex = 0;
    ovrResult res = ovr_GetAudioDeviceOutWaveId(&deviceIndex);
    if (res != ovrSuccess)
    {
        // Fall back to default device
        deviceIndex = 0;
    }

	IMMDevice *device = NULL;
	if (deviceIndex == 0)
	{
		hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
		CHECK_HRESULT(hr, false);
	}
	else
	{
		hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
		CHECK_HRESULT(hr, false);

		hr = deviceCollection->Item(deviceIndex, &device);
		CHECK_HRESULT(hr, false);
	}
	
    *DeviceToUse = device;
    retValue = true;

    return retValue;
}

class AutoCoUninitialize
{
public:
    ~AutoCoUninitialize()
    {
        CoUninitialize();
    }
};

int AudioCreateRenderer(CWASAPIRenderer **renderer)
{
    AutoSafeRelease<IMMDevice> device;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CHECK_HRESULT(hr, hr);

    AutoCoUninitialize acu;

    if (!PickDevice(&device))
    {
        return -1;
    }

    CWASAPIRenderer *ren = new (std::nothrow) CWASAPIRenderer(device);
    if (ren == NULL)
    {
        return -1;
    }

    if (!ren->Initialize(TargetLatency))
    {
        return -1;
    }

    if (!ren->Start())
    {
        return -1;
    }

    *renderer = ren;

    return 0;
}

int AudioShutDown(CWASAPIRenderer *renderer)
{
    renderer->Shutdown();
    SafeRelease(&renderer);

    return 0;
}

CWASAPIRenderer::CWASAPIRenderer(IMMDevice *Device) :
    _RefCount(1),
    _Device(Device),
    _AudioClient(NULL),
    _RenderClient(NULL),
    _RenderThread(NULL),
    _ShutdownEvent(NULL),
    _MixFormat(NULL),
    _AudioSamplesReadyEvent(NULL),
    _StreamSwitchEvent(NULL),
    _StreamSwitchCompleteEvent(NULL),
    _AudioSessionControl(NULL),
    _DeviceEnumerator(NULL),
    _InStreamSwitch(false),
    _Theta(0.0)
{
    _Device->AddRef();    // Since we're holding a copy of the endpoint, take a reference to it.  It'll be released in Shutdown();
}

//
//  Initialize WASAPI in event driven mode.
//
bool CWASAPIRenderer::InitializeAudioEngine(UINT32 EngineLatency)
{
    HRESULT hr = _AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        EngineLatency * 10000,
        0,
        _MixFormat,
        NULL);

    CHECK_HRESULT(hr, false);

    //
    //  Retrieve the buffer size for the audio client.
    //
    hr = _AudioClient->SetEventHandle(_AudioSamplesReadyEvent);
    CHECK_HRESULT(hr, false);

    hr = _AudioClient->GetService(IID_PPV_ARGS(&_RenderClient));
    CHECK_HRESULT(hr, false);

    return true;
}

//
//  The Event Driven renderer will be woken up every defaultDevicePeriod hundred-nano-seconds.
//  Convert that time into a number of frames.
//
UINT32 CWASAPIRenderer::BufferSizePerPeriod()
{
    REFERENCE_TIME defaultDevicePeriod, minimumDevicePeriod;
    HRESULT hr = _AudioClient->GetDevicePeriod(&defaultDevicePeriod, &minimumDevicePeriod);
    CHECK_HRESULT(hr, 0);
    double devicePeriodInSeconds = defaultDevicePeriod / (10000.0*1000.0);
    return static_cast<UINT32>(_MixFormat->nSamplesPerSec * devicePeriodInSeconds + 0.5);
}

//
//  Retrieve the format we'll use to render samples.
//
//  We use the Mix format since we're rendering in shared mode.
//
bool CWASAPIRenderer::LoadFormat()
{
    HRESULT hr = _AudioClient->GetMixFormat(&_MixFormat);
    CHECK_HRESULT(hr, false);

    _FrameSize = _MixFormat->nBlockAlign;
    if (!CalculateMixFormatType())
    {
        return false;
    }
    return true;
}

//
//  Crack open the mix format and determine what kind of samples are being rendered.
//
bool CWASAPIRenderer::CalculateMixFormatType()
{
    if (_MixFormat->wFormatTag == WAVE_FORMAT_PCM ||
        _MixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        reinterpret_cast<WAVEFORMATEXTENSIBLE *>(_MixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
    {
        if (_MixFormat->wBitsPerSample == 16)
        {
            _RenderSampleType = SampleType16BitPCM;
        }
        else
        {
            return false;
        }
    }
    else if (_MixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        (_MixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        reinterpret_cast<WAVEFORMATEXTENSIBLE *>(_MixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
    {
        _RenderSampleType = SampleTypeFloat;
    }
    else
    {
        return false;
    }
    return true;
}
//
//  Initialize the renderer.
//
bool CWASAPIRenderer::Initialize(UINT32 EngineLatency)
{
    if (EngineLatency < 30)
    {
        // Engine latency in shared mode event driven cannot be less than 30ms
        return false;
    }

    //
    //  Create our shutdown and samples ready events- we want auto reset events that start in the not-signaled state.
    //
    _ShutdownEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_ShutdownEvent == NULL)
    {
        return false;
    }

    _AudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_AudioSamplesReadyEvent == NULL)
    {
        return false;
    }

    //
    //  Create our stream switch event- we want auto reset events that start in the not-signaled state.
    //  Note that we create this event even if we're not going to stream switch - that's because the event is used
    //  in the main loop of the renderer and thus it has to be set.
    //
    _StreamSwitchEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_StreamSwitchEvent == NULL)
    {
        return false;
    }

    //
    //  Now activate an IAudioClient object on our preferred endpoint and retrieve the mix format for that endpoint.
    //
    HRESULT hr = _Device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&_AudioClient));
    CHECK_HRESULT(hr, false);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_DeviceEnumerator));
    CHECK_HRESULT(hr, false);

    //
    // Load the MixFormat.  This may differ depending on the shared mode used
    //
    if (!LoadFormat())
    {
        return false;
    }

    if (!InitializeAudioEngine(EngineLatency))
    {
        return false;
    }

    return true;
}

//
//  Shut down the render code and free all the resources.
//
void CWASAPIRenderer::Shutdown()
{
    if (_RenderThread)
    {
        SetEvent(_ShutdownEvent);
        WaitForSingleObject(_RenderThread, INFINITE);
        CloseHandle(_RenderThread);
        _RenderThread = NULL;
    }

    if (_ShutdownEvent)
    {
        CloseHandle(_ShutdownEvent);
        _ShutdownEvent = NULL;
    }
    if (_AudioSamplesReadyEvent)
    {
        CloseHandle(_AudioSamplesReadyEvent);
        _AudioSamplesReadyEvent = NULL;
    }
    if (_StreamSwitchEvent)
    {
        CloseHandle(_StreamSwitchEvent);
        _StreamSwitchEvent = NULL;
    }

    SafeRelease(&_Device);
    SafeRelease(&_AudioClient);
    SafeRelease(&_RenderClient);

    if (_MixFormat)
    {
        CoTaskMemFree(_MixFormat);
        _MixFormat = NULL;
    }
}

bool CWASAPIRenderer::Start()
{
    HRESULT hr;

    BYTE *pData;

    UINT32 renderBufferSizeInBytes = BufferSizePerPeriod() * FrameSize();
    DWORD bufferLengthInFrames = renderBufferSizeInBytes / _FrameSize;
    hr = _RenderClient->GetBuffer(bufferLengthInFrames, &pData);
    CHECK_HRESULT(hr, false);

    switch (_RenderSampleType)
    {
    case CWASAPIRenderer::SampleTypeFloat:
        GenerateSineSamples<float>(pData, renderBufferSizeInBytes, TargetFrequency,
            ChannelCount(), SamplesPerSecond(), &_Theta);
        break;
    case CWASAPIRenderer::SampleType16BitPCM:
        GenerateSineSamples<short>(pData, renderBufferSizeInBytes, TargetFrequency,
            ChannelCount(), SamplesPerSecond(), &_Theta);
        break;
    }

    hr = _RenderClient->ReleaseBuffer(bufferLengthInFrames, 0);
    CHECK_HRESULT(hr, false);

    _RenderThread = CreateThread(NULL, 0, WASAPIRenderThread, this, 0, NULL);
    if (_RenderThread == NULL)
    {
        return false;
    }

    hr = _AudioClient->Start();
    CHECK_HRESULT(hr, false);

    return true;
}

#define RETURN_VOID

void CWASAPIRenderer::Stop()
{
    HRESULT hr;

    if (_ShutdownEvent)
    {
        SetEvent(_ShutdownEvent);
    }

    hr = _AudioClient->Stop();
    CHECK_HRESULT(hr, RETURN_VOID);

    if (_RenderThread)
    {
        WaitForSingleObject(_RenderThread, INFINITE);

        CloseHandle(_RenderThread);
        _RenderThread = NULL;
    }
}

DWORD CWASAPIRenderer::WASAPIRenderThread(LPVOID Context)
{
    CWASAPIRenderer *renderer = static_cast<CWASAPIRenderer *>(Context);
    return renderer->DoRenderThread();
}

DWORD CWASAPIRenderer::DoRenderThread()
{
    bool stillPlaying = true;
    HANDLE waitArray[3] = { _ShutdownEvent, _StreamSwitchEvent, _AudioSamplesReadyEvent };
    HANDLE mmcssHandle = NULL;
    DWORD mmcssTaskIndex = 0;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CHECK_HRESULT(hr, hr);

    mmcssHandle = AvSetMmThreadCharacteristicsW(L"Audio", &mmcssTaskIndex);
    if (mmcssHandle == NULL)
    {
        // Unable to enable MMCSS on render thread
    }

    while (stillPlaying)
    {
        HRESULT hr;
        DWORD waitResult = WaitForMultipleObjects(3, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:     // _ShutdownEvent
            stillPlaying = false;       // We're done, exit the loop.
            break;
        case WAIT_OBJECT_0 + 1:     // _StreamSwitchEvent
            stillPlaying = false;       // We're done, exit the loop.
            break;
        case WAIT_OBJECT_0 + 2:     // _AudioSamplesReadyEvent

            BYTE *pData;
            {
                UINT32 renderBufferSizeInBytes = BufferSizePerPeriod() * FrameSize();
                DWORD bufferLengthInFrames = renderBufferSizeInBytes / _FrameSize;
                hr = _RenderClient->GetBuffer(bufferLengthInFrames, &pData);
                if (SUCCEEDED(hr))
                {
                    switch (_RenderSampleType)
                    {
                    case CWASAPIRenderer::SampleTypeFloat:
                        GenerateSineSamples<float>(pData, renderBufferSizeInBytes, TargetFrequency,
                            ChannelCount(), SamplesPerSecond(), &_Theta);
                        break;
                    case CWASAPIRenderer::SampleType16BitPCM:
                        GenerateSineSamples<short>(pData, renderBufferSizeInBytes, TargetFrequency,
                            ChannelCount(), SamplesPerSecond(), &_Theta);
                        break;
                    }

                    hr = _RenderClient->ReleaseBuffer(bufferLengthInFrames, 0);
                    if (!SUCCEEDED(hr))
                    {
                        stillPlaying = false;
                    }
                }
                else
                {
                    stillPlaying = false;
                }
            }
            break;
        }
    }

    AvRevertMmThreadCharacteristics(mmcssHandle);

    CoUninitialize();
    return 0;
}


//
//  Called when an audio session is disconnected.  
//
//  When a session is disconnected because of a device removal or format change event, we just want 
//  to let the render thread know that the session's gone away
//
HRESULT CWASAPIRenderer::OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason)
{
    UNREFERENCED_PARAMETER(DisconnectReason);

    // TODO: test this, shut down renderer?
    return S_OK;
}

//
//  IUnknown
//
HRESULT CWASAPIRenderer::QueryInterface(REFIID Iid, void **Object)
{
    if (Object == NULL)
    {
        return E_POINTER;
    }
    *Object = NULL;

    if (Iid == IID_IUnknown)
    {
        *Object = static_cast<IUnknown *>(static_cast<IAudioSessionEvents *>(this));
        AddRef();
    }
    else if (Iid == __uuidof(IMMNotificationClient))
    {
        *Object = static_cast<IMMNotificationClient *>(this);
        AddRef();
    }
    else if (Iid == __uuidof(IAudioSessionEvents))
    {
        *Object = static_cast<IAudioSessionEvents *>(this);
        AddRef();
    }
    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

ULONG CWASAPIRenderer::AddRef()
{
    return InterlockedIncrement(&_RefCount);
}

ULONG CWASAPIRenderer::Release()
{
    ULONG returnValue = InterlockedDecrement(&_RefCount);
    if (returnValue == 0)
    {
        delete this;
    }
    return returnValue;
}

HRESULT CWASAPIRenderer::OnDefaultDeviceChanged(EDataFlow Flow, ERole Role, LPCWSTR /*NewDefaultDeviceId*/)
{
    UNREFERENCED_PARAMETER(Flow);
    UNREFERENCED_PARAMETER(Role);

    return S_OK;
}