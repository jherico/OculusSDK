/************************************************************************************

PublicHeader:   None
Filename    :   OVR_Profile.cpp
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

#include "OVR_Profile.h"
#include "OVR_Log.h"
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_SysFile.h"
#include "Kernel/OVR_Allocator.h"
#include "Kernel/OVR_Array.h"
#include <json/json.h>
#include <fstream>
#ifdef OVR_OS_WIN32
#include <Shlobj.h>
#else
#include <dirent.h>
#include <sys/stat.h>

#ifdef OVR_OS_LINUX
#include <unistd.h>
#include <pwd.h>
#endif

#endif


#define PROFILE_VERSION 2
#define MAX_PROFILE_MAJOR_VERSION 2

// Many hard coded strings used in numerous locations have been
// repositioned here, so that there's no chance of a misspelling
// causing a problem.  Not every string has been moved, but most of the
// repeated ones have.
#define KEY_PROFILE_VERSION "Oculus Profile Version"
#define KEY_CURRENT_PROFILE "CurrentProfile"
#define KEY_PROFILES "Profiles"
#define KEY_DEVICES "Devices"
#define KEY_GENDER "Gender"
#define KEY_PLAYER_HEIGHT "PlayerHeight"
#define KEY_IPD "IPD"
#define KEY_STRABISMUS_CORRECTION "StrabismusCorrection"

#define KEY_LL "LL"
#define KEY_LR "LR"
#define KEY_RL "RL"
#define KEY_RR "RR"
#define KEY_EYECUP "EyeCup"
#define EPSILON 0.00001f
// 5'10" inch man
#define DEFAULT_HEIGHT 1.778f
#define DEFAULT_IPD 0.064f


namespace OVR {

//-----------------------------------------------------------------------------
// Returns the pathname of the JSON file containing the stored profiles
String GetBaseOVRPath(bool create_dir)
{
    String path;

#if defined(OVR_OS_WIN32)

    TCHAR data_path[MAX_PATH];
    SHGetFolderPath(0, CSIDL_LOCAL_APPDATA, NULL, 0, data_path);
    path = String(data_path);

    path += "/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        WCHAR wpath[128];
        OVR::UTF8Util::DecodeString(wpath, path.ToCStr());

        DWORD attrib = GetFileAttributes(wpath);
        bool exists = attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY);
        if (!exists)
        {
            CreateDirectory(wpath, NULL);
        }
    }

#elif defined(OVR_OS_MAC)

    const char* home = getenv("HOME");
    path = home;
    path += "/Library/Preferences/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DIR* dir = opendir(path);
        if (dir == NULL)
        {
            mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        else
        {
            closedir(dir);
        }
    }

#else
    // Updated the config folder location logic to rely on the
    // XDG specification for config locations (the XDK location was being
    // used anyway, but was hardcoded).  This is analgous to using
    // SHGetFolderPath in the windows implementation, rather than
    // hardcoding %HOME%/AppData/Local
    const char * config_home = getenv("XDG_CONFIG_HOME");
    if (NULL != config_home) {
      path = config_home;
    } else {
      // only if XDG_CONFIG_HOME is unses does the specification say to
      // fallback on the default of $HOME/.config
      path = getenv("HOME");
      path += "/.config";
    }
    path += "/Oculus";

    if (create_dir)
    {   // Create the Oculus directory if it doesn't exist
        DIR* dir = opendir(path);
        if (dir == NULL)
        {
            mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        else
        {
            closedir(dir);
        }
    }

#endif

    return path;
}

String GetProfilePath(bool create_dir)
{
    String path = GetBaseOVRPath(create_dir);
    path += "/Profiles.json";
    return path;
}

//-----------------------------------------------------------------------------
// ***** ProfileManager

ProfileManager::ProfileManager()
{
    Changed = false;
    Loaded = false;
}

ProfileManager::~ProfileManager()
{
    // If the profiles have been altered then write out the profile file
    if (Changed)
        SaveCache();

    ClearCache();
}

ProfileManager* ProfileManager::Create()
{
    return new ProfileManager();
}

// Clear the local profile cache
void ProfileManager::ClearCache()
{
    Lock::Locker lockScope(&ProfileLock);
    ProfileCache.Clear();
    Loaded = false;
}

// Profile loader is an intermediary that allows me to break down the
// serialization into smaller pieces that work directly against the
// new native JSON container type.  This can't be done in the
// ProfileManager because it can't declare functions referencing the
// new type without polluting the header with knowledge of the JSON
// implementation
class ProfileLoader {
public:
  static void loadHmd(HmdDevice & device, const Json::Value & node) {
    if (node.isNull()) {
      return;
    }
    if (node.isMember(KEY_LL)) {
      device.LL = node.get(KEY_LL, 0).asInt();
    }
    if (node.isMember(KEY_LR)) {
      device.LR = node.get(KEY_LR, 0).asInt();
    }
    if (node.isMember(KEY_RL)) {
      device.RL = node.get(KEY_RL, 0).asInt();
    }
    if (node.isMember(KEY_RR)) {
      device.RR = node.get(KEY_RR, 0).asInt();
    }
  }

  static void loadRift(RiftDevice & device, const Json::Value & node) {
    if (node.isNull()) {
      return;
    }
    String eyeCups = node.get(KEY_EYECUP, "A").asCString();
    char c = eyeCups.GetSize() ? eyeCups.GetCharAt(0) : 'A';
    switch (eyeCups.GetCharAt(0)) {
    case 'A':
      device.EyeCups = EyeCupType::EyeCup_A;
      break;
    case 'B':
      device.EyeCups = EyeCupType::EyeCup_B;
      break;
    case 'C':
      device.EyeCups = EyeCupType::EyeCup_C;
      break;
    }
    loadHmd(device, node);
  }

  // TODO migrate all the string constants up
  static void loadV1Profile(Profile & out, const Json::Value & node) {
    if (node.isNull()) {
      return;
    }
    String gender = node.get(KEY_GENDER, "Unknown").asCString();
    if (gender == "Male") {
      out.Gender = Profile::GenderType::Gender_Male;
    } else if (gender == "Female") {
      out.Gender = Profile::GenderType::Gender_Female;
    } else {
      out.Gender = Profile::GenderType::Gender_Unspecified;
    }

    out.PlayerHeight = node.get(KEY_PLAYER_HEIGHT, DEFAULT_HEIGHT).asFloat();
    out.IPD = node.get(KEY_IPD, DEFAULT_IPD).asFloat();
    loadHmd(out.Generic, node.get("GenericHMD", Json::Value::null));
    loadRift(out.RiftDK1, node.get("RiftDK1", Json::Value::null));
    loadRift(out.RiftDKHD, node.get("RiftDKHD", Json::Value::null));
  }

  static void loadProfile(Profile & out, const Json::Value & node) {
    if (node.isNull()) {
      return;
    }
    loadV1Profile(out, node);
    if (node.isMember(KEY_STRABISMUS_CORRECTION)) {
      loadQuaternion(out.StrabismusCorrection, node[KEY_STRABISMUS_CORRECTION]);
    }
  }

  static void loadQuaternion(Quatf & out, const Json::Value & node) {
    if (node.isNull()) {
      return;
    }
    out.x = node.get("X", 0).asInt();
    out.y = node.get("Y", 0).asInt();
    out.z = node.get("Z", 0).asInt();
    out.w = node.get("W", 1).asInt();
  }

  static void saveQuaternion(Json::Value & out, const Quatf & q) {
    out["X"] = q.x;
    out["Y"] = q.y;
    out["Z"] = q.z;
    out["W"] = q.w;
  }

  // TODO implement, and migrate the Devices.json parsing code to use this
  static void loadMatrix(Matrix4f & out, const Json::Value & node) {
  }

  //
  // The general pattern on writing Json is to write the node only if
  // it differs from the default, and to explicitly remove the node if it
  // is the same as the default.  This is important, because the load / save
  // mechanism is designed to preserve any pre-existing content and not
  // to touch any fields it's not aware of.
  //
  // Therefore, failing to write out a default value doesn't mean there's
  // not already a non-default value there, hence the explicit removes.
  // It gets a little verbose but the result is greater extensibility of the
  // profile data by third parties.
  static void writeHmdDevice(Json::Value & out, const HmdDevice & device) {
    if (device.LL != 0) {
      out[KEY_LL] = device.LL;
    } else {
      out.removeMember(KEY_LL);
    }
    if (device.LR != 0) {
      out[KEY_LR] = device.LR;
    } else {
      out.removeMember(KEY_LR);
    }
    if (device.RL != 0) {
      out[KEY_RL] = device.RL;
    } else {
      out.removeMember(KEY_RL);
    }
    if (device.RR != 0) {
      out[KEY_RR] = device.RR;
    } else {
      out.removeMember(KEY_RR);
    }
  }

  static void writeRiftDevice(Json::Value & deviceNode, const RiftDevice & device) {
    switch (device.EyeCups)
    {
        case EyeCup_B: deviceNode[KEY_EYECUP] = "B"; break;
        case EyeCup_C: deviceNode[KEY_EYECUP] = "C"; break;
        // A is the default, so no need to serialize it
        case EyeCup_A: deviceNode.removeMember(KEY_EYECUP); break;
    }
    writeHmdDevice(deviceNode, device);
  }

  // Applying the "remove nodes if they're empty" logic here is a little
  // arduous, but it makes the resulting JSON cleaner
  static void updateDeviceProfile(Json::Value & parent, const HmdDevice & device) {
    String name;
    Json::Value newChild;
    switch (device.GetDeviceType()) {
    case Profile_RiftDK1:
      name = "RiftDK1";
      newChild = parent[name];
      writeRiftDevice(newChild, (RiftDevice&)device);
      break;

    case Profile_RiftDKHD:
      name = "RiftDKHD";
      newChild = parent[name];
      writeRiftDevice(newChild, (RiftDevice&)device);
      break;

    case Profile_GenericHMD:
      name = "GenericHMD";
      newChild = parent[name];
      writeHmdDevice(newChild, device);
      break;
    }

    // Don't write empty children
    if (newChild.getMemberNames().size()) {
      parent[name] = newChild;
    } else {
      parent.removeMember(name);
    }
  }

  static void writeProfile(Json::Value & parent, const Profile & profile) {
    Json::Value & out = parent[profile.Name];

    switch (profile.GetGender()) {
        case Profile::Gender_Male:
          out[KEY_GENDER] = "Male";
          break;
        case Profile::Gender_Female:
          out[KEY_GENDER] = "Female";
          break;
        default:
          out.removeMember(KEY_GENDER);
    }

    // Epsilon is 10 micrometers. Smaller than a human hair
    if (abs(profile.PlayerHeight - DEFAULT_HEIGHT) > EPSILON) {
      out[KEY_PLAYER_HEIGHT] = profile.PlayerHeight;
    } else {
      out.removeMember(KEY_PLAYER_HEIGHT);
    }

    if (abs(profile.IPD - DEFAULT_IPD) > EPSILON) {
      out[KEY_IPD] = profile.IPD;
    } else {
      out.removeMember(KEY_IPD);
    }

    if (profile.StrabismusCorrection.Distance(Quatf()) > EPSILON) {
      saveQuaternion(out[KEY_STRABISMUS_CORRECTION],
          profile.StrabismusCorrection);
    } else {
      out.removeMember(KEY_STRABISMUS_CORRECTION);
    }

    Json::Value & devicesNode = out[KEY_DEVICES];
    updateDeviceProfile(devicesNode, profile.Generic);
    updateDeviceProfile(devicesNode, profile.RiftDK1);
    updateDeviceProfile(devicesNode, profile.RiftDKHD);
    if (!devicesNode.getMemberNames().size()) {
      out.removeMember(KEY_DEVICES);
    }
  }
};

// Use of STL IO is potentially avoidable with jsoncpp, if you wanted
// to load the file into a String instead and use the char* interface for
// parsing JSON in memory.  Linking to C++ standard library is unavoidable
// though, but it might not take much work to remove iostream usage from
// the jsoncpp lib and make it use std::string only.
bool parseJsonFile(const String & file, Json::Value & root) {
  Json::Reader reader;
  String path = GetProfilePath(false);
  std::ifstream in(path.ToCStr());
  if (!reader.parse(in, root)) {
    // report to the user the failure and their locations in the document.
    LogError("Failed to parse json file: %s\n %s",
        file.ToCStr(),
        reader.getFormattedErrorMessages().c_str());
    return false;
  }
  return true;

}
// Poplulates the local profile cache.  This occurs on the first access of the profile
// data.  All profile operations are performed against the local cache until the
// ProfileManager is released or goes out of scope at which time the cache is serialized
// to disk.
void ProfileManager::LoadCache() {
  if (Loaded) {
    return;
  }

  Lock::Locker lockScope(&ProfileLock);
  ClearCache();

  Json::Value root;
  if (!parseJsonFile(GetProfilePath(false), root)) {
    LogError("Failed to parse configuration");
    return;
  }

  if (!root.isMember(KEY_PROFILE_VERSION)) {
    LogError("Profile JSON is malformed, missing version number");
    return;
  }

  int major = root[KEY_PROFILE_VERSION].asInt();
  switch (major) {
    case 1: {
      if (root.size() < 3) {
        LogError("Profile JSON is malformed, insufficient keys");
        return;
      }
      DefaultProfile = root[KEY_CURRENT_PROFILE].asCString();
      const Json::Value & profileNode = root["Profile"];
      String name = profileNode["Name"].asCString();
      // The profile having to know it's own name is a symptom of the lack
      // of a proper associative array class in the OVR codebase.
      Profile * profile = new Profile(name);
      ProfileLoader::loadV1Profile(*profile, profileNode);
      ProfileCache.PushBack(profile);
    }
    break;

    case 2: {
      if (!root.isMember(KEY_PROFILES)) {
        LogError("Missing profile data");
        return;
      }

      const Json::Value & profiles = root[KEY_PROFILES];
      Json::Value::const_iterator itr;
      for (itr = profiles.begin(); itr != profiles.end(); ++itr) {
        String profileName = itr.memberName();
        Profile * profile = new Profile(profileName);
        ProfileLoader::loadProfile(*profile, root[profileName]);
        ProfileCache.PushBack(profile);
      }

      if (!root.isMember(KEY_CURRENT_PROFILE)) {
        LogError("Missing current profile");
      } else {
        DefaultProfile = root[KEY_CURRENT_PROFILE].asCString();
      }
    }
    break;

    default:
      LogError("Usupported profile version %d", major);
      return;   // don't parse the file on unsupported major version number
  }
  Loaded = true;
}

// Serializes the profiles to disk.
void ProfileManager::SaveCache()
{
    Lock::Locker lockScope(&ProfileLock);
    // Limit scope of reader
    Json::Value root;
    String path = GetProfilePath(false);
    if (!parseJsonFile(path, root))
    {
      root = Json::Value();
    }

    // If the file is V1, we don't want to preserve it.
    // Technically, we could make an effort to preserve it but explcitly
    // remove the V1 tokens we don't use in V2, but that's a bunch of
    // effort that will likely serve no useful purpose.  Since the file
    // isn't valid JSON if it has multiple profiles, I doubt anyone is
    // extending it yet
    if (root.isMember(KEY_PROFILE_VERSION) &&
        PROFILE_VERSION != root[KEY_PROFILE_VERSION].asInt()) {
      root = Json::Value();
    }

    root[KEY_PROFILE_VERSION] = PROFILE_VERSION;
    if (!DefaultProfile.IsEmpty()) {
      root[KEY_CURRENT_PROFILE] = DefaultProfile.ToCStr();
    }

    // Generate a JSON object of 'profile name' to 'profile data'
    Json::Value & profiles = root[KEY_PROFILES];
    for (unsigned int i=0; i<ProfileCache.GetSize(); i++) {
      const Profile * profile = ProfileCache[i];
      ProfileLoader::writeProfile(profiles, *profile);
    }

    {
      Json::StyledWriter writer;
      std::string output = writer.write( root );
      SysFile f;
      if (!f.Open(path, File::Open_Write | File::Open_Create | File::Open_Truncate, File::Mode_Write)) {
        LogError("Unable to open %s for writing", path.ToCStr());
        return;
      }
      int written = f.Write((const unsigned char*)output.c_str(),
          output.length());
      if (written != output.length()) {
        LogError("Short write, only %d of %d bytes written",
            written, (int)output.length());
      }
      f.Close();
    }
}

// Returns the number of stored profiles for this device type
unsigned int ProfileManager::GetProfileCount()
{
    Lock::Locker lockScope(&ProfileLock);
    return ProfileCache.GetSize();
}

bool ProfileManager::HasProfile(const char* name)
{
  return NULL != ProfileCache.at(name);
}

// Returns a profile object for a particular device and user name.  The returned
// memory should be encapsulated in a Ptr<> object or released after use.  Returns
// NULL if the profile is not found
Profile* ProfileManager::LoadProfile(const char* user)
{
   // Maybe 'null' should be interpreted as 'return the default?'
    if (user == NULL)
        return NULL;

    Lock::Locker lockScope(&ProfileLock);
    LoadCache();
    Profile * result = ProfileCache.at(user);
    if (!result) {
      return NULL;
    }
    // Never give the caller memory that we ourselves are managing.
    return result->Clone();
}

// Returns a profile with all system default values
Profile* ProfileManager::GetDefaultProfile()
{
    return new Profile("default");
}

// Returns the name of the profile that is marked as the current default user.
const char* ProfileManager::GetDefaultProfileName()
{
    Lock::Locker lockScope(&ProfileLock);
    LoadCache();

    if (ProfileCache.GetSize() > 0)
    {
        OVR_strcpy(NameBuff, 32, DefaultProfile);
        return NameBuff;
    }
    else
    {
        return NULL;
    }
}


// Marks a particular user as the current default user.
bool ProfileManager::SetDefaultProfileName(const char* name) {
    Lock::Locker lockScope(&ProfileLock);
    LoadCache();
    // TODO: I should verify that the user is valid
    if (ProfileCache.GetSize() > 0)
    {
        DefaultProfile = name;
        Changed = true;
        return true;
    }
    else
    {
        return false;
    }
}


// Saves a new or existing profile.  Returns true on success or false on an
// invalid or failed save.
bool ProfileManager::Save(const Profile * profile)
{
    if (NULL == profile) {
      return false;
    }

    if (OVR_strcmp(profile->Name, "default") == 0)
        return false;  // don't save a default profile

    Lock::Locker lockScope(&ProfileLock);
    LoadCache();

    // Look for the pre-existence of this profile
    int index = ProfileCache.IndexOf(profile->Name);
    if (Map::npos == index) {
      // TODO: I should do a proper field comparison to avoid unnecessary
      // overwrites and file saves

      // Replace the previous instance with the new profile
      ProfileCache[index] = profile->Clone();
    } else {
      ProfileCache.PushBack(profile->Clone());
    }
    Changed = true;
    return true;
}

// Removes an existing profile.  Returns true if the profile was found and deleted
// and returns false otherwise.
bool ProfileManager::Delete(const Profile * profile)
{
    if (NULL == profile) {
      return false;
    }
    Lock::Locker lockScope(&ProfileLock);
    int index = ProfileCache.IndexOf(profile->Name);
    if (Map::npos == index) {
      return false;
    }
    ProfileCache.RemoveAt(index);
    return true;
}

//-----------------------------------------------------------------------------
// ***** Profile

Profile::Profile(const char* name)
{
    Gender       = Gender_Unspecified;
    PlayerHeight = DEFAULT_HEIGHT;
    IPD          = DEFAULT_IPD;
    Name         = name;
}

// Computes the eye height from the metric head height
float Profile::GetEyeHeight() const
{
    const float EYE_TO_HEADTOP_RATIO =   0.44538f;
    const float MALE_AVG_HEAD_HEIGHT =   0.232f;
    const float FEMALE_AVG_HEAD_HEIGHT = 0.218f;

    // compute distance from top of skull to the eye
    float head_height;
    if (Gender == Gender_Female)
        head_height = FEMALE_AVG_HEAD_HEIGHT;
    else
        head_height = MALE_AVG_HEAD_HEIGHT;

    float skull = EYE_TO_HEADTOP_RATIO * head_height;

    float eye_height  = PlayerHeight - skull;
    return eye_height;
}

}  // OVR
