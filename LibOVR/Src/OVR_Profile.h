/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Profile.h
Content     :   Structs and functions for loading and storing device profile settings
Created     :   February 14, 2013
Notes       :
   Profiles are used to store per-user settings that can be transferred and used
   across multiple applications.  For example, player IPD can be configured once
   and reused for a unified experience across games.  Configuration and saving of profiles
   can be accomplished in game via the Profile API or by the official Oculus Configuration
   Utility.

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

************************************************************************************/

#ifndef OVR_Profile_h
#define OVR_Profile_h

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_RefCount.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_Math.h"

namespace OVR {

// Defines the profile object for each device type
enum ProfileDeviceType
{
    Profile_Unknown       = 0,
    Profile_GenericHMD    = 10,
    Profile_RiftDK1       = 11,
    Profile_RiftDKHD      = 12,
};

//
// HMDProfile and it's child classes, RiftProfile, RiftDk1Profile and
// RiftDKHDProfile represent the intersection of 'per-user' and 'per-device'
// settings.
//

//-----------------------------------------------------------------------------
// ***** HMDProfile

// The generic HMD profile is used for properties that are common to all headsets
class HmdDevice
{
protected:
    // FOV extents in pixels measured by a user
    int                 LL;       // left eye outer extent
    int                 LR;       // left eye inner extent
    int                 RL;       // right eye inner extent
    int                 RR;       // right eye outer extent

public:
    virtual ~HmdDevice() {}
    void SetLL(int val) { LL = val; };
    void SetLR(int val) { LR = val; };
    void SetRL(int val) { RL = val; };
    void SetRR(int val) { RR = val; };

    int GetLL() { return LL; };
    int GetLR() { return LR; };
    int GetRL() { return RL; };
    int GetRR() { return RR; };
    virtual ProfileDeviceType GetDeviceType() const {
      return Profile_GenericHMD;
    }

protected:
    HmdDevice() {
          LL = 0;
          LR = 0;
          RL = 0;
          RR = 0;
    }
    friend class ProfileLoader;
    friend class Profile;
};

// For headsets that use eye cups
enum EyeCupType
{
    EyeCup_A = 0,
    EyeCup_B = 1,
    EyeCup_C = 2
};

class RiftDevice : public HmdDevice {
protected:
    EyeCupType          EyeCups;   // Which eye cup does the player use

public:
    EyeCupType          GetEyeCup() { return EyeCups; };
    void                SetEyeCup(EyeCupType cup) { EyeCups = cup; };

protected:
    RiftDevice() {
      EyeCups = EyeCup_A;
    }
    friend class ProfileLoader;
    friend class Profile;
};

//-----------------------------------------------------------------------------
// ***** RiftDK1Profile

// This profile is specific to the Rift Dev Kit 1 and contains overrides specific
// to that device and lens cup settings.
class RiftDK1Device : public RiftDevice
{
public:
  virtual ProfileDeviceType GetDeviceType() const {
    return Profile_RiftDK1;
  }

protected:
  RiftDK1Device() {

    }
    friend class ProfileLoader;
    friend class Profile;
};

//-----------------------------------------------------------------------------
// ***** RiftDKHDProfile

// This profile is specific to the Rift HD Dev Kit and contains overrides specific
// to that device and lens cup settings.
class RiftDKHDDevice : public RiftDevice
{
public:
  virtual ProfileDeviceType GetDeviceType() const {
    return Profile_RiftDKHD;
  }

protected:
    RiftDKHDDevice() {
    }
    friend class ProfileLoader;
    friend class Profile;
};

//-------------------------------------------------------------------
// ***** Profile

// The base profile for all users.  This object is not created directly.
//
class Profile : public RefCountBase<Profile>
{
public:
    enum GenderType
    {
        Gender_Unspecified  = 0,
        Gender_Male         = 1,
        Gender_Female       = 2
    };

protected:
    GenderType           Gender;            // The gender of the user
    float                PlayerHeight;      // The height of the user in meters
    float                IPD;               // Distance between eyes in meters
    Quatf                StrabismusCorrection;  // Amount to rotate modelview matrix to correct for corss-eyed vision
                                                // Should be applied as is to the left eye, and inverted to apply to the
                                                // right eye
    HmdDevice           Generic;
    RiftDK1Device       RiftDK1;
    RiftDKHDDevice      RiftDKHD;

public:
    // These are properties which are intrinsic to the user and affect scene setup
    GenderType           GetGender() const               { return Gender; };
    float                GetPlayerHeight() const         { return PlayerHeight; };
    float                GetIPD()  const                 { return IPD; };
    float                GetEyeHeight() const;
    const Quatf &        GetStrabismusCorrection() const { return StrabismusCorrection; };
    HmdDevice &          GetGenericDevice()              { return Generic; }
    RiftDK1Device &      GetRiftDK1Device()              { return RiftDK1; }
    RiftDKHDDevice &     GetRiftDKHDDevice()             { return RiftDKHD; }
    const HmdDevice &    GetGenericDevice() const        { return Generic; }
    const RiftDK1Device& GetRiftDK1Device() const     { return RiftDK1; }
    const RiftDKHDDevice&GetRiftDKHDDevice() const    { return RiftDKHD; }

    void                 SetGender(GenderType gender)    { Gender = gender; };
    void                 SetPlayerHeight(float height)   { PlayerHeight = height; };
    void                 SetIPD(float ipd)               { IPD = ipd; };
    void                 SetStrabismusCorrection(const Quatf & quat) { StrabismusCorrection = quat; };
    Profile *            Clone() const {
      return new Profile(*this);
    }

protected:
    Profile();
    friend class ProfileManagerImpl;
    friend class ProfileLoader;
};

// -----------------------------------------------------------------------------
// ***** ProfileManager

// Profiles are interfaced through a ProfileManager object.  Applications should
// create a ProfileManager each time they intend to read or write user profile data.
// The scope of the ProfileManager object defines when disk I/O is performed.  Disk
// reads are performed on the first profile access and disk writes are performed when
// the ProfileManager goes out of scope.  All profile interactions between these times
// are performed in local memory and are fast.  A typical profile interaction might
// look like this:
//
// {
//     Ptr<ProfileManager> pm      = *ProfileManager::Create();
//     Ptr<Profile>        profile = pm->LoadProfile(pm->GetDefaultProfileName());
//     if (profile)
//     {   // Retrieve the current profile settings
//     }
// }   // Profile will be destroyed and any disk I/O completed when going out of scope

class ProfileManager : public RefCountBase<ProfileManager>
{
public:
    static ProfileManager* Create();

    virtual unsigned    GetProfileCount() = 0;
    virtual bool        HasProfile(const String & name) = 0;
    virtual Profile*    LoadProfile(const String & name) = 0;
    virtual Profile*    GetDefaultProfile() = 0;
    virtual const String & GetDefaultProfileName() = 0;
    virtual bool        SetDefaultProfileName(const String & name) =0;
    virtual bool        Save(const String & name, const Profile * profile) = 0;
    virtual bool        Delete(const String & name) = 0;
protected:
    ProfileManager() {
    }

    virtual ~ProfileManager() {
    }
};


String GetBaseOVRPath(bool create_dir);

}

#endif // OVR_Profile_h
