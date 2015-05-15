/************************************************************************************

Filename    :   OVR_Stereo.h
Content     :   Stereo rendering functions
Created     :   November 30, 2013
Authors     :   Tom Fosyth

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_Stereo_h
#define OVR_Stereo_h

#include "Sensors/OVR_DeviceConstants.h"
#include "Vision/SensorFusion/Vision_SensorStateReader.h"
#include "OVR_Profile.h"
#include "OVR_CAPI.h" // ovrDistortionMesh
#include "OVR_StereoProjection.h"


// CAPI Forward declaration.
typedef struct ovrFovPort_ ovrFovPort;
typedef struct ovrRecti_ ovrRecti;

namespace OVR {

class SensorDevice; // Opaque forward declaration


//-----------------------------------------------------------------------------------
// *****  Distortion Capability Flags

// These distortion flags used to be reported for Hmd and configured through ovrHmd_ConfigureRendering.
// With ConfigureRendering gone, there is no longer place for these in public API.
// We are now using the ovrDistortionCap_Default instead.
// 
// Moving here and keeping 'ovr' prefix for now to avoid extra change until their fate is fully decided. 
// Some may be re-exposed as debug flags.
//
// We do have some agreement on keeping a few of these internally:  Vignette, Overdrive and DisableChromatic
// are planned to be exposed through ConfigUtil user options.
//

typedef enum ovrDistortionCaps_
{
    // 0x01 unused - Previously ovrDistortionCap_Chromatic. Use ovrDistortionCap_DisableChromatic to explicitly disable
    ovrDistortionCap_TimeWarp           = 0x02,     ///< Supports timewarp.
    // 0x04 unused

    /// \brief Supports vignetting around the edges of the view.
    /// \details Vignette adds a fade to the edges of the display for each eye, instead of a harsh cutoff
    ovrDistortionCap_Vignette           =    0x08,    
    ovrDistortionCap_SRGB               =    0x40,     ///< Assume input images are in sRGB gamma-corrected color space. This is not supported on D3D. May change in a future revision.
    /// \brief Overdrive brightness transitions to reduce artifacts on DK2+ displays
    /// \details <I>This option has a slight GPU cost, but by default should probably be enabled 99% of the time.</I>
    ovrDistortionCap_Overdrive          =    0x80,

    ovrDistortionCap_ComputeShader      =   0x400,     ///< \internal Using compute shader for timewarp and distortion. (DX11+ only)
    //ovrDistortionCap_NoTimewarpJit    =   0x800      RETIRED - do not reuse this bit without major versioning changes.
    /// \brief Enables a spin-wait that tries to push time-warp to be as close to V-sync as possible. WARNING - this may backfire and cause framerate loss - use with caution.
    /// \details Default to off, recommended do not use due to inaccuracies in the used timing and waiting methods employed in Windows.
    ovrDistortionCap_TimewarpJitDelay   =  0x1000,
    ovrDistortionCap_DisableChromatic   =  0x2000,     ///< Disables de-chromatic aberration in distortion pass w/o perf improvements (useful for debugging visuals outside of the HMD)

    ovrDistortionCap_ProfileNoSpinWaits = 0x10000,     ///< \deprecated Use when profiling with timewarp to remove false positives
    ovrDistortionCap_EnumSize           = 0x7fffffff,   ///< @internal Force type int32_t.

    // Default values passed now that DistortionCaps are no longer public.
    // Always use Timewarp and Overdrive
    ovrDistortionCap_Default            = ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive | ovrDistortionCap_Vignette

} ovrDistortionCaps;



//-----------------------------------------------------------------------------------
// ***** Misc. utility functions.

// Inputs are 4 points (pFitX[0],pFitY[0]) through (pFitX[3],pFitY[3])
// Result is four coefficients in pResults[0] through pResults[3] such that
//      y = pResult[0] + x * ( pResult[1] + x * ( pResult[2] + x * ( pResult[3] ) ) );
// passes through all four input points.
// Return is true if it succeeded, false if it failed (because two control points
// have the same pFitX value).
bool FitCubicPolynomial ( float *pResult, const float *pFitX, const float *pFitY );

//-----------------------------------------------------------------------------------
// ***** LensConfig

// LensConfig describes the configuration of a single lens in an HMD.
// - Eqn and K[] describe a distortion function.
// - MetersPerTanAngleAtCenter is the relationship between distance on a
//   screen (at the center of the lens), and the angle variance of the light after it
//   has passed through the lens.
// - ChromaticAberration is an array of parameters for controlling
//   additional Red and Blue scaling in order to reduce chromatic aberration
//   caused by the Rift lenses.
struct LensConfig
{
    LensConfig()
      : Eqn(Distortion_CatmullRom10)
      //K()
      , MaxR(0.0f)
      , MetersPerTanAngleAtCenter(0.0f)
      //ChromaticAberration()
      //InvK()
      , MaxInvR(0.0f)
    {
        memset(&K, 0, sizeof(K));
        memset(&ChromaticAberration, 0, sizeof(ChromaticAberration));
        memset(&InvK, 0, sizeof(InvK));
    }
    
    // The result is a scaling applied to the distance from the center of the lens.
    float    DistortionFnScaleRadiusSquared (float rsq) const;
    // x,y,z components map to r,g,b scales.
    Vector3f DistortionFnScaleRadiusSquaredChroma (float rsq) const;

    // DistortionFn applies distortion to the argument.
    // Input: the distance in TanAngle/NIC space from the optical center to the input pixel.
    // Output: the resulting distance after distortion.
    float DistortionFn(float r) const
    {
        return r * DistortionFnScaleRadiusSquared ( r * r );
    }

    // DistortionFnInverse computes the inverse of the distortion function on an argument.
    float DistortionFnInverse(float r) const;

    // Also computes the inverse, but using a polynomial approximation. Warning - it's just an approximation!
    float DistortionFnInverseApprox(float r) const;
    // Sets up InvK[].
    void SetUpInverseApprox();

    // Sets a bunch of sensible defaults.
    void SetToIdentity();



    enum { NumCoefficients = 11 };

    DistortionEqnType   Eqn;
    float               K[NumCoefficients];
    float               MaxR;       // The highest R you're going to query for - the curve is unpredictable beyond it.

    float               MetersPerTanAngleAtCenter;

    // Additional per-channel scaling is applied after distortion:
    //  Index [0] - Red channel constant coefficient.
    //  Index [1] - Red channel r^2 coefficient.
    //  Index [2] - Blue channel constant coefficient.
    //  Index [3] - Blue channel r^2 coefficient.
    float               ChromaticAberration[4];

    float               InvK[NumCoefficients];
    float               MaxInvR;
};


// For internal use - storing and loading lens config data

// Returns true on success.
bool LoadLensConfig ( LensConfig *presult, uint8_t const *pbuffer, int bufferSizeInBytes );

// Returns number of bytes needed.
int SaveLensConfigSizeInBytes ( LensConfig const &config );
// Returns true on success.
bool SaveLensConfig ( uint8_t *pbuffer, int bufferSizeInBytes, LensConfig const &config );


//-----------------------------------------------------------------------------------
// ***** DistortionRenderDesc

// This describes distortion for a single eye in an HMD with a display, not just the lens by itself.
struct DistortionRenderDesc
{
    // The raw lens values.
    LensConfig          Lens;

    // These map from [-1,1] across the eye being rendered into TanEyeAngle space (but still distorted)
    Vector2f            LensCenter;
    Vector2f            TanEyeAngleScale;
    // Computed from device characteristics, IPD and eye-relief.
    // (not directly used for rendering, but very useful)
    Vector2f            PixelsPerTanAngleAtCenter;
};


//-------------------------------------------------------------------------------------
// ***** HMDInfo 

// This structure describes various aspects of the HMD allowing us to configure rendering.
//
//  Currently included data:
//   - Physical screen dimensions, resolution, and eye distances.
//     (some of these will be configurable with a tool in the future).
//     These arguments allow us to properly setup projection across HMDs.
//   - DisplayDeviceName for identifying HMD screen; system-specific interpretation.
//
// TBD:
//  - Power on/ off?
//  - Sensor rates and capabilities
//  - Distortion radius/variables    
//  - Screen update frequency
//  - Distortion needed flag
//  - Update modes:
//      Set update mode: Stereo (both sides together), mono (same in both eyes),
//                       Alternating, Alternating scan-lines.

// Oculus VR Display Driver Shim Information
struct ExtraMonitorInfo
{
	int DeviceNumber;
	int NativeWidth;
	int NativeHeight;
	int Rotation;
	int UseMirroring;

	ExtraMonitorInfo() :
		DeviceNumber(0),
		NativeWidth(1920),
		NativeHeight(1080),
		Rotation(0),
		UseMirroring(1)
	{
	}
};

class HMDInfo
{
public:
	// Name string describing the product: "Oculus Rift DK1", etc.
	String      ProductName;
	String      Manufacturer;

	unsigned    Version;

	// Characteristics of the HMD screen and enclosure
	HmdTypeEnum HmdType;
    bool        DebugDevice;                    // Indicates if the HMD is a virtual debug device, such as when there is no HMD present.
	Size<int>   ResolutionInPixels;
	Size<float> ScreenSizeInMeters;
	float       ScreenGapSizeInMeters;
	float       CenterFromTopInMeters;
	float       LensSeparationInMeters;
    Vector2f    PelOffsetR;                     // Offsets from the green pel in pixels (i.e. usual values are 0.5 or 0.333)
    Vector2f    PelOffsetB;

	// Timing & shutter data. All values in seconds.
	struct ShutterInfo
	{
		HmdShutterTypeEnum  Type;
		float   VsyncToNextVsync;                // 1/framerate
		float   VsyncToFirstScanline;            // for global shutter, vsync->shutter open.
		float   FirstScanlineToLastScanline;     // for global shutter, will be zero.
		float   PixelSettleTime;                 // estimated.
		float   PixelPersistence;                // Full persistence = 1/framerate.
	}           Shutter;

	// Desktop coordinate position of the screen (can be negative; may not be present on all platforms)
	int         DesktopX;
	int         DesktopY;

	// Windows:
	// "\\\\.\\DISPLAY3", etc. Can be used in EnumDisplaySettings/CreateDC.
	String      DisplayDeviceName;
	ExtraMonitorInfo ShimInfo;

	// MacOS:
	int         DisplayId;

	bool	    InCompatibilityMode;

	// Printed serial number for the HMD; should match external sticker
    String      PrintedSerial;

    // Tracker descriptor information:
    int         VendorId;
    int         ProductId;
    int         FirmwareMajor;
    int         FirmwareMinor;

    float   CameraFrustumHFovInRadians;
    float   CameraFrustumVFovInRadians;
    float   CameraFrustumNearZInMeters;
    float   CameraFrustumFarZInMeters;

	// Constructor initializes all values to 0s.
	// To create a "virtualized" HMDInfo, use CreateDebugHMDInfo instead.
	HMDInfo() :
		ProductName(),
        Manufacturer(),
        Version(0),
		HmdType(HmdType_None),
        DebugDevice(false),
		ResolutionInPixels(0),
		ScreenSizeInMeters(0.0f),
		ScreenGapSizeInMeters(0.0f),
		CenterFromTopInMeters(0),
		LensSeparationInMeters(0),
        PelOffsetR(0.0f,0.0f),
        PelOffsetB(0.0f,0.0f),
      //Shutter (initialized below)
		DesktopX(0),
		DesktopY(0),
        DisplayDeviceName(),
        ShimInfo(),
		DisplayId(-1),
		InCompatibilityMode(false),
        PrintedSerial(),
        VendorId(-1),
        ProductId(-1),
        FirmwareMajor(-1),
        FirmwareMinor(-1),
        CameraFrustumHFovInRadians(0.0f),
        CameraFrustumVFovInRadians(0.0f),
        CameraFrustumNearZInMeters(0.0f),
        CameraFrustumFarZInMeters(0.0f)
	{
		Shutter.Type = HmdShutter_LAST;
		Shutter.VsyncToNextVsync = 0.0f;
		Shutter.VsyncToFirstScanline = 0.0f;
		Shutter.FirstScanlineToLastScanline = 0.0f;
		Shutter.PixelSettleTime = 0.0f;
		Shutter.PixelPersistence = 0.0f;
    }

	// Operator = copies local fields only (base class must be correct already)
	void operator=(const HMDInfo& src)
	{
		ProductName = src.ProductName;
		Manufacturer = src.Manufacturer;
		Version = src.Version;
		HmdType = src.HmdType;
        DebugDevice = src.DebugDevice;
		ResolutionInPixels = src.ResolutionInPixels;
		ScreenSizeInMeters = src.ScreenSizeInMeters;
		ScreenGapSizeInMeters = src.ScreenGapSizeInMeters;
		CenterFromTopInMeters = src.CenterFromTopInMeters;
		LensSeparationInMeters = src.LensSeparationInMeters;
        PelOffsetR = src.PelOffsetR;
        PelOffsetB = src.PelOffsetB;
		DesktopX = src.DesktopX;
		DesktopY = src.DesktopY;
		Shutter = src.Shutter;
		DisplayDeviceName = src.DisplayDeviceName;
		ShimInfo = src.ShimInfo;
		DisplayId = src.DisplayId;
		InCompatibilityMode = src.InCompatibilityMode;
        VendorId = src.VendorId;
        ProductId = src.ProductId;
        FirmwareMajor = src.FirmwareMajor;
        FirmwareMinor = src.FirmwareMinor;
        PrintedSerial = src.PrintedSerial;
        CameraFrustumHFovInRadians = src.CameraFrustumHFovInRadians;
        CameraFrustumVFovInRadians = src.CameraFrustumVFovInRadians;
        CameraFrustumNearZInMeters = src.CameraFrustumNearZInMeters;
        CameraFrustumFarZInMeters = src.CameraFrustumFarZInMeters;
    }

	void SetScreenParameters(int hres, int vres,
							 float hsize, float vsize,
							 float vCenterFromTopInMeters, float lensSeparationInMeters,
							 bool compatibilityMode)
	{
		ResolutionInPixels = Sizei(hres, vres);
		ScreenSizeInMeters = Sizef(hsize, vsize);
		CenterFromTopInMeters = vCenterFromTopInMeters;
		LensSeparationInMeters = lensSeparationInMeters;
		InCompatibilityMode = compatibilityMode;
	}

	bool IsSameDisplay(const HMDInfo& o) const
	{
		return DisplayId == o.DisplayId &&
			DisplayDeviceName.CompareNoCase(o.DisplayDeviceName) == 0;
	}
};


//-----------------------------------------------------------------------------------
// ***** HmdRenderInfo

// All the parts of the HMD info that are needed to set up the rendering system.

struct HmdRenderInfo
{
    // The start of this structure is intentionally very similar to HMDInfo in OVER_Device.h
    // However to reduce interdependencies, one does not simply #include the other.

    HmdTypeEnum HmdType;

    // Size of the entire screen
    Size<int>   ResolutionInPixels;
    Size<float> ScreenSizeInMeters;
    float       ScreenGapSizeInMeters;
    Vector2f    PelOffsetR;                     // Offsets from the green pel in pixels (i.e. usual values are 0.5 or 0.333)
    Vector2f    PelOffsetB;

    // Display is rotated 0/90/180/270 degrees counter-clockwise?
    int         Rotation;

    // Some displays scan out in different directions, so this flag can be used to change
    // where we render the latency test pixel.
    bool        OffsetLatencyTester;

    // Characteristics of the lenses.
    float       CenterFromTopInMeters;
    float       LensSeparationInMeters;
    float       LensDiameterInMeters;
    float       LensSurfaceToMidplateInMeters;
    EyeCupType  EyeCups;

    // Timing & shutter data. All values in seconds.
    struct ShutterInfo
    {
        HmdShutterTypeEnum  Type;
        float               VsyncToNextVsync;                // 1/framerate
        float               VsyncToFirstScanline;            // for global shutter, vsync->shutter open.
        float               FirstScanlineToLastScanline;     // for global shutter, will be zero.
        float               PixelSettleTime;                 // estimated.
        float               PixelPersistence;                // Full persistence = 1/framerate.
    } Shutter;


    // These are all set from the user's profile.
    struct EyeConfig
    {
        // Distance from center of eyeball to front plane of lens.
        float               ReliefInMeters;
        // Distance from nose (technically, center of Rift) to the middle of the eye.
        float               NoseToPupilInMeters;

        LensConfig          Distortion;
    } EyeLeft, EyeRight;


    HmdRenderInfo()
    {
        HmdType = HmdType_None;
        ResolutionInPixels.w = 0;
        ResolutionInPixels.h = 0;
        ScreenSizeInMeters.w = 0.0f;
        ScreenSizeInMeters.h = 0.0f;
        ScreenGapSizeInMeters = 0.0f;
        CenterFromTopInMeters = 0.0f;
        LensSeparationInMeters = 0.0f;
        LensDiameterInMeters = 0.0f;
        LensSurfaceToMidplateInMeters = 0.0f;
        PelOffsetR = Vector2f ( 0.0f, 0.0f );
        PelOffsetB = Vector2f ( 0.0f, 0.0f );
        Rotation = 0;
        OffsetLatencyTester = false;
        Shutter.Type = HmdShutter_LAST;
        Shutter.VsyncToNextVsync = 0.0f;
        Shutter.VsyncToFirstScanline = 0.0f;
        Shutter.FirstScanlineToLastScanline = 0.0f;
        Shutter.PixelSettleTime = 0.0f;
        Shutter.PixelPersistence = 0.0f;
        EyeCups = EyeCup_DK1A;
        EyeLeft.ReliefInMeters = 0.0f;
        EyeLeft.NoseToPupilInMeters = 0.0f;
        EyeLeft.Distortion.SetToIdentity();
        EyeRight = EyeLeft;
    }

    // The "center eye" is the position the HMD tracking returns,
    // and games will also usually use it for audio, aiming reticles, some line-of-sight tests, etc.
    EyeConfig GetEyeCenter() const
    {
        EyeConfig result;
        result.ReliefInMeters = 0.5f * ( EyeLeft.ReliefInMeters + EyeRight.ReliefInMeters );
        result.NoseToPupilInMeters = 0.0f;
        result.Distortion.SetToIdentity();
        return result;
    }

};


//-----------------------------------------------------------------------------
// ProfileRenderInfo
//
// Render-related information from the user profile.
struct ProfileRenderInfo
{
    // Type of eye cup on the headset
    // ie. "A", "Orange A"
    String EyeCupType;

    // IPD/2 offset for each eye
    float Eye2Nose[2];

    // Eye to plate distance for each eye
    float Eye2Plate[2];

    // Eye relief dial
    int EyeReliefDial;

    //Profile option to disable the HSW
    bool  HSWDisabled;


    ProfileRenderInfo();
};



//-----------------------------------------------------------------------------------
// A description of a layer.
//
// This does not include the texture pointers/IDs/etc since everything that uses
// this description uses a slightly different format for them. They are here in comment form only.
// If you change this, update IPCCompositorSubmitLayersParams::Serialize()
struct LayerDesc
{
    // To consider: just use ovrLayerType directly instead of LayerType, as they match 1:1.
    enum LayerType
    {
        LayerType_Disabled       = ovrLayerType_Disabled,         ///< Layer is disabled.
        LayerType_Fov            = ovrLayerType_EyeFov,           ///< Standard rendered 3D view - usually stereo.
        LayerType_FovWithDepth   = ovrLayerType_EyeFovDepth,      ///< Rendered 3D view with depth buffer - usually stereo.
        LayerType_QuadInWorld    = ovrLayerType_QuadInWorld,      ///< Arbitrarily-positioned quad - usually mono. Pos+orn specified in "zero pose" space.
        LayerType_QuadHeadLocked = ovrLayerType_QuadHeadLocked,   ///< Quad in face space. Pos+orn specified in current HMD space - moves/TW with HMD.
        LayerType_Direct         = ovrLayerType_Direct,           ///< Drawn directly to the HMD, no distortion, CA, timewarp.
        // TODO: cubemap layer?

        LayerType_COUNT // Always last.
    };

    enum QualityType
    {
        QualityType_Normal,         // Single sample.
        QualityType_EWA,            // 7-tap EWA

        QualityType_COUNT,
    };

    LayerDesc()
        : Type(LayerType_Fov)
        , Quality(QualityType_Normal)
        , bAnisoFiltering(false)
        , bTextureOriginAtBottomLeft(false)
    {}

    LayerType           Type;
    QualityType         Quality;
    bool                bAnisoFiltering;                // otherwise trilinear

    bool                bTextureOriginAtBottomLeft;     // Generally false for D3D, true for OpenGL.
    ovrSizei            EyeTextureSize[2];
    ovrRecti            EyeRenderViewport[2];
    ovrFovPort          EyeRenderFovPort[2];
    ovrPosef            EyeRenderPose[2];               // quadCenterPose in the case of LayerType_HangingQuad
    ovrVector2f         QuadSize[2];                    // for LayerType_HangingQuad
    ovrTimewarpProjectionDesc ProjectionDesc;           // for LayerType_FovWithDepth

    // TODO: motion vectors.

    // Used ONLY on client side to specify which texture set should be used.
    ovrSwapTextureSet const *  pEyeTextureSets[2];      // NOTE - both texture sets may be the same.
    ovrSwapTextureSet const *  pEyeDepthTextureSets[2]; // NOTE - both texture sets may be the same.

    void SetToDisabled()
    {
        Type                        = LayerDesc::LayerType_Disabled;
        bTextureOriginAtBottomLeft  = false;
        bAnisoFiltering             = false;
        Quality                     = LayerDesc::QualityType_Normal;
        ProjectionDesc.Projection22 = 0.0f;
        ProjectionDesc.Projection23 = 0.0f;
        ProjectionDesc.Projection32 = 0.0f;
        for (int eyeId = 0; eyeId < 2; eyeId++)
        {
            EyeTextureSize[eyeId]       = ovrSizei();
            EyeRenderViewport[eyeId]    = ovrRecti();
            QuadSize[eyeId]             = ovrVector2f();
            EyeRenderPose[eyeId]        = Posef();
            EyeRenderFovPort[eyeId]     = FovPort();
            pEyeTextureSets[eyeId]      = nullptr;
            pEyeDepthTextureSets[eyeId] = nullptr;
        }
    }

};

struct DistortionRendererLayerDesc
{
    int LayerNum;
    OVR::LayerDesc Desc;

    // Used ONLY by server side after resolving the texture set to an actual texture
    ovrTexture *  pEyeTextures[2];        // NOTE - both textures may be the same.
    ovrTexture *  pEyeDepthTextures[2];   // NOTE - both textures may be the same.

    DistortionRendererLayerDesc() :
        LayerNum(0),
        Desc()
    {
        SetToDisabled();
    }

    void SetToDisabled()
    {
        Desc.SetToDisabled();
        for (int eyeId = 0; eyeId < 2; eyeId++)
        {
            pEyeTextures[eyeId]      = nullptr;
            pEyeDepthTextures[eyeId] = nullptr;
        }
    }
};


enum {
    // Arbitrary number - all this does is control the size of some arrays - rendering will multi-pass as needed.
    MaxNumLayersTotal = 33,
    // HSW always lives in the last one.
    HswLayerNum = MaxNumLayersTotal - 1,
    // ...and we don't tell people that layer exists.
    MaxNumLayersPublic = MaxNumLayersTotal - 1
};



//-----------------------------------------------------------------------------------

// Stateless computation functions, in somewhat recommended execution order.
// For examples on how to use many of them, see the StereoConfig::UpdateComputedState function.

const float OVR_DEFAULT_EXTRA_EYE_ROTATION = 30.0f * MATH_FLOAT_DEGREETORADFACTOR;

// Creates a dummy debug HMDInfo matching a particular HMD model.
// Useful for development without an actual HMD attached.
HMDInfo             CreateDebugHMDInfo(HmdTypeEnum hmdType);

// Fills in a render info object from a user Profile object.
// It may need to fill in some defaults, so it also takes in the HMD type.
ProfileRenderInfo   GenerateProfileRenderInfoFromProfile( HMDInfo const& hmdInfo,
                                                          Profile const* profile );

// profile may be NULL, in which case it uses the hard-coded defaults.
// distortionType should be left at the default unless you require something specific for your distortion shaders.
// eyeCupOverride can be EyeCup_LAST, in which case it uses the one in the profile.
HmdRenderInfo       GenerateHmdRenderInfoFromHmdInfo ( HMDInfo const &hmdInfo,
                                                       ProfileRenderInfo const& profileRenderInfo,
                                                       DistortionEqnType distortionType = Distortion_CatmullRom10,
                                                       EyeCupType eyeCupOverride = EyeCup_LAST );

LensConfig          GenerateLensConfigFromEyeRelief ( float eyeReliefInMeters, HmdRenderInfo const &hmd,
                                                      DistortionEqnType distortionType = Distortion_CatmullRom10 );

DistortionRenderDesc CalculateDistortionRenderDesc ( StereoEye eyeType, HmdRenderInfo const &hmd,
                                                     LensConfig const *pLensOverride = NULL );

FovPort             CalculateFovFromEyePosition ( float eyeReliefInMeters,
                                                  float offsetToRightInMeters,
                                                  float offsetDownwardsInMeters,
                                                  float lensDiameterInMeters,
                                                  float extraEyeRotationInRadians = OVR_DEFAULT_EXTRA_EYE_ROTATION);

FovPort             CalculateFovFromHmdInfo ( StereoEye eyeType,
                                              DistortionRenderDesc const &distortion,
                                              HmdRenderInfo const &hmd,
                                              float extraEyeRotationInRadians = OVR_DEFAULT_EXTRA_EYE_ROTATION );

FovPort             GetPhysicalScreenFov ( StereoEye eyeType, DistortionRenderDesc const &distortion );

FovPort             GetPhysicalScreenDiagonalFov(StereoEye eyeType, DistortionRenderDesc const &distortion);

FovPort             ClampToPhysicalScreenFov ( StereoEye eyeType, DistortionRenderDesc const &distortion,
                                               FovPort inputFovPort );

Sizei               CalculateIdealPixelSize ( StereoEye eyeType, DistortionRenderDesc const &distortion,
                                              FovPort fov, float pixelsPerDisplayPixel );

Recti               GetFramebufferViewport ( StereoEye eyeType, HmdRenderInfo const &hmd );

ScaleAndOffset2D    CreateUVScaleAndOffsetfromNDCScaleandOffset ( ScaleAndOffset2D scaleAndOffsetNDC,
                                                                  Recti renderedViewport,
                                                                  Sizei renderTargetSize );


//-----------------------------------------------------------------------------------
// ***** StereoEyeParams

// StereoEyeParams describes RenderDevice configuration needed to render
// the scene for one eye. 
struct StereoEyeParams
{
    StereoEye               Eye;
    Matrix4f                HmdToEyeViewOffset;     // Translation from the HMD "middle eye" to actual eye.

    // Distortion and the VP on the physical display - the thing to run the distortion shader on.
    DistortionRenderDesc    Distortion;
    Recti                   DistortionViewport;

    // Projection and VP of a particular view (you could have multiple of these).
    Recti                   RenderedViewport;       // Viewport that we render the standard scene to.
    FovPort                 Fov;                    // The FOVs of this scene.
    Matrix4f                RenderedProjection;     // Projection matrix used with this eye.
    ScaleAndOffset2D        EyeToSourceNDC;         // Mapping from TanEyeAngle space to [-1,+1] on the rendered image.
    ScaleAndOffset2D        EyeToSourceUV;          // Mapping from TanEyeAngle space to actual texture UV coords.
};


//-----------------------------------------------------------------------------------
// A set of "forward-mapping" functions, mapping from framebuffer space to real-world and/or texture space.
Vector2f TransformScreenNDCToTanFovSpace ( DistortionRenderDesc const &distortion,
                                           const Vector2f &framebufferNDC );
void TransformScreenNDCToTanFovSpaceChroma ( Vector2f *resultR, Vector2f *resultG, Vector2f *resultB, 
                                             DistortionRenderDesc const &distortion,
                                             const Vector2f &framebufferNDC );
Vector2f TransformTanFovSpaceToRendertargetTexUV ( ScaleAndOffset2D const &eyeToSourceUV,
                                                   Vector2f const &tanEyeAngle );
Vector2f TransformTanFovSpaceToRendertargetNDC ( ScaleAndOffset2D const &eyeToSourceNDC,
                                                 Vector2f const &tanEyeAngle );
Vector2f TransformScreenPixelToScreenNDC( Recti const &distortionViewport,
                                          Vector2f const &pixel );
Vector2f TransformScreenPixelToTanFovSpace ( Recti const &distortionViewport,
                                             DistortionRenderDesc const &distortion,
                                             Vector2f const &pixel );
Vector2f TransformScreenNDCToRendertargetTexUV( DistortionRenderDesc const &distortion,
                                                StereoEyeParams const &eyeParams,
                                                Vector2f const &pixel );
Vector2f TransformScreenPixelToRendertargetTexUV( Recti const &distortionViewport,
                                                  DistortionRenderDesc const &distortion,
                                                  StereoEyeParams const &eyeParams,
                                                  Vector2f const &pixel );

// A set of "reverse-mapping" functions, mapping from real-world and/or texture space back to the framebuffer.
// Be aware that many of these are significantly slower than their forward-mapping counterparts.
Vector2f TransformTanFovSpaceToScreenNDC( DistortionRenderDesc const &distortion,
                                          const Vector2f &tanEyeAngle, bool usePolyApprox = false );
Vector2f TransformRendertargetNDCToTanFovSpace( const ScaleAndOffset2D &eyeToSourceNDC,
                                                const Vector2f &textureNDC );

// Handy wrappers.
inline Vector2f TransformTanFovSpaceToRendertargetTexUV ( StereoEyeParams const &eyeParams,
                                                          Vector2f const &tanEyeAngle )
{
    return TransformTanFovSpaceToRendertargetTexUV ( eyeParams.EyeToSourceUV, tanEyeAngle );
}
inline Vector2f TransformTanFovSpaceToRendertargetNDC ( StereoEyeParams const &eyeParams,
                                                        Vector2f const &tanEyeAngle )
{
    return TransformTanFovSpaceToRendertargetNDC ( eyeParams.EyeToSourceNDC, tanEyeAngle );
}


//-----------------------------------------------------------------------------
// CalculateOrientationTimewarpMatrix
//
// For Orientation-only Timewarp, the inputs are quaternions and the output is
// a transform matrix.  The matrix may need to be transposed depending on which
// renderer is used.  This function produces one compatible with D3D11.
//
// eye: Input quaternion of EyeRenderPose.Orientation inverted.
// pred: Input quaternion of predicted eye pose at scanout.
// M: Output D3D11-compatible transform matrix for the Timewarp shader.
void CalculateOrientationTimewarpMatrix(Quatf const & eye, Quatf const & pred, Matrix4f& M);

//-----------------------------------------------------------------------------
// CalculatePositionalTimewarpMatrix
//
// The matrix may need to be transposed depending on which
// renderer is used.  This function produces one compatible with D3D11.
//
// renderFromEyeInverted: Input render transform from eye inverted.
// hmdPose: Input predicted head pose from HMD tracking code.
// extraEyeOffset: Input extra eye position offset applied to calculations.
// M: Output D3D11-compatible transform matrix for the Timewarp shader.
void CalculatePositionalTimewarpMatrix(Posef const & renderFromEyeInverted, Posef const & hmdPose,
                                       Vector3f const & extraEyeOffset, Matrix4f& M);


//-----------------------------------------------------------------------------
// CalculateTimewarpFromPoses
//
// Given start/end predicted poses, construct timewarp matrices.
//
// eyeRenderPose: RenderPose eye quaternion, *not* inverted.
// poseInFaceSpace: true if the pose supplied is stuck-to-your-face rather than fixed-in-space
// calcPosition: true if the position part of the result is actually used (false = orientation only)
// hmdToEyeViewOffset: offset from the HMD "middle eye" to actual eye.
// hmdStartEndPoses: the predicted poses of the HMD at start/end times.
//
// Results:
// startEndMatrices: Timewarp matrices for the start and end times respectively.
void CalculateTimewarpFromPoses(Posef const & eyeRenderPose,
                                bool poseInFaceSpace,
                                bool calcPosition, 
                                Vector3f const &hmdToEyeViewOffset,
                                Posef const hmdStartEndPoses[2],
                                Matrix4f startEndMatrices[2]);

//-----------------------------------------------------------------------------
// CalculateOrientationTimewarpFromSensors
//
// LEGACY - DO NOT USE. Only for the OGL path right now.

// Explicit use of something like:
// DistortionRenderer::readSensorsAndCalculateHmdPoses + CalculateTimewarpFromPoses
// is preferred.
//
// Similar to CalculateTimewarpFromPoses, but reads the sensors for you,
// and only handles orientation, not position.
//
// eyeQuat: RenderPose eye quaternion, *not* inverted.
// reader: the tracking state
// startEndTimes: start and end times of the screen - typically fed direct from Timing->GetTimewarpTiming()->EyeStartEndTimes
//
// Results:
// startEndMatrices: Timewarp matrices for the start and end times respectively.
// timewarpIMUTime: On success it contains the raw IMU sample time for the pose.
void CalculateOrientationTimewarpFromSensors(Quatf const & eyeQuat,
                                             Vision::TrackingStateReader* reader,
                                             const double startEndTimes[2],
                                             Matrix4f startEndMatrices[2],
                                             double& timewarpIMUTime);


//-----------------------------------------------------------------------------
// CalculateEyeTimewarpTimes
//
// Given the scanout start time, duration of scanout, and shutter type, this
// function returns the timewarp start and end prediction times.
void CalculateEyeTimewarpTimes(double scanoutStartTime, double scanoutDuration,
                               HmdShutterTypeEnum shutterType,
                               double eyeStartEndTime[2]);

//-----------------------------------------------------------------------------
// CalculateEyeRenderTimes
//
// Given the scanout start time, duration of scanout, and shutter type, this
// function returns the left/right eye render times.
void CalculateEyeRenderTimes(double scanoutStartTime, double scanoutDuration,
                             HmdShutterTypeEnum shutterType,
                             double& leftEyeRenderTime, double& rightEyeRenderTime);


//-----------------------------------------------------------------------------------
// ***** Distortion mesh structures

/// Describes a vertex used by the distortion mesh. This is intended to be converted into
/// the engine-specific format. Some fields may be unused based on the ovrDistortionCaps
/// flags selected. TexG and TexB, for example, are not used if chromatic correction is
/// not requested.
typedef struct OVR_ALIGNAS(8) DistortionMeshVertex_
{
    ovrVector2f ScreenPosNDC;   ///< [-1,+1],[-1,+1] over the entire framebuffer.
    float       TimeWarpFactor; ///< Lerp factor between time-warp matrices. Can be encoded in Pos.z.
    float       VignetteFactor; ///< Vignette fade factor. Can be encoded in Pos.w.
    ovrVector2f TanEyeAnglesR;  ///< The tangents of the horizontal and vertical eye angles for the red channel.
    ovrVector2f TanEyeAnglesG;  ///< The tangents of the horizontal and vertical eye angles for the green channel.
    ovrVector2f TanEyeAnglesB;  ///< The tangents of the horizontal and vertical eye angles for the blue channel.
} DistortionMeshVertex;

/// Describes a full set of distortion mesh data, filled in by CalculateDistortionMeshFromFOV.
/// Contents of this data structure, if not null, should be freed by DestroyDistortionMeshObject.
typedef struct OVR_ALIGNAS(8) DistortionMesh_
{
    DistortionMeshVertex* pVertexData; ///< The distortion vertices representing each point in the mesh.
    unsigned short*       pIndexData;  ///< Indices for connecting the mesh vertices into polygons.
    unsigned int          VertexCount; ///< The number of vertices in the mesh.
    unsigned int          IndexCount;  ///< The number of indices in the mesh.
} DistortionMesh;



//-----------------------------------------------------------------------------
// CalculateDistortionMeshFromFOV
//
// This function fills in the target meshData object given the provided
// parameters, for a single specified eye.
//
// Returns false on failure.
bool CalculateDistortionMeshFromFOV(HmdRenderInfo const & renderInfo,
                                    DistortionRenderDesc const & distortionDesc,
                                    StereoEye stereoEye, FovPort fov,
                                    unsigned distortionCaps,
                                    DistortionMesh *meshData);

void DestroyDistortionMeshObject(DistortionMesh* meshData);



/// Computes updated 'uvScaleOffsetOut' to be used with a distortion if render target size or
/// viewport changes after the fact. This can be used to adjust render size every frame if desired.
void GetRenderScaleAndOffset(ovrFovPort fov,
                             ovrSizei textureSize, ovrRecti renderViewport,
                             ovrVector2f uvScaleOffsetOut[2]);


} //namespace OVR

#endif // OVR_Stereo_h
