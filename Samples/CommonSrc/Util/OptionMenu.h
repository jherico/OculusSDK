/************************************************************************************

Filename    :   OptionMenu.h
Content     :   Option selection and editing for OculusWorldDemo
Created     :   March 7, 2014
Authors     :   Michael Antonov, Caleb Leak

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

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

#ifndef INC_OptionMenu_h
#define INC_OptionMenu_h

#include "OVR.h"

#include "../Platform/Platform_Default.h"
#include "../Render/Render_Device.h"
#include "../Platform/Gamepad.h"

#include "Util/Util_Render_Stereo.h"
using namespace OVR::Util::Render;

#include <Kernel/OVR_SysFile.h>
#include <Kernel/OVR_Log.h>
#include <Kernel/OVR_Timer.h>

#include "OVR_DeviceConstants.h"


using namespace OVR;
using namespace OVR::Platform;
using namespace OVR::Render;


//-------------------------------------------------------------------------------------
struct FunctionNotifyBase : public NewOverrideBase
{
    virtual void CallNotify(void*) { }
    virtual void CallNotify() { }
};

// Simple member pointer wrapper to support calling class members
template<class C, class X>
struct FunctionNotifyContext : public FunctionNotifyBase
{
    typedef void (C::*FnPtr)(X*);

    FunctionNotifyContext(C* p, FnPtr fn) : pClass(p), pFn(fn), pContext(NULL) { }
    FunctionNotifyContext(C* p, FnPtr fn, X* pContext) : pClass(p), pFn(fn), pContext(pContext) { }
    virtual void CallNotify(void* var)  { (void)(pClass->*pFn)(static_cast<X*>(var)); }
    virtual void CallNotify()  { (void)(pClass->*pFn)(pContext); }
private:

    X*      pContext;
    C*      pClass;
    FnPtr   pFn;
};

template<class C>
struct FunctionNotifySimple : public FunctionNotifyBase
{
    typedef void (C::*FnPtr)(void);

    FunctionNotifySimple(C* p, FnPtr fn) : pClass(p), pFn(fn) { }
    virtual void CallNotify(void*)  { CallNotify(); }
    virtual void CallNotify()  { (void)(pClass->*pFn)(); }
private:

    C*      pClass;
    FnPtr   pFn;
};


//-------------------------------------------------------------------------------------
// Describes a shortcut key 
struct ShortcutKey
{
    enum ShiftUsageType
    {
        Shift_Ignore,
        Shift_Modify,
        Shift_RequireOn,
        Shift_RequireOff
    };

    ShortcutKey(OVR::KeyCode key = Key_None, ShiftUsageType shiftUsage = Shift_RequireOff)
        : Key(key), ShiftUsage(shiftUsage) { }

    OVR::KeyCode        Key;
    ShiftUsageType ShiftUsage;
};


//-------------------------------------------------------------------------------------
struct OptionShortcut
{
    Array<ShortcutKey>  Keys;
    Array<UInt32>       GamepadButtons;
    FunctionNotifyBase* pNotify;

    OptionShortcut() : pNotify(NULL) {}
    OptionShortcut(FunctionNotifyBase* pNotify) : pNotify(pNotify) {}
    ~OptionShortcut()  { if (pNotify) delete pNotify; }

    void AddShortcut(ShortcutKey key) { Keys.PushBack(key); }
    void AddShortcut(UInt32 gamepadButton) { GamepadButtons.PushBack(gamepadButton); }

    bool MatchKey(OVR::KeyCode key, bool shift) const;
    bool MatchGamepadButton(UInt32 gamepadButtonMask) const;
};


//-------------------------------------------------------------------------------------

// Base class for a menu item. Internally, this can also be OptionSelectionMenu itself
// to support nested menus. Users shouldn't need to use this class.
class OptionMenuItem : public NewOverrideBase
{
public:
    virtual ~OptionMenuItem() { }

    virtual void    Select() { }
        
    virtual void    NextValue(bool* pFastStep = NULL) { OVR_UNUSED1(pFastStep); }
    virtual void    PrevValue(bool* pFastStep = NULL) { OVR_UNUSED1(pFastStep); }

    virtual String  GetLabel() { return Label; }
    virtual String  GetValue() { return ""; }

    // Returns empty string if shortcut not handled
    virtual String  ProcessShortcutKey(OVR::KeyCode key, bool shift) = 0;
    virtual String  ProcessShortcutButton(UInt32 buttonMask) = 0;

    virtual bool    IsMenu() const { return false; }

protected:
    String          Label;
    String          PopNamespaceFrom(OptionMenuItem* menuItem);
};


//-------------------------------------------------------------------------------------

// OptionVar implements a basic menu item, which binds to an external variable,
// displaying and editing its state.
class OptionVar : public OptionMenuItem
{
public:

    enum VarType
    {
        Type_Enum,
        Type_Int,
        Type_Float,
        Type_Bool
    };

    typedef String (*FormatFunction)(OptionVar*);
    typedef void (*UpdateFunction)(OptionVar*);
    
    static String FormatEnum(OptionVar* var);
    static String FormatInt(OptionVar* var);
    static String FormatFloat(OptionVar* var);
    static String FormatTan(OptionVar* var);
    static String FormatBool(OptionVar* var);

    OptionVar(const char* name, void* pVar, VarType type,
              FormatFunction formatFunction,
              UpdateFunction updateFunction = NULL);

    // Integer with range and step size.
    OptionVar(const char* name, SInt32* pVar,
              SInt32 min, SInt32 max, SInt32 stepSize=1,
              const char* formatString = "%d",
              FormatFunction formatFunction = 0, // Default int formatting.
              UpdateFunction updateFunction = NULL);

    // Float with range and step size.
    OptionVar(const char* name, float* pvar, 
              float minf, float maxf, float stepSize = 1.0f,
              const char* formatString = "%.3f", float formatScale = 1.0f,
              FormatFunction formatFunction = 0, // Default float formatting.
              UpdateFunction updateFunction = 0 );

    virtual ~OptionVar();

    SInt32* AsInt()         { return reinterpret_cast<SInt32*>(pVar); }
    bool*   AsBool()        { return reinterpret_cast<bool*>(pVar); }
    float*  AsFloat()       { return reinterpret_cast<float*>(pVar); }
    VarType GetType()       { return Type; }

    // Step through values (wrap for enums).
    virtual void NextValue(bool* pFastStep);
    virtual void PrevValue(bool* pFastStep);

    // Executes shortcut message and returns notification string.
    // Returns empty string for no action.
    String         HandleShortcutUpdate();
    virtual String ProcessShortcutKey(OVR::KeyCode key, bool shift);
    virtual String ProcessShortcutButton(UInt32 buttonMask);

    OptionVar& AddEnumValue(const char* displayName, SInt32 value);

    template<class C>
    OptionVar& SetNotify(C* p, void (C::*fn)(OptionVar*))
    {
        OVR_ASSERT(pNotify == 0); // Can't set notifier twice.
        pNotify = new FunctionNotifyContext<C, OptionVar>(p, fn, this);
        return *this;
    }   


    //String Format();
    virtual String GetValue();

    OptionVar& AddShortcutUpKey(const ShortcutKey& shortcut)
    { ShortcutUp.AddShortcut(shortcut); return *this; }
    OptionVar& AddShortcutUpKey(OVR::KeyCode key,
                                ShortcutKey::ShiftUsageType shiftUsage = ShortcutKey::Shift_Modify)
    { ShortcutUp.AddShortcut(ShortcutKey(key, shiftUsage)); return *this; }
    OptionVar& AddShortcutUpButton(UInt32 gamepadButton)
    { ShortcutUp.AddShortcut(gamepadButton); return *this; }

    OptionVar& AddShortcutDownKey(const ShortcutKey& shortcut)
    { ShortcutDown.AddShortcut(shortcut); return *this; }
    OptionVar& AddShortcutDownKey(OVR::KeyCode key,
                                  ShortcutKey::ShiftUsageType shiftUsage = ShortcutKey::Shift_Modify)
    { ShortcutDown.AddShortcut(ShortcutKey(key, shiftUsage)); return *this; }
    OptionVar& AddShortcutDownButton(UInt32 gamepadButton)
    { ShortcutDown.AddShortcut(gamepadButton); return *this; }
    
    OptionVar& AddShortcutKey(const ShortcutKey& shortcut)
    { return AddShortcutUpKey(shortcut); }
    OptionVar& AddShortcutKey(OVR::KeyCode key,
                               ShortcutKey::ShiftUsageType shiftUsage = ShortcutKey::Shift_RequireOff)
    { return AddShortcutUpKey(key, shiftUsage); }
    OptionVar& AddShortcutButton(UInt32 gamepadButton)
    { return AddShortcutUpButton(gamepadButton); }

private:

    void SignalUpdate()
    {
        if (fUpdate) fUpdate(this);
        if (pNotify) pNotify->CallNotify(this);
    }

    struct EnumEntry
    {
        // Human readable name for enum.
        String Name;
        SInt32 Value;
    };    

    // Array of possible enum values.
    Array<EnumEntry>    EnumValues;
    // Gets the index of the current enum value.
    UInt32              GetEnumIndex();

    FormatFunction      fFormat;
    UpdateFunction      fUpdate;
    FunctionNotifyBase* pNotify; 

    VarType         Type;
    void*           pVar;
    const char*     FormatString;
    
    OptionShortcut  ShortcutUp;
    OptionShortcut  ShortcutDown;

    float       MinFloat;
    float       MaxFloat;
    float       StepFloat;
    float       FormatScale; // Multiply float by this before rendering

    SInt32      MinInt;
    SInt32      MaxInt;
    SInt32      StepInt;
};


//-------------------------------------------------------------------------------------
// ***** OptionSelectionMenu

// Implements an overlay option menu, brought up by the 'Tab' key.
// Items are added to the menu with AddBool, AddEnum, AddFloat on startup,
// and are editable by using arrow keys (underlying variable is modified).
// 
// Call Render() to render the menu every frame.
//
// Menu also support displaying popup messages with a timeout, displayed
// when menu body isn't up.

class OptionSelectionMenu : public OptionMenuItem
{
public:
    OptionSelectionMenu(OptionSelectionMenu* parentMenu = NULL);
    ~OptionSelectionMenu();


    bool OnKey(OVR::KeyCode key, int chr, bool down, int modifiers);
    bool OnGamepad(UInt32 buttonMask);

    void Render(RenderDevice* prender, String title = "");
    
    void AddItem(OptionMenuItem* menuItem);

    // Adds a boolean toggle. Returns added item to allow customization.
    OptionVar& AddBool(const char* name, bool* pvar,
                       OptionVar::UpdateFunction updateFunction = 0,
                       OptionVar::FormatFunction formatFunction = OptionVar::FormatBool)
    {
        OptionVar* p = new OptionVar(name, pvar, OptionVar::Type_Bool,
                                     formatFunction, updateFunction);
        AddItem(p);
        return *p;
    }

    // Adds a boolean toggle. Returns added item to allow customization.
    OptionVar& AddEnum(const char* name, void* pvar,
                       OptionVar::UpdateFunction updateFunction = 0)
    {
        OptionVar* p = new OptionVar(name, pvar, OptionVar::Type_Enum,
                                     OptionVar::FormatEnum, updateFunction);
        AddItem(p);
        return *p;
    }


    // Adds a Float variable. Returns added item to allow customization.
    OptionVar& AddFloat( const char* name, float* pvar, 
                         float minf, float maxf, float stepSize = 1.0f,
                         const char* formatString = "%.3f", float formatScale = 1.0f,
                         OptionVar::FormatFunction formatFunction = 0, // Default float formatting.
                         OptionVar::UpdateFunction updateFunction = 0 )
    {
        OptionVar* p = new OptionVar(name, pvar, minf, maxf, stepSize,
                                     formatString, formatScale,
                                     formatFunction, updateFunction);
        AddItem(p);
        return *p;
    }

    virtual void    Select();
    virtual String  GetLabel() { return Label + " >"; }

    virtual String  ProcessShortcutKey(OVR::KeyCode key, bool shift);
    virtual String  ProcessShortcutButton(UInt32 buttonMask);
    
    // Sets a message to display with a time-out. Default time-out is 4 seconds.
    // This uses the same overlay approach as used for shortcut notifications.
    void            SetPopupMessage(const char* format, ...);
    // Overrides current timeout, in seconds (not the future default value);
    // intended to be called right after SetPopupMessage.
    void            SetPopupTimeout(double timeoutSeconds, bool border = false);

    virtual bool    IsMenu() const { return true; }

protected:

    void renderShortcutChangeMessage(RenderDevice* prender);

public:
    OptionSelectionMenu* GetSubmenu();
    OptionSelectionMenu* GetOrCreateSubmenu(String submenuName);

    enum DisplayStateType
    {
        Display_None,
        Display_Menu,
        Display_SingleItem
    };

    DisplayStateType          DisplayState;
    OptionSelectionMenu*      ParentMenu;
   
    ArrayPOD<OptionMenuItem*> Items;
    int                       SelectedIndex;
    bool                      SelectionActive;

    String                    PopupMessage;
    double                    PopupMessageTimeout;
    bool                      PopupMessageBorder;

    // Possible menu navigation actions.
    enum NavigationActions
    {
        Nav_Up,
        Nav_Down,
        Nav_Left,
        Nav_Right,
        Nav_Select,
        Nav_Back,
        Nav_LAST
    };

    // Handlers for navigation actions.
    void HandleUp(bool* pFast);
    void HandleDown(bool* pFast);
    void HandleLeft();
    void HandleRight();
    void HandleSelect();
    void HandleBack();

    void HandleMenuToggle();
    void HandleSingleItemToggle();

    OptionShortcut NavShortcuts[Nav_LAST];
    OptionShortcut ToggleShortcut;
    OptionShortcut ToggleSingleItemShortcut;
};


//-------------------------------------------------------------------------------------
// Text Rendering Utility
enum DrawTextCenterType
{
    DrawText_NoCenter= 0,
    DrawText_VCenter = 0x01,
    DrawText_HCenter = 0x02,
    DrawText_Center  = DrawText_VCenter | DrawText_HCenter,
    DrawText_Border  = 0x10,
};

void    DrawTextBox(RenderDevice* prender, float x, float y,
                    float textSize, const char* text,
                    unsigned centerType = DrawText_NoCenter);

void    CleanupDrawTextFont();


#endif // INC_OptionMenu_h
