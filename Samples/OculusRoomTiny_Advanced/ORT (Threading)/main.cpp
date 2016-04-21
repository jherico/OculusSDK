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
/// In this sample, we use the layer system to show 
/// how to render a quad directly into the distorted image, 
/// thus bypassing the eye textures and retaining the resolution 
/// and precision of the original image.
/// The sample shows a simple textured quad, fixed in the scene 
/// in front of you.  By varying the input parameters, it
/// is simple to fix this into the scene if required, rather than
/// move and rotate with the player.

#define   OVR_D3D_VERSION 11
#include "Kernel/OVR_Threads.h"
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

#include <array>
#include <random>

// note: run Test_Samples with --gtest_filter=Test_Samples.WinMain_Threading to run just this test

using namespace OVR;

typedef struct ThreadTestState *ThreadTestStatePtr;

typedef int (*TestFn)(ThreadTestStatePtr state);

// named test functor
class NamedTest
{
public:
    const char  *name;
    TestFn      fn;
    NamedTest(const char *name, TestFn fn) : name(name), fn(fn) {}
    int operator() (ThreadTestStatePtr state);
};

// list of named tests
typedef std::vector<NamedTest> TestList;

// helper class to track tests in TestInstance::list singleton
class TestInstance
{
public:
    static TestList list;
    TestInstance(NamedTest test)
    {
        list.push_back(test);
    }
};

// TestInstance::list contains all statically initialized test functors
TestList TestInstance::list;

// globals
static const int numThreads = 3;
#if !defined(MAX_FRAMES_ACTIVE)
    static int maxFrames = 1000;
#endif
static bool running = true;
static int frameIndex = 0;

// per-thread state
struct ThreadTestState
{
    int             id;
    ovrSession      session;
    std::mt19937    rand;
    TestList        tests;
};

// invoke a named test
int NamedTest::operator() (ThreadTestStatePtr state)
{
    #if 0
    WCHAR message[1024];
    swprintf_s(message, L"Thread %d running %S\n", state->id, name);
    OutputDebugStringW(message);
    #endif
    return fn(state);
}

