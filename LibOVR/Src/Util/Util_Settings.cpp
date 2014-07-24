/************************************************************************************

Filename    :   Util_Settings.cpp
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

#include "Util_Settings.h"
#include "../OVR_Profile.h"

OVR_DEFINE_SINGLETON(Util::Settings);

namespace OVR { namespace Util {


//// Settings

Settings::Settings() :
    Dirty(false)
{
    // Set up long poll handler
    PollObserver.SetHandler(LongPollThread::PollFunc::FromMember<Settings, &Settings::pollDirty>(this));
    LongPollThread::GetInstance()->AddPollFunc(PollObserver);

    PushDestroyCallbacks();
}

Settings::~Settings()
{
    PollObserver.ReleaseAll();

    Lock::Locker locker(&DataLock);

    if (Dirty)
    {
        updateFile();
        Dirty = false;
    }
}

void Settings::OnSystemDestroy()
{
    delete this;
}

void Settings::SetFileName(const String& fileName)
{
    Lock::Locker locker(&DataLock);

    OVR_ASSERT(FullFilePath.IsEmpty());

    if (FullFilePath.IsEmpty())
    {
        FullFilePath = GetBaseOVRPath(true) + "/" + fileName;

        loadFile();
    }
}

void Settings::loadFile()
{
    Root = *JSON::Load(FullFilePath.ToCStr());
    if (!Root)
    {
        OVR_DEBUG_LOG(("[Settings] Settings file was empty"));
    }
    else
    {
        OVR_DEBUG_LOG(("[Settings] Successfully read settings file"));
    }
}

void Settings::updateFile()
{
    OVR_ASSERT(!FullFilePath.IsEmpty());

    if (Root->Save(FullFilePath.ToCStr()))
    {
        OVR_DEBUG_LOG(("[Settings] Updated settings file: %s", FullFilePath.ToCStr()));
        Dirty = false;
    }
    else
    {
        LogError("[Settings] WARNING: Unable to write settings file: %s", FullFilePath.ToCStr());
        OVR_ASSERT(false);
    }
}

void Settings::pollDirty()
{
    // If dirty,
    if (Dirty)
    {
        Lock::Locker locker(&DataLock);

        if (!Dirty)
        {
            return;
        }

        updateFile();
    }
}

void Settings::SetNumber(const char* key, double value)
{
    Lock::Locker locker(&DataLock);

    Dirty = true;

    if (!Root)
    {
        Root = *JSON::CreateObject();
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        Root->AddNumberItem(key, value);
        return;
    }

    item->Type = JSON_Number;
    item->dValue = value;
}

void Settings::SetInt(const char* key, int value)
{
    Lock::Locker locker(&DataLock);

    if (!Root)
    {
        Root = *JSON::CreateObject();
        Dirty = true;
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        Root->AddIntItem(key, value);
        Dirty = true;
        return;
    }

    // If the value changed,
    if (item->Type != JSON_Number ||
        (int)item->dValue != value)
    {
        item->Type = JSON_Number;
        item->dValue = value;
        Dirty = true;
    }
}

void Settings::SetBool(const char* key, bool value)
{
    Lock::Locker locker(&DataLock);

    if (!Root)
    {
        Root = *JSON::CreateObject();
        Dirty = true;
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        Root->AddBoolItem(key, value);
        Dirty = true;
        return;
    }

    // If the value changed,
    if (item->Type != JSON_Bool ||
        ((int)item->dValue != 0) != value)
    {
        item->Type = JSON_Bool;
        item->dValue = value ? 1. : 0.;
        item->Value = value ? "true" : "false";
        Dirty = true;
    }
}

void Settings::SetString(const char* key, const char* value)
{
    Lock::Locker locker(&DataLock);

    if (!Root)
    {
        Root = *JSON::CreateObject();
        Dirty = true;
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        Root->AddStringItem(key, value);
        Dirty = true;
        return;
    }

    // If the value changed,
    if (item->Type != JSON_String ||
        item->Value != value)
    {
        item->Type = JSON_String;
        item->Value = value;
        Dirty = true;
    }
}

double Settings::GetNumber(const char* key, double defaultValue)
{
    Lock::Locker locker(&DataLock);

    if (!Root)
    {
        SetNumber(key, defaultValue);
        return defaultValue;
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        SetNumber(key, defaultValue);
        return defaultValue;
    }

    return item->dValue;
}

int Settings::GetInt(const char* key, int defaultValue)
{
    Lock::Locker locker(&DataLock);

    if (!Root)
    {
        SetInt(key, defaultValue);
        return defaultValue;
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        SetInt(key, defaultValue);
        return defaultValue;
    }

    return (int)item->dValue;
}

bool Settings::GetBool(const char* key, bool defaultValue)
{
    Lock::Locker locker(&DataLock);

    if (!Root)
    {
        SetBool(key, defaultValue);
        return defaultValue;
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        SetBool(key, defaultValue);
        return defaultValue;
    }

    return (int)item->dValue != 0;
}

String Settings::GetString(const char* key, const char* defaultValue)
{
    Lock::Locker locker(&DataLock);

    if (!Root)
    {
        SetString(key, defaultValue);
        return defaultValue;
    }

    Ptr<JSON> item = Root->GetItemByName(key);
    if (!item)
    {
        SetString(key, defaultValue);
        return defaultValue;
    }

    return item->Value;
}


}} // namespace OVR::Util
