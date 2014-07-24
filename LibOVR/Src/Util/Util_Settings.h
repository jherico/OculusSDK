/************************************************************************************

PublicHeader:   n/a
Filename    :   Util_Settings.h
Content     :   Persistent settings subsystem
Created     :   June 11, 2014
Author      :   Chris Taylor

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Settings_h
#define OVR_Settings_h

#include "../Kernel/OVR_String.h"
#include "../Kernel/OVR_System.h"
#include "../Kernel/OVR_Observer.h"
#include "../OVR_JSON.h"
#include "Util_LongPollThread.h"

namespace OVR { namespace Util {


//-----------------------------------------------------------------------------
// Settings

class Settings : public NewOverrideBase, public SystemSingletonBase<Settings>
{
    OVR_DECLARE_SINGLETON(Settings);

    ObserverScope<LongPollThread::PollFunc> PollObserver;
    void pollDirty();

    // Helpers (call with lock held)
    void loadFile();
    void updateFile();

    // Synchronization for data access
    Lock DataLock;

    // Full path to the JSON settings file
    String FullFilePath;

    // Backed by JSON
    Ptr<JSON> Root;

    // Dirty flag to capture multiple changes for long poll writes
    bool Dirty;

public:
    void SetFileName(const String& fileName);

    void SetNumber(const char* key, double value);
    void SetInt(const char* key, int value);
    void SetBool(const char* key, bool value);
    void SetString(const char* key, const char* value);

    double GetNumber(const char* key, double defaultValue = 0.);
    int GetInt(const char* key, int defaultValue = 0);
    bool GetBool(const char* key, bool defaultValue = false);
    String GetString(const char* key, const char* defaultValue = "");
};


}} // namespace OVR::Util

#endif // OVR_Settings_h