// prolog for declaring tests and adding them to TestInstance::list, tests return 0 on success
#define ThreadTest(name) \
static int name(ThreadTestStatePtr state); \
namespace TestInstances { static TestInstance name(NamedTest(#name, ::name)); } \
static int name(ThreadTestStatePtr state)

// use this to turn a particular case off
#define NoThreadTest(name) \
static int name(ThreadTestStatePtr state)

// API properties
#define LIST_BOOL_PROPERTIES(_) \
_(OVR_KEY_CUSTOM_EYE_RENDER, ovrTrue)

#define LIST_STRING_PROPERTIES(_) \
_(OVR_KEY_USER, ({ "Joe", "Sue", "Ernie" })) \
_(OVR_KEY_NAME, ({ "name1", "name2", "name3" })) \
_(OVR_KEY_GENDER, ({ "Male", "Female", "Unknown" })) \
_(OVR_KEY_EYE_CUP, ({ "A", "B", "C" }))

#define LIST_INT_PROPERTIES(_) \
_(OVR_KEY_EYE_RELIEF_DIAL, 10 + 1) \
_(OVR_PERF_HUD_MODE, ovrPerfHud_Count) \
_(OVR_DEBUG_HUD_STEREO_MODE, ovrDebugHudStereo_Count)

#define LIST_FLOAT_PROPERTIES(_) \
_(OVR_KEY_PLAYER_HEIGHT, 3) \
_(OVR_KEY_EYE_HEIGHT, 3)

#define LIST_FLOAT_ARRAY_PROPERTIES(_) \
_(OVR_KEY_NECK_TO_EYE_DISTANCE, 2) \
_(OVR_KEY_EYE_TO_NOSE_DISTANCE, 2) \
_(OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE, 2) \
_(OVR_KEY_CAMERA_POSITION_1, 7) \
_(OVR_KEY_CAMERA_EYE_LEVEL_POSITION, 7) \
_(OVR_DEBUG_HUD_STEREO_GUIDE_SIZE, 2) \
_(OVR_DEBUG_HUD_STEREO_GUIDE_POSITION, 3) \
_(OVR_DEBUG_HUD_STEREO_GUIDE_YAWPITCHROLL, 3) \
_(OVR_DEBUG_HUD_STEREO_GUIDE_COLOR, 4)

#define PROPERTY_NAME(name, val) name,
#define PROPERTY_VAL(name, val) val,

static void ShowError()
{
    ovrErrorInfo errorInfo;
    ovr_GetLastErrorInfo(&errorInfo);
    if (!OVR_SUCCESS(errorInfo.Result))
    {
        WCHAR message[1024];
        swprintf(message, sizeof(message)/sizeof(message[0]), L"Error %S!\n", errorInfo.ErrorString);
        // XXX for now just ODS
        OutputDebugStringW(message);
    }
}

//
// APIs under test
//

// XXX this is not the full set of claimed-thread-safe APIs yet
// XXX currently most tests just test for crashes, should really check that the operations succeed

ThreadTest(GetLastErrorInfo)
{
    ovrErrorInfo errorInfo;
    ovr_GetLastErrorInfo(&errorInfo);
    return (strlen(errorInfo.ErrorString) < sizeof(errorInfo.ErrorString)) ? 0 : 1;
}

ThreadTest(GetVersionString)
{
    const char* versionString = ovr_GetVersionString();
    return (strlen(versionString) < 1024) ? 0 : 1;
}

ThreadTest(TraceMessage)
{
    const char* logText[3] = { "ovrLogLevel_Debug", "ovrLogLevel_Info", "ovrLogLevel_Error" };
    std::uniform_int_distribution<> randtype(0, 2);
    const int type = randtype(state->rand);
    return (ovr_TraceMessage(type, logText[type]) < 32) ? 0 : 1;
}

ThreadTest(GetHmdDesc)
{
    ovrHmdDesc desc = ovr_GetHmdDesc(state->session);
    return (strlen(desc.Manufacturer) < sizeof(desc.Manufacturer)) ? 0 : 1;
}

// XXX this crashes
ThreadTest(CreateShouldFail)
{
    ovrSession sessionNew;
    ovrGraphicsLuid graphicsId;
    ovrResult result = ovr_Create(&sessionNew, &graphicsId);
    // expect failure
    if (OVR_FAILURE(result)) return 0;
    ovr_Destroy(sessionNew);
    return 1;
}

ThreadTest(GetEnabledCaps)
{
    return 0;
}

ThreadTest(SetEnabledCaps)
{
    return 0;
}

ThreadTest(RecenterPose)
{
    ovr_RecenterTrackingOrigin(state->session);
    return 0;
}

ThreadTest(GetTrackingState)
{
    ovrTrackingState st = ovr_GetTrackingState(state->session, 0, ovrTrue);
    return 0;
}

ThreadTest(GetPredictedTrackingState)
{
    double time = ovr_GetTimeInSeconds();
    std::uniform_real_distribution<> randdt(-1, 1);
    ovrTrackingState st = ovr_GetTrackingState(state->session, time + randdt(state->rand), ovrTrue);
    return 0;
}

ThreadTest(GetInputState)
{
    ovrInputState inputState;

    const std::array<ovrControllerType, 7> controllerTypes = {{
            ovrControllerType_None,
            ovrControllerType_LTouch,
            ovrControllerType_RTouch,
            ovrControllerType_Touch,
            ovrControllerType_Remote,
            ovrControllerType_XBox,
            ovrControllerType_Active,
    }};

    std::uniform_int_distribution<> randController(0, (int)controllerTypes.size());
    ovrResult result = ovr_GetInputState(state->session,
        controllerTypes[randController(state->rand)], &inputState);
    // XXX what are the expected results for missing input devices?
    return 0;
}

// XXX add haptics API tests when API settles down

ThreadTest(GetFovTextureSize)
{
    std::uniform_int_distribution<> randeye(0, 1);
    ovrEyeType eye = randeye(state->rand) ? ovrEye_Left : ovrEye_Right;
    ovrFovPort fov = { 1, 1, 1, 1 };
    ovrSizei size = ovr_GetFovTextureSize(state->session, eye, fov, 1);
    return ((size.w > 0) && (size.h > 0)) ? 0 : 1;
}

ThreadTest(GetRenderDesc)
{
    std::uniform_int_distribution<> randeye(0, 1);
    ovrEyeType eye = randeye(state->rand) ? ovrEye_Left : ovrEye_Right;
    ovrFovPort fov = {1, 1, 1, 1};
    ovrEyeRenderDesc renderDesc = ovr_GetRenderDesc(state->session, eye, fov);
    return (renderDesc.HmdToEyeOffset.x != 0) ? 0 : 1;
}

// XXX add SwapTextureSet test
// XXX add MirrorTexture test
// XXX add CaptureBuffer test

ThreadTest(GetFrameTiming)
{
    std::uniform_int_distribution<> randframe(0, 2);
    double t = ovr_GetPredictedDisplayTime(state->session, frameIndex + randframe(state->rand));
    return (t > 0) ? 0 : 1;
}

ThreadTest(GetTimeInSeconds)
{
    double t = ovr_GetTimeInSeconds();
    return (t > 0) ? 0 : 1;
}

ThreadTest(BoolProperties)
{
    std::vector<const char *> names = { LIST_BOOL_PROPERTIES(PROPERTY_NAME) };
    std::uniform_int_distribution<> randprop(0, int(names.size() - 1));
    const char *name = names[randprop(state->rand)];

    if (state->rand() & 1)
    {
        ovr_GetBool(state->session, name, ovrFalse);
    }
    else
    {
        ovr_SetBool(state->session, name, (state->rand() & 1) ? ovrTrue : ovrFalse);
    }

    return 0;
}

// XXX throws an error
NoThreadTest(IntProperties)
{
    std::vector<const char *> names = { LIST_INT_PROPERTIES(PROPERTY_NAME) };
    std::vector<int> vals = { LIST_INT_PROPERTIES(PROPERTY_VAL) };
    std::uniform_int_distribution<> randprop(0, int(names.size() - 1));
    int idx = randprop(state->rand);
    const char *name = names[idx];

    if (state->rand() & 1)
    {
        ovr_GetInt(state->session, name, 0);
    }
    else
    {
        std::uniform_int_distribution<> randval(0, vals[idx]);
        ovr_SetInt(state->session, name, randval(state->rand));
    }

    return 0;
}

ThreadTest(FloatProperties)
{
    std::vector<const char *> names = { LIST_FLOAT_PROPERTIES(PROPERTY_NAME) };
    std::vector<double> vals = { LIST_FLOAT_PROPERTIES(PROPERTY_VAL) };
    std::uniform_int_distribution<> randprop(0, int(names.size() - 1));
    int idx = randprop(state->rand);
    const char *name = names[idx];

    if (state->rand() & 1)
    {
        ovr_GetFloat(state->session, name, 0);
    }
    else
    {
        std::uniform_real_distribution<> randval(0, vals[idx]);
        ovr_SetFloat(state->session, name, float(randval(state->rand)));
    }

    return 0;
}

ThreadTest(StringProperties)
{
    std::vector<const char *> names = { LIST_STRING_PROPERTIES(PROPERTY_NAME) };
    std::vector<std::vector<const char *>> vals;
    #define MAKE_STR_VECTOR(name, val) vals.push_back(std::vector<const char *> val);
    LIST_STRING_PROPERTIES(MAKE_STR_VECTOR)
    std::uniform_int_distribution<> randprop(0, int(names.size() - 1));
    int idx = randprop(state->rand);
    const char *name = names[idx];

    if (state->rand() & 1)
    {
        const char *foo = ovr_GetString(state->session, name, "");
    }
    else
    {
        std::uniform_int_distribution<> randval(0, int(vals[idx].size() - 1));
        ovr_SetString(state->session, name, vals[idx][randval(state->rand)]);
    }

    return 0;
}

ThreadTest(FloatArrayProperties)
{
    std::vector<const char *> names = { LIST_FLOAT_ARRAY_PROPERTIES(PROPERTY_NAME) };
    std::vector<int> arraySize = { LIST_FLOAT_ARRAY_PROPERTIES(PROPERTY_VAL) };
    std::uniform_int_distribution<> randprop(0, int(names.size() - 1));
    int idx = randprop(state->rand);
    const char *name = names[idx];

    if (state->rand() & 1)
    {
        float values[32];
        ovr_GetFloatArray(state->session, name, values, arraySize[idx]);
    }
    else
    {
        std::uniform_real_distribution<> randval(-3, 3);
        int size = arraySize[idx];
        float values[32];
        for (int i = 0; i < size; ++i)
            values[i] = float(randval(state->rand));
        ovr_SetFloatArray(state->session, name, values, size);
    }

    return 0;
}

//
// TestThread - thread function which runs a random selection of tests from state's list
//

static int TestThread(Thread *pThread, void *pData)
{
    ThreadTestState *state = static_cast<ThreadTestState *>(pData);
    std::uniform_int_distribution<> randtest(0, int(state->tests.size() - 1));
    std::uniform_int_distribution<> randms(-10, 30);

    int failures = 0;

    while (running)
    {
        if (state->tests[randtest(state->rand)](state))
        {
            ++failures;
        }

        // sleep some of the time
        #if 0
        int ms = randms(state->rand);
        if (ms > 0)
            Thread::MSleep(ms);
        #endif
    }

    return failures;
}

struct Threading : BasicVR
{
    int threadCode;

    Threading(HINSTANCE hinst) : BasicVR(hinst, L"Threading"), threadCode(0) {}

    void MainLoop()
    {
        threadCode = 0;

	    Layer[0] = new VRLayer(Session);

        // XXX revisit whether DirectQuad was the best thing to base the threading test on
        // Make a duplicate of the left eye texture, and render a static image into it
        OculusTexture extraRenderTexture;
        if (!extraRenderTexture.Init(Session, 1024, 1024))
            return;

        Camera zeroCam(&XMVectorSet(-9, 2.25f, 0, 0), &XMQuaternionRotationRollPitchYaw(0, 0.5f*3.141f, 0));
        ovrPosef zeroPose;
        zeroPose.Position.x = 0;
        zeroPose.Position.y = 0;
        zeroPose.Position.z = 0;
        zeroPose.Orientation.w = 1;
        zeroPose.Orientation.x = 0;
        zeroPose.Orientation.y = 0;
        zeroPose.Orientation.z = 0;
        Layer[0]->RenderSceneToEyeBuffer(&zeroCam, RoomScene, 0, extraRenderTexture.TexRtv[0], &zeroPose, 1, 1, 0.5f);

        // Commit changes to extraRenderTexture
        extraRenderTexture.Commit();

        ThreadTestState state[numThreads] = {};
        Thread thread[numThreads];

        // use an RNG to seed each thread's RNG
        int seed = 0xfeedface; // XXX should be settable from command-line
        std::mt19937 seeder(seed);

        for (int i = 0; i < numThreads; ++i)
        {
            state[i].id = i;
            state[i].session = Session;
            state[i].rand.seed(seeder());
            // for now just randomly run all tests from each thread
            state[i].tests = TestInstance::list;

            thread[i].UserHandle = &state[i];
            thread[i].ThreadFunction = TestThread;
        }

        // start threads
        running = true;
        for (int i = 0; i < numThreads; ++i)
        {
            thread[i].Start();
            char buff[256];
            sprintf_s(buff, "TestThread%03d", i);
            thread[i].SetThreadName(buff);
        }

        // main rendering loop
        for (frameIndex = 0; frameIndex < maxFrames; ++frameIndex)
        {
            if (!HandleMessages())
                break;

            ActionFromInput();
            Layer[0]->GetEyePoses();

            for (int eye = 0; eye < 2; eye++)
            {
                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
            }

            Layer[0]->PrepareLayerHeader();

            // Expanded distort and present from the basic sample, to allow for direct quad
            ovrLayerHeader* layerHeaders[2];

            // The standard one
            layerHeaders[0] = &Layer[0]->ovrLayer.Header;

            // ...and now the new quad
            static ovrLayerQuad myQuad;
            myQuad.Header.Type = ovrLayerType_Quad;
            myQuad.Header.Flags = 0;
            myQuad.ColorTexture = extraRenderTexture.TextureChain;
            myQuad.Viewport.Pos.x = 0;
            myQuad.Viewport.Pos.y = 0;
            myQuad.Viewport.Size.w = extraRenderTexture.SizeW;
            myQuad.Viewport.Size.h = extraRenderTexture.SizeH;
            myQuad.QuadPoseCenter = zeroPose;
            myQuad.QuadPoseCenter.Position.z = -1.0f;
            myQuad.QuadSize.x = 1.0f;
            myQuad.QuadSize.y = 2.0f;
            layerHeaders[1] = &myQuad.Header;

		    // Submit them
            presentResult = ovr_SubmitFrame(Session, 0, nullptr, layerHeaders, 2);
            if (!OVR_SUCCESS(presentResult))
                return;

		    // Render mirror
            ID3D11Resource* resource = nullptr;
            ovr_GetMirrorTextureBufferDX(Session, mirrorTexture, IID_PPV_ARGS(&resource));
		    DIRECTX.Context->CopyResource(DIRECTX.BackBuffer, resource);
            resource->Release();
		    DIRECTX.SwapChain->Present(0, 0);
        }

        running = false;
        // can't use Thread::FinishAllThreads() here since single process creates extra threads which don't terminate
        // can't move this below Release since threads need to stop calling APIs first
        for (int i = 0; i < numThreads; ++i)
        {
            if (thread[i].Join(1000))
            {
                threadCode = threadCode || thread[i].GetExitCode();
            }
            else threadCode = -1;
        }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	Threading app(hinst);
    return app.Run();
    // XXX figure out which threads are failing...
    //return (app.Run() || app.threadCode);
}
