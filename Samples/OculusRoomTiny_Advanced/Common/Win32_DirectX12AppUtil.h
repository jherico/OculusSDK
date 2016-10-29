
/************************************************************************************
Filename    :   Win32_DirectX12AppUtil.h
Content     :   D3D12 application/Window setup functionality for RoomTiny
Created     :   10/28/2015

Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
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
#ifndef OVR_Win32_DirectXAppUtil_h
#define OVR_Win32_DirectXAppUtil_h

#include <cstdint>
#include <vector>
#include <d3dcompiler.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <new>
#include <stdio.h>
#include "DirectXMath.h"
using namespace DirectX;

#include "Win32_d3dx12.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#ifndef VALIDATE
    #define VALIDATE(x, msg) if (!(x)) { MessageBoxA(NULL, (msg), "OculusRoomTiny", MB_ICONERROR | MB_OK); exit(-1); }
#endif

#ifndef FATALERROR
#define FATALERROR(msg) { MessageBoxA(NULL, (msg), "OculusRoomTiny", MB_ICONERROR | MB_OK); exit(-1); }
#endif

// clean up member COM pointers
template<typename T> void Release(T *&obj)
{
    if (!obj) return;
    obj->Release();
    obj = nullptr;
}

//------------------------------------------------------------
struct DepthBuffer
{
    ID3D12Resource*             TextureRes;
    D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle;

    DepthBuffer(ID3D12Device * Device, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, int sizeW, int sizeH, int sampleCount = 1)
    {
        D3D12_RESOURCE_DESC dsDesc = {};
        dsDesc.Width = sizeW;
        dsDesc.Height = sizeH;
        dsDesc.MipLevels = 1;
        dsDesc.DepthOrArraySize = 1;
        dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dsDesc.SampleDesc.Count = sampleCount;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_CLEAR_VALUE clearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

        HRESULT hr = Device->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &dsDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&TextureRes));
        VALIDATE((hr == ERROR_SUCCESS), "CreateCommittedResource failed");

        DsvHandle = dsvHandle;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
        Device->CreateDepthStencilView(TextureRes, &dsvDesc, DsvHandle);
    }
    ~DepthBuffer()
    {
        Release(TextureRes);
    }
};

////----------------------------------------------------------------
struct DataBuffer
{
    ID3D12Resource*  D3DBuffer;
    size_t           BufferSize;

    DataBuffer(ID3D12Device * Device, const void* rawData, size_t bufferSize) : BufferSize(bufferSize)
    {
        CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC buf = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);

        HRESULT hr = Device->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &buf,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&D3DBuffer));
        VALIDATE((hr == ERROR_SUCCESS), "CreateCommittedResource failed");

        // Copy the triangle data to the vertex buffer.
        UINT8* pBufferHead;
        hr = D3DBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pBufferHead));
        VALIDATE((hr == ERROR_SUCCESS), "Vertex buffer map failed");
        memcpy(pBufferHead, rawData, bufferSize);
        D3DBuffer->Unmap(0, nullptr);
    }
    ~DataBuffer()
    {
        Release(D3DBuffer);
    }
};

////----------------------------------------------------------------
struct DescHandleProvider
{
    ID3D12DescriptorHeap*         DescHeap;
    CD3DX12_CPU_DESCRIPTOR_HANDLE NextAvailableCpuHandle;
    UINT IncrementSize;
    UINT CurrentHandleCount;
    UINT MaxHandleCount;
    DescHandleProvider() {};
    DescHandleProvider(ID3D12DescriptorHeap* descHeap, UINT incrementSize, UINT handleCount)
        : DescHeap(descHeap), IncrementSize(incrementSize)
        , MaxHandleCount(handleCount), CurrentHandleCount(0)
    {
        VALIDATE((descHeap), "NULL heap provided");
        NextAvailableCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descHeap->GetCPUDescriptorHandleForHeapStart());
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE AllocCpuHandle()
    {
        VALIDATE((CurrentHandleCount < MaxHandleCount), "Hit maximum number of handles available");
        CD3DX12_CPU_DESCRIPTOR_HANDLE newHandle = NextAvailableCpuHandle;
        NextAvailableCpuHandle.Offset(IncrementSize);
        CurrentHandleCount++;
        return newHandle;
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandleFromCpuHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
    {
        int offset = (int)(cpuHandle.ptr - DescHeap->GetCPUDescriptorHandleForHeapStart().ptr);
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(DescHeap->GetGPUDescriptorHandleForHeapStart(), offset);
    }
};

enum DrawContext
{
    DrawContext_EyeRenderLeft = 0,
    DrawContext_EyeRenderRight,
    DrawContext_Final,

    DrawContext_Count,
};

//---------------------------------------------------------------------
struct DirectX12
{
	HWND                        Window;
	bool                        Running;
	bool                        Key[256];
	int                         WinSizeW;
	int                         WinSizeH;
    ID3D12Debug*                DebugController;
	ID3D12Device*               Device;
    ID3D12CommandQueue*         CommandQueue;
    DepthBuffer               * MainDepthBuffer;
    D3D12_RECT                  ScissorRect;

    HINSTANCE                   hInstance;

    ID3D12DescriptorHeap*       RtvHeap;
    ID3D12DescriptorHeap*       DsvHeap;
    ID3D12DescriptorHeap*       CbvSrvHeap;

    DescHandleProvider          RtvHandleProvider;
    DescHandleProvider          DsvHandleProvider;
    DescHandleProvider          CbvSrvHandleProvider;
    
    IDXGISwapChain3*            SwapChain;
    static const int            SwapChainNumFrames = 4;
    UINT                        SwapChainFrameIndex;

    UINT                        ActiveEyeIndex;
    DrawContext                 ActiveContext;

    // per-swap-chain-frame resources
    struct SwapChainFrameResources
    {
        ID3D12CommandAllocator*         CommandAllocators[DrawContext_Count];
        ID3D12GraphicsCommandList*      CommandLists[DrawContext_Count];
        bool                            CommandListSubmitted[DrawContext_Count];

        ID3D12Resource*                 SwapChainBuffer;
        CD3DX12_CPU_DESCRIPTOR_HANDLE   SwapChainRtvHandle;

        // Synchronization objects.
        HANDLE                          PresentFenceEvent;
        ID3D12Fence*                    PresentFenceRes;
        UINT64                          PresentFenceValue;
        UINT64                          PresentFenceWaitValue;
    };
    SwapChainFrameResources             PerFrameResources[SwapChainNumFrames];

	static LRESULT CALLBACK WindowProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
	{
        auto p = reinterpret_cast<DirectX12 *>(GetWindowLongPtr(hWnd, 0));
        switch (Msg)
        {
        case WM_KEYDOWN:
            p->Key[wParam] = true;
            break;
        case WM_KEYUP:
            p->Key[wParam] = false;
            break;
        case WM_DESTROY:
            p->Running = false;
            break;
        default:
            return DefWindowProcW(hWnd, Msg, wParam, lParam);
        }
        if ((p->Key['Q'] && p->Key[VK_CONTROL]) || p->Key[VK_ESCAPE])
        {
            p->Running = false;
        }
        return 0;
	}

    DirectX12() :
        Window(nullptr),
        Running(false),
        WinSizeW(0),
        WinSizeH(0),
        Device(nullptr),
        SwapChain(nullptr),
        MainDepthBuffer(nullptr),
        hInstance(nullptr),
        ActiveContext(DrawContext_Count),   // require init by app
        ActiveEyeIndex(UINT(-1))            // require init by app
    {
		// Clear input
		for (int i = 0; i < _countof(Key); ++i)
            Key[i] = false;
    }

    ~DirectX12()
	{
        ReleaseDevice();
        CloseWindow();
    }

    bool InitWindow(HINSTANCE hinst, LPCWSTR title)
	{
        hInstance = hinst;
		Running = true;

		WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
		wc.lpszClassName = L"App";
		wc.style = CS_OWNDC;
		wc.lpfnWndProc = WindowProc;
		wc.cbWndExtra = sizeof(this);
		RegisterClassW(&wc);

        // adjust the window size and show at InitDevice time
		Window = CreateWindowW(wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, 0, 0, hinst, 0);
        if (!Window) return false;

		SetWindowLongPtr(Window, 0, LONG_PTR(this));

        return true;
	}

    void CloseWindow()
    {
        if (Window)
        {
	        DestroyWindow(Window);
	        Window = nullptr;
	        UnregisterClassW(L"App", hInstance);
        }
    }

    bool InitDevice(int vpW, int vpH, const LUID* pLuid, bool windowed = true)
	{
		WinSizeW = vpW;
		WinSizeH = vpH;

        ScissorRect.right = static_cast<LONG>(WinSizeW);
        ScissorRect.bottom = static_cast<LONG>(WinSizeH);

		RECT size = { 0, 0, vpW, vpH };
		AdjustWindowRect(&size, WS_OVERLAPPEDWINDOW, false);
        const UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW;
        if (!SetWindowPos(Window, nullptr, 0, 0, size.right - size.left, size.bottom - size.top, flags))
            return false;

		IDXGIFactory4 * DXGIFactory = nullptr;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory));
        VALIDATE((hr == ERROR_SUCCESS), "CreateDXGIFactory1 failed");

		IDXGIAdapter * Adapter = nullptr;
        for (UINT iAdapter = 0; DXGIFactory->EnumAdapters(iAdapter, &Adapter) != DXGI_ERROR_NOT_FOUND; ++iAdapter)
        {
            DXGI_ADAPTER_DESC adapterDesc;
            Adapter->GetDesc(&adapterDesc);
            if ((pLuid == nullptr) || memcmp(&adapterDesc.AdapterLuid, pLuid, sizeof(LUID)) == 0)
                break;
            Release(Adapter);
        }


#ifdef _DEBUG
        // Enable the D3D12 debug layer.
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController))))
        {
            DebugController->EnableDebugLayer();
        }
#endif

        hr = D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&Device));
        VALIDATE((hr == ERROR_SUCCESS), "D3D12CreateDevice failed");
        Release(Adapter);

        //{
        //    // Set max frame latency to 1
        //    IDXGIDevice1* DXGIDevice1 = nullptr;
        //    hr = Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&DXGIDevice1);
        //    VALIDATE((hr == ERROR_SUCCESS), "QueryInterface failed");
        //    DXGIDevice1->SetMaximumFrameLatency(1);
        //    Release(DXGIDevice1);
        //}

        // Describe and create the command queue.
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        hr = Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CommandQueue));
        VALIDATE((hr == ERROR_SUCCESS), "CreateCommandQueue failed");
                
		// Create swap chain
        DXGI_SWAP_CHAIN_DESC scDesc = {};
        scDesc.BufferCount = SwapChainNumFrames;
		scDesc.BufferDesc.Width = WinSizeW;
		scDesc.BufferDesc.Height = WinSizeH;
		scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		//scDesc.BufferDesc.RefreshRate.Denominator = 1;
		scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scDesc.OutputWindow = Window;
		scDesc.SampleDesc.Count = 1;
		scDesc.Windowed = windowed;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        IDXGISwapChain* swapChainBase;
        hr = DXGIFactory->CreateSwapChain(CommandQueue, &scDesc, &swapChainBase);
        VALIDATE((hr == ERROR_SUCCESS), "CreateSwapChain failed");
        SwapChain = (IDXGISwapChain3*)swapChainBase;

        // This sample does not support fullscreen transitions.
        hr = DXGIFactory->MakeWindowAssociation(Window, DXGI_MWA_NO_ALT_ENTER);
        VALIDATE((hr == ERROR_SUCCESS), "MakeWindowAssociation failed");
        Release(DXGIFactory);

        SwapChainFrameIndex = SwapChain->GetCurrentBackBufferIndex();

        // Create descriptor heaps.
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.NumDescriptors = SwapChainNumFrames * 10;
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&RtvHeap));
            VALIDATE((hr == ERROR_SUCCESS), "CreateDescriptorHeap failed");

            RtvHandleProvider = DescHandleProvider(
                RtvHeap,
                Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
                rtvHeapDesc.NumDescriptors);
        }
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
            dsvHeapDesc.NumDescriptors = SwapChainNumFrames * 10;
            dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&DsvHeap));
            VALIDATE((hr == ERROR_SUCCESS), "CreateDescriptorHeap failed");

            DsvHandleProvider = DescHandleProvider(
                DsvHeap,
                Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
                dsvHeapDesc.NumDescriptors);
        }
        {
            UINT maxNumCbvSrvHandles = 100;
            D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
            cbvSrvHeapDesc.NumDescriptors = maxNumCbvSrvHandles * 10;
            cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = Device->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&CbvSrvHeap));
            VALIDATE((hr == ERROR_SUCCESS), "CreateDescriptorHeap failed");

            CbvSrvHandleProvider = DescHandleProvider(
                CbvSrvHeap,
                Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
                cbvSrvHeapDesc.NumDescriptors);
        }

        // Create frame resources.
        for (int frameIdx = 0; frameIdx < SwapChainNumFrames; frameIdx++)
        {
            SwapChainFrameResources& frameRes = PerFrameResources[frameIdx];

            // Create a RTV for buffer in swap chain
            {
                frameRes.SwapChainRtvHandle = RtvHandleProvider.AllocCpuHandle();

                hr = SwapChain->GetBuffer(frameIdx, IID_PPV_ARGS(&frameRes.SwapChainBuffer));
                VALIDATE((hr == ERROR_SUCCESS), "SwapChain GetBuffer failed");

                Device->CreateRenderTargetView(frameRes.SwapChainBuffer, nullptr, frameRes.SwapChainRtvHandle);
            }

            for (int contextIdx = 0; contextIdx < DrawContext_Count; contextIdx++)
            {
                Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frameRes.CommandAllocators[contextIdx]));
            }

            // Create an event handle to use for frame synchronization.
            frameRes.PresentFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            hr = HRESULT_FROM_WIN32(GetLastError());
            VALIDATE((hr == ERROR_SUCCESS), "CreateEvent failed");

            hr = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameRes.PresentFenceRes));
            VALIDATE((hr == ERROR_SUCCESS), "CreateFence failed");

            frameRes.PresentFenceWaitValue = UINT64(-1);

            // Create the command lists
            for (int contextIdx = 0; contextIdx < DrawContext_Count; contextIdx++)
            {
                hr = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frameRes.CommandAllocators[contextIdx],
                    nullptr, IID_PPV_ARGS(&frameRes.CommandLists[contextIdx]));
                VALIDATE((hr == ERROR_SUCCESS), "CreateCommandList failed");
                frameRes.CommandLists[contextIdx]->Close();

                frameRes.CommandListSubmitted[contextIdx] = true;   // to make sure we reset it properly first time thru
            }
        }

		// Main depth buffer
        D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle = DsvHandleProvider.AllocCpuHandle();
        MainDepthBuffer = new DepthBuffer(Device, DsvHandle, WinSizeW, WinSizeH);

        return true;
	}

    SwapChainFrameResources& CurrentFrameResources()
    {
        return PerFrameResources[SwapChainFrameIndex];
    }

    void SetActiveContext(DrawContext context)
    {
        ActiveContext = context;
    }

    void SetActiveEye(int eye)
    {
        ActiveEyeIndex = eye;
    }

    void SetAndClearRenderTarget(const D3D12_CPU_DESCRIPTOR_HANDLE* rendertarget, const D3D12_CPU_DESCRIPTOR_HANDLE* depthbuffer, float R = 0, float G = 0, float B = 0, float A = 1)
    {
        float black[] = { R, G, B, A }; // Important that alpha=0, if want pixels to be transparent, for manual layers
        CurrentFrameResources().CommandLists[ActiveContext]->OMSetRenderTargets(1, rendertarget, false, depthbuffer);
        CurrentFrameResources().CommandLists[ActiveContext]->ClearRenderTargetView(*rendertarget, black, 0, nullptr);
        if (depthbuffer)
            CurrentFrameResources().CommandLists[ActiveContext]->ClearDepthStencilView(*depthbuffer, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    void SetAndClearRenderTarget(const D3D12_CPU_DESCRIPTOR_HANDLE& rendertarget, const D3D12_CPU_DESCRIPTOR_HANDLE& depthbuffer, float R = 0, float G = 0, float B = 0, float A = 1)
    {
        SetAndClearRenderTarget(&rendertarget, &depthbuffer, R, G, B, A);
    }

    void SetViewport(float vpX, float vpY, float vpW, float vpH)
    {
        D3D12_VIEWPORT D3Dvp;
        D3Dvp.Width = vpW;    D3Dvp.Height = vpH;
        D3Dvp.MinDepth = 0;   D3Dvp.MaxDepth = 1;
        D3Dvp.TopLeftX = vpX; D3Dvp.TopLeftY = vpY;
        CurrentFrameResources().CommandLists[ActiveContext]->RSSetViewports(1, &D3Dvp);

        D3D12_RECT scissorRect;
        scissorRect.left    = static_cast<LONG>(vpX);
        scissorRect.right   = static_cast<LONG>(vpX + vpW);
        scissorRect.top     = static_cast<LONG>(vpY);
        scissorRect.bottom  = static_cast<LONG>(vpY + vpH);

        CurrentFrameResources().CommandLists[ActiveContext]->RSSetScissorRects(1, &scissorRect);
    }

	bool HandleMessages(void)
	{
		MSG msg;
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// This is to provide a means to terminate after a maximum number of frames
		// to facilitate automated testing
        #ifdef MAX_FRAMES_ACTIVE 
            if (maxFrames > 0)
            {
		        if (--maxFrames <= 0)
			        Running = false;
            }
        #endif
		return Running;
	}

    void Run(bool (*MainLoop)(bool retryCreate))
    {
        while (HandleMessages())
        {
            // true => we'll attempt to retry for ovrError_DisplayLost
            if (!MainLoop(true))
                break;
            // Sleep a bit before retrying to reduce CPU load while the HMD is disconnected
            Sleep(10);
        }
    }

	void ReleaseDevice()
	{
        if (SwapChain)
        {
            SwapChain->SetFullscreenState(FALSE, NULL);
            Release(SwapChain);
        }
        for (int i = 0; i < SwapChainNumFrames; i++)
        {
            SwapChainFrameResources& currFrameRes = PerFrameResources[i];
            Release(currFrameRes.SwapChainBuffer);

            for (int contextIdx = 0; contextIdx < DrawContext_Count; contextIdx++)
            {
                Release(currFrameRes.CommandAllocators[contextIdx]);
                Release(currFrameRes.CommandLists[contextIdx]);
            }
            Release(currFrameRes.PresentFenceRes);
        }
        Release(RtvHeap);
        Release(DsvHeap);
        Release(CbvSrvHeap);
        Release(Device);
        Release(DebugController);
        delete MainDepthBuffer;
        MainDepthBuffer = nullptr;
	}

    void InitCommandList(DrawContext context)
    {
        SwapChainFrameResources& currFrameRes = CurrentFrameResources();

        if (currFrameRes.CommandListSubmitted[context])
        {
            HRESULT hr = currFrameRes.CommandAllocators[context]->Reset();
            VALIDATE((hr == ERROR_SUCCESS), "CommandAllocator Reset failed");

            hr = currFrameRes.CommandLists[context]->Reset(currFrameRes.CommandAllocators[context], nullptr);
            VALIDATE((hr == ERROR_SUCCESS), "CommandList Reset failed");

            ID3D12DescriptorHeap* heaps[] = { CbvSrvHeap };
            currFrameRes.CommandLists[context]->SetDescriptorHeaps(_countof(heaps), heaps);

            currFrameRes.CommandListSubmitted[context] = false;
        }
    }

    void InitFrame(bool finalContextUsed)
    {
        for (int bufIdx = 0; bufIdx < DrawContext_Count; bufIdx++)
        {
            if(!finalContextUsed && bufIdx == DrawContext_Final)
                continue;

            InitCommandList((DrawContext)bufIdx);
        }

        SwapChainFrameResources& currFrameRes = CurrentFrameResources();

        if (finalContextUsed)
        {
            CD3DX12_RESOURCE_BARRIER rb = CD3DX12_RESOURCE_BARRIER::Transition(currFrameRes.SwapChainBuffer,
                                                                               D3D12_RESOURCE_STATE_PRESENT,
                                                                               D3D12_RESOURCE_STATE_RENDER_TARGET);
            currFrameRes.CommandLists[DrawContext_Final]->ResourceBarrier(1, &rb);
        }
    }

    void WaitForPreviousFrame()
    {
        {
            DirectX12::SwapChainFrameResources& currFrameRes = CurrentFrameResources();

            // Signal and increment the fence value.
            currFrameRes.PresentFenceWaitValue = currFrameRes.PresentFenceValue;
            HRESULT hr = CommandQueue->Signal(currFrameRes.PresentFenceRes, currFrameRes.PresentFenceWaitValue);
            VALIDATE((hr == ERROR_SUCCESS), "CommandQueue Signal failed");
            currFrameRes.PresentFenceValue++;

            currFrameRes.PresentFenceRes->SetEventOnCompletion(currFrameRes.PresentFenceWaitValue, currFrameRes.PresentFenceEvent);
            VALIDATE((hr == ERROR_SUCCESS), "SetEventOnCompletion failed");
        }

        // goto next frame index and start waiting for the fence - ideally we don't wait at all
        {
            SwapChainFrameIndex = (SwapChainFrameIndex + 1) % SwapChainNumFrames;
            DirectX12::SwapChainFrameResources& currFrameRes = CurrentFrameResources();

            // Wait until the previous frame is finished.
            if (currFrameRes.PresentFenceWaitValue != -1 && // -1 means we never kicked off this frame
                currFrameRes.PresentFenceRes->GetCompletedValue() < currFrameRes.PresentFenceWaitValue)
            {
                WaitForSingleObject(currFrameRes.PresentFenceEvent, 10000);
            }

            VALIDATE((SwapChainFrameIndex == SwapChain->GetCurrentBackBufferIndex()), "Swap chain index validation failed");
            //SwapChainFrameIndex = SwapChain->GetCurrentBackBufferIndex();
        }
    }

    void SubmitCommandList(DrawContext context)
    {
        DirectX12::SwapChainFrameResources& currFrameRes = CurrentFrameResources();

        HRESULT hr = currFrameRes.CommandLists[context]->Close();
        VALIDATE((hr == ERROR_SUCCESS), "CommandList Close failed");

        ID3D12CommandList* ppCommandLists[] = { currFrameRes.CommandLists[context] };
        CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        currFrameRes.CommandListSubmitted[context] = true;
    }

    void SubmitCommandListAndPresent(bool finalContextUsed)
    {
        if (finalContextUsed)
        {
            DirectX12::SwapChainFrameResources& currFrameRes = CurrentFrameResources();

            VALIDATE(ActiveContext == DrawContext_Final, "Invalid context set before Present");

            // Indicate that the back buffer will now be used to present.
            CD3DX12_RESOURCE_BARRIER rb = CD3DX12_RESOURCE_BARRIER::Transition(currFrameRes.SwapChainBuffer,
                                                                               D3D12_RESOURCE_STATE_COPY_DEST,
                                                                               D3D12_RESOURCE_STATE_PRESENT);
            currFrameRes.CommandLists[ActiveContext]->ResourceBarrier(1, &rb);

            SubmitCommandList(DrawContext_Final);

            // Present the frame.
            HRESULT hr = SwapChain->Present(0, 0);
            VALIDATE((hr == ERROR_SUCCESS), "SwapChain Present failed");

            WaitForPreviousFrame();
        }

        InitFrame(finalContextUsed);
    }
};

// global DX12 state
static struct DirectX12 DIRECTX;

//------------------------------------------------------------
struct Texture
{
    ID3D12Resource* TextureRes;
    CD3DX12_CPU_DESCRIPTOR_HANDLE SrvHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle;

    int SizeW, SizeH;
    UINT MipLevels;

	enum { AUTO_WHITE = 1, AUTO_WALL, AUTO_FLOOR, AUTO_CEILING, AUTO_GRID, AUTO_GRADE_256 };

private: Texture() {};

public:
    void Init(int sizeW, int sizeH, bool rendertarget, UINT mipLevels, int sampleCount)
    {
        SizeW = sizeW;
        SizeH = sizeH;
        MipLevels = mipLevels;

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = UINT16(MipLevels);
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = SizeW;
        textureDesc.Height = SizeH;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = sampleCount;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        if (rendertarget) textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearVal = { DXGI_FORMAT_R8G8B8A8_UNORM,{ 0.0f, 0.0f, 0.0f, 1.0f } };

        CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = DIRECTX.Device->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            rendertarget ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COPY_DEST,
            rendertarget ? &clearVal : nullptr,
            IID_PPV_ARGS(&TextureRes));
        VALIDATE((hr == ERROR_SUCCESS), "CreateCommittedResource failed");

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = MipLevels;
        SrvHandle = DIRECTX.CbvSrvHandleProvider.AllocCpuHandle();
        DIRECTX.Device->CreateShaderResourceView(TextureRes, &srvDesc, SrvHandle);

        if (rendertarget)
        {
            RtvHandle = DIRECTX.RtvHandleProvider.AllocCpuHandle();
            DIRECTX.Device->CreateRenderTargetView(TextureRes, nullptr, RtvHandle);
        }
    }
	Texture(int sizeW, int sizeH, bool rendertarget, int mipLevels = 1, int sampleCount = 1)
	{
        Init(sizeW, sizeH, rendertarget, mipLevels, sampleCount);
	}
	Texture(bool rendertarget, int sizeW, int sizeH, int autoFillData = 0, int sampleCount = 1)
	{
        Init(sizeW, sizeH, rendertarget, autoFillData ? 8 : 1, sampleCount);
		if (!rendertarget && autoFillData) AutoFillTexture(autoFillData);
	}
    ~Texture()
    {
        Release(TextureRes);
    }

	void FillTexture(uint32_t * pix)
	{
        HRESULT hr;
        ID3D12Resource* textureUploadHeap;

		//Make local ones, because will be reducing them
		int sizeW = SizeW; 
		int sizeH = SizeH;
		for (UINT level = 0; level < MipLevels; level++)
		{
            // Push data into the texture
            {
                DirectX12::SwapChainFrameResources& currFrameRes = DIRECTX.CurrentFrameResources();
                currFrameRes.CommandLists[DrawContext_Final]->Reset(currFrameRes.CommandAllocators[DrawContext_Final], nullptr);

                {
                    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(TextureRes, 0, 1);

                    // Create the GPU upload buffer.
                    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
                    CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
                    hr = DIRECTX.Device->CreateCommittedResource(
                        &heapProp,
                        D3D12_HEAP_FLAG_NONE,
                        &resDesc,
                        D3D12_RESOURCE_STATE_GENERIC_READ,
                        nullptr,
                        IID_PPV_ARGS(&textureUploadHeap));
                    VALIDATE((hr == ERROR_SUCCESS), "CreateCommittedResource upload failed");

                    // Copy data to the intermediate upload heap and then schedule a copy from the upload heap to the Texture2D
                    uint8_t* textureByte = (uint8_t*)pix;

                    D3D12_SUBRESOURCE_DATA textureData = {};
                    textureData.pData = textureByte;
                    textureData.RowPitch = sizeW * sizeof(uint32_t);
                    textureData.SlicePitch = textureData.RowPitch * sizeH;

                    UpdateSubresources(currFrameRes.CommandLists[DrawContext_Final], TextureRes, textureUploadHeap, 0, level, 1, &textureData);

                    // transition resource on last mip-level
                    if (level == MipLevels - 1)
                    {
                        CD3DX12_RESOURCE_BARRIER resBar = CD3DX12_RESOURCE_BARRIER::Transition(TextureRes,
                                                                                               D3D12_RESOURCE_STATE_COPY_DEST,
                                                                                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                        currFrameRes.CommandLists[DrawContext_Final]->ResourceBarrier(1, &resBar);
                    }
                }


                // Close the command list and execute it to begin the initial GPU setup.
                hr = currFrameRes.CommandLists[DrawContext_Final]->Close();
                VALIDATE((hr == ERROR_SUCCESS), "CommandList Close failed");

                ID3D12CommandList* ppCommandLists[] = { currFrameRes.CommandLists[DrawContext_Final] };
                DIRECTX.CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

                // Create synchronization objects and wait until assets have been uploaded to the GPU.
                {
                    // Wait for the command list to execute; we are reusing the same command 
                    // list in our main loop but for now, we just want to wait for setup to 
                    // complete before continuing.
                    {
                        // Signal and increment the fence value.
                        currFrameRes.PresentFenceWaitValue = currFrameRes.PresentFenceValue;
                        hr = DIRECTX.CommandQueue->Signal(currFrameRes.PresentFenceRes, currFrameRes.PresentFenceWaitValue);
                        VALIDATE((hr == ERROR_SUCCESS), "CommandQueue Signal failed");
                        currFrameRes.PresentFenceValue++;

                        // Wait until the copy is finished.
                        if (currFrameRes.PresentFenceRes->GetCompletedValue() < currFrameRes.PresentFenceWaitValue)
                        {
                            hr = currFrameRes.PresentFenceRes->SetEventOnCompletion(currFrameRes.PresentFenceWaitValue, currFrameRes.PresentFenceEvent);
                            VALIDATE((hr == ERROR_SUCCESS), "SetEventOnCompletion failed");
                            WaitForSingleObject(currFrameRes.PresentFenceEvent, 10000);
                        }
                    }
                }
            }

			for (int j = 0; j < (sizeH & ~1); j += 2)
			{
				uint8_t* psrc = (uint8_t *)pix + (sizeW * j * 4);
				uint8_t* pdest = (uint8_t *)pix + (sizeW * j);
				for (int i = 0; i < sizeW >> 1; i++, psrc += 8, pdest += 4)
				{
					pdest[0] = (((int)psrc[0]) + psrc[4] + psrc[sizeW * 4 + 0] + psrc[sizeW * 4 + 4]) >> 2;
					pdest[1] = (((int)psrc[1]) + psrc[5] + psrc[sizeW * 4 + 1] + psrc[sizeW * 4 + 5]) >> 2;
					pdest[2] = (((int)psrc[2]) + psrc[6] + psrc[sizeW * 4 + 2] + psrc[sizeW * 4 + 6]) >> 2;
					pdest[3] = (((int)psrc[3]) + psrc[7] + psrc[sizeW * 4 + 3] + psrc[sizeW * 4 + 7]) >> 2;
				}
			}
			sizeW >>= 1;  sizeH >>= 1;
		}
        Release(textureUploadHeap);
    }

    static void ConvertToSRGB(uint32_t * linear)
	{
        uint32_t drgb[3];
		for (int k = 0; k < 3; k++)
		{
			float rgb = ((float)((*linear >> (k * 8)) & 0xff)) / 255.0f;
			rgb = pow(rgb, 2.2f);
            drgb[k] = (uint32_t)(rgb * 255.0f);
		}
		*linear = (*linear & 0xff000000) + (drgb[2] << 16) + (drgb[1] << 8) + (drgb[0] << 0);
	}

	void AutoFillTexture(int autoFillData)
	{
        uint32_t * pix = (uint32_t *)malloc(sizeof(uint32_t) *  SizeW * SizeH);
		for (int j = 0; j < SizeH; j++)
		for (int i = 0; i < SizeW; i++)
		{
            uint32_t * curr = &pix[j*SizeW + i];
			switch (autoFillData)
			{
			case(AUTO_WALL) : *curr = (((j / 4 & 15) == 0) || (((i / 4 & 15) == 0) && ((((i / 4 & 31) == 0) ^ ((j / 4 >> 4) & 1)) == 0))) ?
				0xff3c3c3c : 0xffb4b4b4; break;
			case(AUTO_FLOOR) : *curr = (((i >> 7) ^ (j >> 7)) & 1) ? 0xffb4b4b4 : 0xff505050; break;
			case(AUTO_CEILING) : *curr = (i / 4 == 0 || j / 4 == 0) ? 0xff505050 : 0xffb4b4b4; break;
			case(AUTO_WHITE) : *curr = 0xffffffff;              break;
			case(AUTO_GRADE_256) : *curr = 0xff000000 + i*0x010101;              break;
			case(AUTO_GRID) : *curr = (i<4) || (i>(SizeW - 5)) || (j<4) || (j>(SizeH - 5)) ? 0xffffffff : 0xff000000; break;
			default: *curr = 0xffffffff;              break;
			}
		}
		FillTexture(pix);
        free(pix);
	}
};

//-----------------------------------------------------
struct Material
{
	Texture*                    Tex;
	UINT                        VertexSize;
    ID3D12RootSignature*        RootSignature;
    ID3D12PipelineState*        PipelineState;
    
	enum { MAT_WRAP = 1, MAT_WIRE = 2, MAT_ZALWAYS = 4, MAT_NOCULL = 8 , MAT_TRANS = 16};

    Material(
        Texture * t,
        uint32_t flags = MAT_WRAP | MAT_TRANS,
        D3D12_INPUT_ELEMENT_DESC * vertexDesc = NULL,
        int numVertexDesc = 3,
        char* vertexShaderStr = NULL,
        char* pixelShaderStr = NULL,
        int vSize = 24)
        : Tex(t)
        , VertexSize(vSize)
    {
        UNREFERENCED_PARAMETER(flags);

        {
            // Create the pipeline state, which includes compiling and loading shaders.
            {
                D3D12_INPUT_ELEMENT_DESC defaultVertexDesc[] = {
                    { "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			        { "Color",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			        { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, };

		        // Use defaults if no shaders specified
		        char* defaultVertexShaderSrc =
			        "float4x4 ProjView;\n"
                    "float4 MasterCol;\n"
			        "void main(in  float4 Position  : POSITION,    in  float4 Color : COLOR0, in  float2 TexCoord  : TEXCOORD0,\n"
			        "          out float4 oPosition : SV_Position, out float4 oColor: COLOR0, out float2 oTexCoord : TEXCOORD0)\n"
			        "{   oPosition = mul(ProjView, Position); oTexCoord = TexCoord;\n"
			        "    oColor = MasterCol * Color; }\n"; 
		        char* defaultPixelShaderSrc =
                    "Texture2D Texture : register(t0); SamplerState Linear : register(s0);\n"
			        "float4 main(in float4 Position : SV_Position, in float4 Color: COLOR0, in float2 TexCoord : TEXCOORD0) : SV_Target\n"
			        "{   float4 TexCol = Texture.Sample(Linear, TexCoord);\n"
			        "    if (TexCol.a==0) clip(-1);\n" // If alpha = 0, don't draw
			        "    return(Color * TexCol); }\n";

		        if (!vertexDesc)        vertexDesc = defaultVertexDesc;
		        if (!vertexShaderStr)   vertexShaderStr = defaultVertexShaderSrc;
		        if (!pixelShaderStr)    pixelShaderStr = defaultPixelShaderSrc;

                UINT compileFlags = 0;
#ifdef _DEBUG
                // Enable better shader debugging with the graphics debugging tools.
                compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

                ID3DBlob* compiledVS;
                HRESULT hr = D3DCompile(vertexShaderStr, strlen(vertexShaderStr), 0, 0, 0, "main", "vs_5_0", compileFlags, 0, &compiledVS, 0);
                VALIDATE((hr == ERROR_SUCCESS), "D3DCompile VertexShader failed");

                ID3DBlob* compiledPS;
                hr = D3DCompile(pixelShaderStr, strlen(pixelShaderStr), 0, 0, 0, "main", "ps_5_0", compileFlags, 0, &compiledPS, 0);
                VALIDATE((hr == ERROR_SUCCESS), "D3DCompile PixelShader failed");

                {
                    CD3DX12_DESCRIPTOR_RANGE ranges[2];
                    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
                    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

                    CD3DX12_ROOT_PARAMETER rootParameters[2];
                    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
                    rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);

                    D3D12_STATIC_SAMPLER_DESC sampler = {};
                    sampler.Filter = D3D12_FILTER_ANISOTROPIC;
                    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                    sampler.MipLODBias = 0;
                    sampler.MaxAnisotropy = 8;
                    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
                    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
                    sampler.MinLOD = 0.0f;
                    sampler.MaxLOD = D3D12_FLOAT32_MAX;
                    sampler.ShaderRegister = 0;
                    sampler.RegisterSpace = 0;
                    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

                    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
                    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler,
                        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

                    ID3DBlob* signature;
                    ID3DBlob* error;
                    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
                    //{
                    //    char* errStr = (char*)error->GetBufferPointer();
                    //    SIZE_T len = error->GetBufferSize();

                    //    if (errStr && len > 0)
                    //    {
                    //        OutputDebugStringA(errStr);
                    //    }
                    //}
                    VALIDATE((hr == ERROR_SUCCESS), "D3D12SerializeRootSignature failed");
                    hr = DIRECTX.Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature));
                    VALIDATE((hr == ERROR_SUCCESS), "CreateRootSignature failed");
                }

                D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.InputLayout.pInputElementDescs = vertexDesc;
				psoDesc.InputLayout.NumElements = (UINT)numVertexDesc;
                psoDesc.pRootSignature = RootSignature;
                psoDesc.VS.pShaderBytecode = reinterpret_cast<UINT8*>(compiledVS->GetBufferPointer());
                psoDesc.VS.BytecodeLength = compiledVS->GetBufferSize();
				psoDesc.PS.pShaderBytecode = reinterpret_cast<UINT8*>(compiledPS->GetBufferPointer());
				psoDesc.PS.BytecodeLength = compiledPS->GetBufferSize();
                psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
                psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
                psoDesc.DepthStencilState.DepthEnable = TRUE;
                psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
                psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
                psoDesc.DepthStencilState.StencilEnable = FALSE;
                psoDesc.SampleMask = UINT_MAX;
                psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                psoDesc.NumRenderTargets = 1;
                psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
                psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
                psoDesc.SampleDesc.Count = 1;
                hr = DIRECTX.Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState));
                VALIDATE((hr == ERROR_SUCCESS), "CreateGraphicsPipelineState failed");
            }
        }
	}
    ~Material()
    {
        delete Tex; Tex = nullptr;
        Release(RootSignature);
        Release(PipelineState);
    }
};

//----------------------------------------------------------------------
struct Vertex
{
	XMFLOAT3    Pos;
	uint32_t    C;
	float       U, V;
	Vertex() {};
    Vertex(XMFLOAT3 pos, uint32_t c, float u, float v) : Pos(pos), C(c), U(u), V(v) {};
};

//-----------------------------------------------------------------------
struct TriangleSet
{
	int       numVertices, numIndices, maxBuffer;
	Vertex    * Vertices;
	short     * Indices;
	TriangleSet(int maxTriangles = 2000) : maxBuffer(3 * maxTriangles)
	{
		numVertices = numIndices = 0;
		Vertices = (Vertex *)_aligned_malloc(maxBuffer *sizeof(Vertex), 16);
		Indices = (short *)  _aligned_malloc(maxBuffer *sizeof(short), 16);
	}
    ~TriangleSet()
    {
        _aligned_free(Vertices);
        _aligned_free(Indices);
    }
	void AddQuad(Vertex v0, Vertex v1, Vertex v2, Vertex v3) { AddTriangle(v0, v1, v2);	AddTriangle(v3, v2, v1); }
	void AddTriangle(Vertex v0, Vertex v1, Vertex v2)
	{
		VALIDATE(numVertices <= (maxBuffer - 3), "Insufficient triangle set");
		for (int i = 0; i < 3; i++) Indices[numIndices++] = short(numVertices + i);
		Vertices[numVertices++] = v0;
		Vertices[numVertices++] = v1;
		Vertices[numVertices++] = v2;
	}

    uint32_t ModifyColor(uint32_t c, XMFLOAT3 pos)
	{
		#define GetLengthLocal(v)  (sqrt(v.x*v.x + v.y*v.y + v.z*v.z))
		float dist1 = GetLengthLocal(XMFLOAT3(pos.x - (-2), pos.y - (4), pos.z - (-2)));
		float dist2 = GetLengthLocal(XMFLOAT3(pos.x - (3),  pos.y - (4), pos.z - (-3)));
		float dist3 = GetLengthLocal(XMFLOAT3(pos.x - (-4), pos.y - (3), pos.z - (25)));
		int   bri = rand() % 160;
		float R = ((c >> 16) & 0xff) * (bri + 192.0f*(0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
		float G = ((c >> 8) & 0xff) * (bri + 192.0f*(0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
		float B = ((c >> 0) & 0xff) * (bri + 192.0f*(0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
		return( (c & 0xff000000) + ((R>255 ? 255 : (uint32_t)R) << 16) + ((G>255 ? 255 : (uint32_t)G) << 8) + (B>255 ? 255 : (uint32_t)B));
	}

    void AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, uint32_t c)
	{
		AddQuad(Vertex(XMFLOAT3(x1, y2, z1), ModifyColor(c, XMFLOAT3(x1, y2, z1)), z1, x1),
			    Vertex(XMFLOAT3(x2, y2, z1), ModifyColor(c, XMFLOAT3(x2, y2, z1)), z1, x2),
			    Vertex(XMFLOAT3(x1, y2, z2), ModifyColor(c, XMFLOAT3(x1, y2, z2)), z2, x1),
			    Vertex(XMFLOAT3(x2, y2, z2), ModifyColor(c, XMFLOAT3(x2, y2, z2)), z2, x2));
		AddQuad(Vertex(XMFLOAT3(x2, y1, z1), ModifyColor(c, XMFLOAT3(x2, y1, z1)), z1, x2),
			    Vertex(XMFLOAT3(x1, y1, z1), ModifyColor(c, XMFLOAT3(x1, y1, z1)), z1, x1),
			    Vertex(XMFLOAT3(x2, y1, z2), ModifyColor(c, XMFLOAT3(x2, y1, z2)), z2, x2),
			    Vertex(XMFLOAT3(x1, y1, z2), ModifyColor(c, XMFLOAT3(x1, y1, z2)), z2, x1));
		AddQuad(Vertex(XMFLOAT3(x1, y1, z2), ModifyColor(c, XMFLOAT3(x1, y1, z2)), z2, y1),
			    Vertex(XMFLOAT3(x1, y1, z1), ModifyColor(c, XMFLOAT3(x1, y1, z1)), z1, y1),
			    Vertex(XMFLOAT3(x1, y2, z2), ModifyColor(c, XMFLOAT3(x1, y2, z2)), z2, y2),
			    Vertex(XMFLOAT3(x1, y2, z1), ModifyColor(c, XMFLOAT3(x1, y2, z1)), z1, y2));
		AddQuad(Vertex(XMFLOAT3(x2, y1, z1), ModifyColor(c, XMFLOAT3(x2, y1, z1)), z1, y1),
			    Vertex(XMFLOAT3(x2, y1, z2), ModifyColor(c, XMFLOAT3(x2, y1, z2)), z2, y1),
			    Vertex(XMFLOAT3(x2, y2, z1), ModifyColor(c, XMFLOAT3(x2, y2, z1)), z1, y2),
			    Vertex(XMFLOAT3(x2, y2, z2), ModifyColor(c, XMFLOAT3(x2, y2, z2)), z2, y2));
		AddQuad(Vertex(XMFLOAT3(x1, y1, z1), ModifyColor(c, XMFLOAT3(x1, y1, z1)), x1, y1),
			    Vertex(XMFLOAT3(x2, y1, z1), ModifyColor(c, XMFLOAT3(x2, y1, z1)), x2, y1),
			    Vertex(XMFLOAT3(x1, y2, z1), ModifyColor(c, XMFLOAT3(x1, y2, z1)), x1, y2),
			    Vertex(XMFLOAT3(x2, y2, z1), ModifyColor(c, XMFLOAT3(x2, y2, z1)), x2, y2));
		AddQuad(Vertex(XMFLOAT3(x2, y1, z2), ModifyColor(c, XMFLOAT3(x2, y1, z2)), x2, y1),
			    Vertex(XMFLOAT3(x1, y1, z2), ModifyColor(c, XMFLOAT3(x1, y1, z2)), x1, y1),
			    Vertex(XMFLOAT3(x2, y2, z2), ModifyColor(c, XMFLOAT3(x2, y2, z2)), x2, y2),
			    Vertex(XMFLOAT3(x1, y2, z2), ModifyColor(c, XMFLOAT3(x1, y2, z2)), x1, y2));
	}
};

//----------------------------------------------------------------------
struct Model
{
    static const int NumEyes = 2;
	XMFLOAT3     Pos;
	XMFLOAT4     Rot;
	Material   * MaterialState;
	DataBuffer * VertexBuffer;
	DataBuffer * IndexBuffer;
	int          NumIndices;

    ID3D12PipelineState       * PipelineState;
    D3D12_VERTEX_BUFFER_VIEW    VertexBufferView;
    D3D12_INDEX_BUFFER_VIEW     IndexBufferView;

    struct ModelConstants
    {
        XMFLOAT4X4  WorldViewProj;
        XMFLOAT4    MasterColor;
    };

    // per-frame constant buffer data
    struct FrameResources
    {
        ID3D12Resource*             ConstantBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE ConstantBufferHandle;
        ModelConstants              ConstantBufferData;
        UINT8*                      ConstantBufferMapPtr;
    };
    FrameResources  PerFrameRes[DirectX12::SwapChainNumFrames][NumEyes];

    void Init(TriangleSet * t)
    {
		NumIndices = t->numIndices;
		VertexBuffer = new DataBuffer(DIRECTX.Device, &t->Vertices[0], t->numVertices * sizeof(Vertex));
		IndexBuffer = new DataBuffer(DIRECTX.Device, &t->Indices[0], t->numIndices * sizeof(short));

        // Initialize vertex buffer view
        VertexBufferView.BufferLocation = VertexBuffer->D3DBuffer->GetGPUVirtualAddress();
        VertexBufferView.StrideInBytes = sizeof(Vertex);
        VertexBufferView.SizeInBytes = (UINT)VertexBuffer->BufferSize;

        // Initialize index buffer view
        IndexBufferView.BufferLocation = IndexBuffer->D3DBuffer->GetGPUVirtualAddress();
        IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
        IndexBufferView.SizeInBytes = (UINT)IndexBuffer->BufferSize;

        for (int frameIdx = 0; frameIdx < DirectX12::SwapChainNumFrames; frameIdx++)
        {
            for (int eye = 0; eye < NumEyes; eye++)
            {
                FrameResources& frameRes = PerFrameRes[frameIdx][eye];

                CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
                CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
                HRESULT hr = DIRECTX.Device->CreateCommittedResource(
                    &heapProp,
                    D3D12_HEAP_FLAG_NONE,
                    &resDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&frameRes.ConstantBuffer));
                VALIDATE((hr == ERROR_SUCCESS), "CommandQueue Signal failed");

                // Describe and create a constant buffer view.
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation = frameRes.ConstantBuffer->GetGPUVirtualAddress();
                // CB size is required to be 256-byte aligned.
                cbvDesc.SizeInBytes = (sizeof(ModelConstants) + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
                frameRes.ConstantBufferHandle = DIRECTX.CbvSrvHandleProvider.AllocCpuHandle();
                DIRECTX.Device->CreateConstantBufferView(&cbvDesc, frameRes.ConstantBufferHandle);

                hr = frameRes.ConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&frameRes.ConstantBufferMapPtr));
                VALIDATE((hr == ERROR_SUCCESS), "Constant Buffer Map failed");
                ZeroMemory(&frameRes.ConstantBufferData, sizeof(ModelConstants));
                memcpy(frameRes.ConstantBufferMapPtr, &frameRes.ConstantBufferData, sizeof(ModelConstants));
            }
        }
    }
	Model(TriangleSet * t, XMFLOAT3 argPos, XMFLOAT4 argRot, Material * argMaterial) :
        Pos(argPos),
        Rot(argRot),
        MaterialState(argMaterial)
	{
        Init(t);
	}
    // 2D scenes, for latency tester and full screen copies, etc
	Model(Material * mat, float minx, float miny, float maxx, float maxy,  float zDepth = 0) :
        Pos(XMFLOAT3(0, 0, 0)),
        Rot(XMFLOAT4(0, 0, 0, 1)),
        MaterialState(mat)
	{
		TriangleSet quad;
		quad.AddQuad(Vertex(XMFLOAT3(minx, miny, zDepth), 0xffffffff, 0, 1),
			Vertex(XMFLOAT3(minx, maxy, zDepth), 0xffffffff, 0, 0),
			Vertex(XMFLOAT3(maxx, miny, zDepth), 0xffffffff, 1, 1),
			Vertex(XMFLOAT3(maxx, maxy, zDepth), 0xffffffff, 1, 0));
        Init(&quad);
	}
    ~Model()
    {
        delete MaterialState; MaterialState = nullptr;
        delete VertexBuffer; VertexBuffer = nullptr;
        delete IndexBuffer; IndexBuffer = nullptr;
    }

	void Render(XMMATRIX * projView, float R, float G, float B, float A, bool standardUniforms)
	{
        DirectX12::SwapChainFrameResources& currFrameRes = DIRECTX.CurrentFrameResources();
        FrameResources& currConstantRes = PerFrameRes[DIRECTX.SwapChainFrameIndex][DIRECTX.ActiveEyeIndex];

        if (standardUniforms)
        {
            XMMATRIX modelMat = XMMatrixMultiply(XMMatrixRotationQuaternion(XMLoadFloat4(&Rot)), XMMatrixTranslationFromVector(XMLoadFloat3(&Pos)));
		    XMMATRIX mat = XMMatrixMultiply(modelMat, *projView);            
            XMStoreFloat4x4(&currConstantRes.ConstantBufferData.WorldViewProj, mat);
            currConstantRes.ConstantBufferData.MasterColor = XMFLOAT4(R, G, B, A);

            memcpy(currConstantRes.ConstantBufferMapPtr + 0, &currConstantRes.ConstantBufferData, sizeof(ModelConstants));
        }

        auto activeCmdList = currFrameRes.CommandLists[DIRECTX.ActiveContext];

        activeCmdList->SetGraphicsRootSignature(MaterialState->RootSignature);
        activeCmdList->SetPipelineState(MaterialState->PipelineState);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(DIRECTX.CbvSrvHandleProvider.GpuHandleFromCpuHandle(MaterialState->Tex->SrvHandle));
        activeCmdList->SetGraphicsRootDescriptorTable(0, srvGpuHandle);
        
        CD3DX12_GPU_DESCRIPTOR_HANDLE constantBufferGpuhandle(DIRECTX.CbvSrvHandleProvider.GpuHandleFromCpuHandle(currConstantRes.ConstantBufferHandle));
        activeCmdList->SetGraphicsRootDescriptorTable(1, constantBufferGpuhandle);

        activeCmdList->IASetIndexBuffer(&IndexBufferView);
        activeCmdList->IASetVertexBuffers(0, 1, &VertexBufferView);
        activeCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        activeCmdList->DrawIndexedInstanced(IndexBufferView.SizeInBytes / sizeof(short), 1, 0, 0, 0);
	}
};

//------------------------------------------------------------------------- 
struct Scene
{
    static const int MAX_MODELS = 100;
	Model *Models[MAX_MODELS];
    int numModels;

	void Add(Model * n)
    {
        if (numModels < MAX_MODELS)
            Models[numModels++] = n;
    }

	void Render(XMMATRIX * projView, float R, float G, float B, float A, bool standardUniforms)
	{
		for (int i = 0; i < numModels; ++i)
            Models[i]->Render(projView, R, G, B, A, standardUniforms);
	}
    
    void Init(bool includeIntensiveGPUobject)
	{
		TriangleSet cube;
		cube.AddSolidColorBox(0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0xff404040);
		Add(
            new Model(&cube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
                new Material(
                    new Texture(false, 256, 256, Texture::AUTO_CEILING)
                )
            )
        );

		TriangleSet spareCube;
		spareCube.AddSolidColorBox(0.1f, -0.1f, 0.1f, -0.1f, +0.1f, -0.1f, 0xffff0000);
		Add(
            new Model(&spareCube, XMFLOAT3(0, -10, 0), XMFLOAT4(0, 0, 0, 1),
                new Material(
                    new Texture(false, 256, 256, Texture::AUTO_CEILING)
                )
            )
        );

		TriangleSet walls;
		walls.AddSolidColorBox(10.1f, 0.0f, 20.0f, 10.0f, 4.0f, -20.0f, 0xff808080);  // Left Wall
		walls.AddSolidColorBox(10.0f, -0.1f, 20.1f, -10.0f, 4.0f, 20.0f, 0xff808080); // Back Wall
		walls.AddSolidColorBox(-10.0f, -0.1f, 20.0f, -10.1f, 4.0f, -20.0f, 0xff808080);   // Right Wall
		Add(
            new Model(&walls, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
                new Material(
                    new Texture(false, 256, 256, Texture::AUTO_WALL)
                )
            )
        );

		if (includeIntensiveGPUobject)
		{
			TriangleSet partitions;
			for (float depth = 0.0f; depth > -3.0f; depth -= 0.1f)
				partitions.AddSolidColorBox(9.0f, 0.5f, -depth, -9.0f, 3.5f, -depth, 0x10ff80ff); // Partition
			Add(
                new Model(&partitions, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
                    new Material(
                        new Texture(false, 256, 256, Texture::AUTO_FLOOR)
                    )
                )
            ); // Floors
		}

		TriangleSet floors;
		floors.AddSolidColorBox(10.0f, -0.1f, 20.0f, -10.0f, 0.0f, -20.1f, 0xff808080); // Main floor
		floors.AddSolidColorBox(15.0f, -6.1f, -18.0f, -15.0f, -6.0f, -30.0f, 0xff808080); // Bottom floor
		Add(
            new Model(&floors, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
                new Material(
                    new Texture(false, 256, 256, Texture::AUTO_FLOOR)
                )
            )
        ); // Floors

		TriangleSet ceiling;
		ceiling.AddSolidColorBox(10.0f, 4.0f, 20.0f, -10.0f, 4.1f, -20.1f, 0xff808080);
		Add(
            new Model(&ceiling, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
                new Material(
                    new Texture(false, 256, 256, Texture::AUTO_CEILING)
                )
            )
        ); // Ceiling

		TriangleSet furniture;
		furniture.AddSolidColorBox(-9.5f, 0.75f, -3.0f, -10.1f, 2.5f, -3.1f, 0xff383838);    // Right side shelf// Verticals
		furniture.AddSolidColorBox(-9.5f, 0.95f, -3.7f, -10.1f, 2.75f, -3.8f, 0xff383838);   // Right side shelf
		furniture.AddSolidColorBox(-9.55f, 1.20f, -2.5f, -10.1f, 1.30f, -3.75f, 0xff383838); // Right side shelf// Horizontals
		furniture.AddSolidColorBox(-9.55f, 2.00f, -3.05f, -10.1f, 2.10f, -4.2f, 0xff383838); // Right side shelf
		furniture.AddSolidColorBox(-5.0f, 1.1f, -20.0f, -10.0f, 1.2f, -20.1f, 0xff383838);   // Right railing   
		furniture.AddSolidColorBox(10.0f, 1.1f, -20.0f, 5.0f, 1.2f, -20.1f, 0xff383838);   // Left railing  
		for (float f = 5; f <= 9; f += 1)
            furniture.AddSolidColorBox(-f, 0.0f, -20.0f, -f - 0.1f, 1.1f, -20.1f, 0xff505050); // Left Bars
		for (float f = 5; f <= 9; f += 1)
            furniture.AddSolidColorBox(f, 1.1f, -20.0f, f + 0.1f, 0.0f, -20.1f, 0xff505050); // Right Bars
		furniture.AddSolidColorBox(1.8f, 0.8f, -1.0f, 0.0f, 0.7f, 0.0f, 0xff505000);  // Table
		furniture.AddSolidColorBox(1.8f, 0.0f, 0.0f, 1.7f, 0.7f, -0.1f, 0xff505000); // Table Leg 
		furniture.AddSolidColorBox(1.8f, 0.7f, -1.0f, 1.7f, 0.0f, -0.9f, 0xff505000); // Table Leg 
		furniture.AddSolidColorBox(0.0f, 0.0f, -1.0f, 0.1f, 0.7f, -0.9f, 0xff505000);  // Table Leg 
		furniture.AddSolidColorBox(0.0f, 0.7f, 0.0f, 0.1f, 0.0f, -0.1f, 0xff505000);  // Table Leg 
		furniture.AddSolidColorBox(1.4f, 0.5f, 1.1f, 0.8f, 0.55f, 0.5f, 0xff202050);  // Chair Set
		furniture.AddSolidColorBox(1.401f, 0.0f, 1.101f, 1.339f, 1.0f, 1.039f, 0xff202050); // Chair Leg 1
		furniture.AddSolidColorBox(1.401f, 0.5f, 0.499f, 1.339f, 0.0f, 0.561f, 0xff202050); // Chair Leg 2
		furniture.AddSolidColorBox(0.799f, 0.0f, 0.499f, 0.861f, 0.5f, 0.561f, 0xff202050); // Chair Leg 2
		furniture.AddSolidColorBox(0.799f, 1.0f, 1.101f, 0.861f, 0.0f, 1.039f, 0xff202050); // Chair Leg 2
		furniture.AddSolidColorBox(1.4f, 0.97f, 1.05f, 0.8f, 0.92f, 1.10f, 0xff202050); // Chair Back high bar
		for (float f = 3.0f; f <= 6.6f; f += 0.4f)
            furniture.AddSolidColorBox(3, 0.0f, -f, 2.9f, 1.3f, -f - 0.1f, 0xff404040); // Posts
		Add(
            new Model(&furniture, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
                new Material(
                    new Texture(false, 256, 256, Texture::AUTO_WHITE)
                )
            )
        ); // Fixtures & furniture
	}

	Scene() : numModels(0) {}
	Scene(bool includeIntensiveGPUobject) :
        numModels(0)
    {
        Init(includeIntensiveGPUobject);
    }
    void Release()
    {
        while (numModels-- > 0)
            delete Models[numModels];
    }
    ~Scene()
    {
        Release();
    }
};

//-----------------------------------------------------------
struct Camera
{
    XMFLOAT4 Pos;
    XMFLOAT4 Rot;
    Camera() {};
    Camera(XMVECTOR pos, XMVECTOR rot)
    {
        XMStoreFloat4(&Pos, pos);
        XMStoreFloat4(&Rot, rot);
    }

    XMMATRIX GetViewMatrix()
    {
        XMVECTOR posVec = XMLoadFloat4(&Pos);
        XMVECTOR rotVec = XMLoadFloat4(&Rot);
        XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), rotVec);
        return(XMMatrixLookAtRH(posVec, XMVectorAdd(posVec, forward), XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rotVec)));
    }

    XMVECTOR GetPosVec() { return XMLoadFloat4(&Pos); }
    XMVECTOR GetRotVec() { return XMLoadFloat4(&Rot); }

    void SetPosVec(XMVECTOR posVec) { XMStoreFloat4(&Pos, posVec); }
    void SetRotVec(XMVECTOR rotVec) { XMStoreFloat4(&Rot, rotVec); }
};

//----------------------------------------------------
struct Utility
{
	void Output(const char * fnt, ...)
	{
		static char string_text[1000];
		va_list args; va_start(args, fnt);
		vsprintf_s(string_text, fnt, args);
		va_end(args);
		OutputDebugStringA(string_text);
	}
} static Util;

#endif // OVR_Win32_DirectXAppUtil_h
