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

#include "OptionMenu.h"

// Embed the font.
#include "../Render/Render_FontEmbed_DejaVu48.h"


//-------------------------------------------------------------------------------------
bool OptionShortcut::MatchKey(OVR::KeyCode key, bool shift) const
{
    for (UInt32 i = 0; i < Keys.GetSize(); i++)
    {
        if (Keys[i].Key != key)
            continue;
        
        if (!shift && Keys[i].ShiftUsage == ShortcutKey::Shift_RequireOn)
            continue;

        if (shift && Keys[i].ShiftUsage == ShortcutKey::Shift_RequireOff)
            continue;

        if(Keys[i].ShiftUsage == ShortcutKey::Shift_Modify)
        {
            pNotify->CallNotify(&shift);
        }
        else
        {
            pNotify->CallNotify();
        }
        return true;
    }
    return false;
}

bool OptionShortcut::MatchGamepadButton(UInt32 gamepadButtonMask) const
{
    for (UInt32 i = 0; i < GamepadButtons.GetSize(); i++)
    {
        if (GamepadButtons[i] & gamepadButtonMask)
        {
            if (pNotify != NULL)
            {
                pNotify->CallNotify();
            }
            return true;
        }
    }
    return false;
}


//-------------------------------------------------------------------------------------
String OptionMenuItem::PopNamespaceFrom(OptionMenuItem* menuItem)
{
    String label = menuItem->Label;
    for (UInt32 i = 0; i < label.GetLength(); i++)
    {
        if (label.GetCharAt(i) == '.')
        {
            String ns = label.Substring(0, i);
            menuItem->Label = label.Substring(i + 1, label.GetLength());
            return ns;
        }
    }
    return "";
}

//-------------------------------------------------------------------------------------

String OptionVar::FormatEnum(OptionVar* var)
{
    UInt32 index = var->GetEnumIndex();
    if (index < var->EnumValues.GetSize())
        return var->EnumValues[index].Name;
    return String("<Bad enum index>");    
}

String OptionVar::FormatInt(OptionVar* var)
{
    char buff[64];
    OVR_sprintf(buff, sizeof(buff), var->FormatString, *var->AsInt());
    return String(buff);
}

String OptionVar::FormatFloat(OptionVar* var)
{
    char buff[64];
    OVR_sprintf(buff, sizeof(buff), var->FormatString, *var->AsFloat() * var->FormatScale);
    return String(buff);
}

String OptionVar::FormatBool(OptionVar* var)
{
    return *var->AsBool() ? "On" : "Off";
}


OptionVar::OptionVar(const char* name, void* pvar, VarType type,
                     FormatFunction formatFunction,
                     UpdateFunction updateFunction)
{
    Label       = name;
    Type        = type;
    this->pVar  = pvar;
    fFormat     = formatFunction;
    fUpdate     = updateFunction;
    pNotify     = 0;
    FormatString= 0;

    MaxFloat    = Math<float>::MaxValue;
    MinFloat    = -Math<float>::MaxValue;
    StepFloat   = 1.0f;
    FormatScale = 1.0f;

    MaxInt      = 0x7FFFFFFF;
    MinInt      = -(MaxInt) - 1;
    StepInt     = 1;

    ShortcutUp.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::NextValue);
    ShortcutDown.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::PrevValue);
}

OptionVar::OptionVar(const char* name, SInt32* pvar,
                     SInt32 min, SInt32 max, SInt32 stepSize,
                     const char* formatString,
                     FormatFunction formatFunction,
                     UpdateFunction updateFunction)
{
    Label       = name;
    Type        = Type_Int;
    this->pVar  = pvar;
    fFormat     = formatFunction ? formatFunction : FormatInt;
    fUpdate     = updateFunction;
    pNotify     = 0;
    FormatString= formatString;

    MinInt      = min;
    MaxInt      = max;
    StepInt     = stepSize;

    ShortcutUp.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::NextValue);
    ShortcutDown.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::PrevValue);
}

// Float with range and step size.
OptionVar::OptionVar(const char* name, float* pvar, 
                     float minf, float maxf, float stepSize,
                     const char* formatString, float formatScale,
                     FormatFunction formatFunction, 
                     UpdateFunction updateFunction)
{
    Label       = name;
    Type        = Type_Float;
    this->pVar  = pvar;
    fFormat     = formatFunction ? formatFunction : FormatFloat;
    fUpdate     = updateFunction;
    pNotify     = 0;
    FormatString= formatString ? formatString : "%.3f";

    MinFloat    = minf;
    MaxFloat    = maxf;
    StepFloat   = stepSize;
    FormatScale = formatScale;

    ShortcutUp.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::NextValue);
    ShortcutDown.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::PrevValue);
}

OptionVar::~OptionVar()
{
    if (pNotify)
        delete pNotify;
}

void OptionVar::NextValue(bool* pFastStep)
{
    bool fastStep = (pFastStep != NULL && *pFastStep);
    switch (Type)
    {
    case Type_Enum:
        *AsInt() = ((GetEnumIndex() + 1) % EnumValues.GetSize());
        break;

    case Type_Int:
        *AsInt() = Alg::Min<SInt32>(*AsInt() + StepInt * (fastStep ? 5 : 1), MaxInt);
        break;

    case Type_Float:
        // TODO: Will behave strange with NaN values.
        *AsFloat() = Alg::Min<float>(*AsFloat() + StepFloat * (fastStep ? 5.0f : 1.0f), MaxFloat);
        break;

    case Type_Bool:
        *AsBool() = !*AsBool();
        break;
    }

    SignalUpdate();
}

void OptionVar::PrevValue(bool* pFastStep)
{
    bool fastStep = (pFastStep != NULL && *pFastStep);
    switch (Type)
    {
    case Type_Enum:
        *AsInt() = ((GetEnumIndex() + (UInt32)EnumValues.GetSize() - 1) % EnumValues.GetSize());
        break;

    case Type_Int:
        *AsInt() = Alg::Max<SInt32>(*AsInt() - StepInt * (fastStep ? 5 : 1), MinInt);
        break;

    case Type_Float:
        // TODO: Will behave strange with NaN values.
        *AsFloat() = Alg::Max<float>(*AsFloat() - StepFloat * (fastStep ? 5.0f : 1.0f), MinFloat);
        break;

    case Type_Bool:
        *AsBool() = !*AsBool();
        break;
    }

    SignalUpdate();
}

String OptionVar::HandleShortcutUpdate()
{
    SignalUpdate();
    return Label + " - " + GetValue();
}

String OptionVar::ProcessShortcutKey(OVR::KeyCode key, bool shift)
{
    if (ShortcutUp.MatchKey(key, shift) || ShortcutDown.MatchKey(key, shift))
    {
        return HandleShortcutUpdate();
    }

    return String();
}

String OptionVar::ProcessShortcutButton(UInt32 buttonMask)
{
    if (ShortcutUp.MatchGamepadButton(buttonMask) || ShortcutDown.MatchGamepadButton(buttonMask))
    {
        return HandleShortcutUpdate();
    }
    return String();
}


OptionVar& OptionVar::AddEnumValue(const char* displayName, SInt32 value)
{
    EnumEntry entry;
    entry.Name = displayName;
    entry.Value = value;
    EnumValues.PushBack(entry);
    return *this;
}

String OptionVar::GetValue()
{
    return fFormat(this);
}

UInt32 OptionVar::GetEnumIndex()
{
    OVR_ASSERT(Type == Type_Enum);
    OVR_ASSERT(EnumValues.GetSize() > 0);

    // TODO: Change this from a linear search to binary or a hash.
    for (UInt32 i = 0; i < EnumValues.GetSize(); i++)
    {
        if (EnumValues[i].Value == *AsInt())
            return i;
    }

    // Enum values should always be found.
    OVR_ASSERT(false);
    return 0;
}


//-------------------------------------------------------------------------------------

OptionSelectionMenu::OptionSelectionMenu(OptionSelectionMenu* parentMenu)
{
    DisplayState	= Display_None;
    SelectedIndex 	= 0;
    SelectionActive = false;
    ParentMenu 		= parentMenu;

    PopupMessageTimeout = 0.0;
    PopupMessageBorder  = false;

    // Setup handlers for menu navigation actions.
    NavShortcuts[Nav_Up].pNotify = new FunctionNotifyContext<OptionSelectionMenu, bool>(this, &OptionSelectionMenu::HandleUp);
    NavShortcuts[Nav_Down].pNotify = new FunctionNotifyContext<OptionSelectionMenu, bool>(this, &OptionSelectionMenu::HandleDown);
    NavShortcuts[Nav_Left].pNotify = new FunctionNotifySimple<OptionSelectionMenu>(this, &OptionSelectionMenu::HandleLeft);
    NavShortcuts[Nav_Right].pNotify = new FunctionNotifySimple<OptionSelectionMenu>(this, &OptionSelectionMenu::HandleRight);
    NavShortcuts[Nav_Select].pNotify = new FunctionNotifySimple<OptionSelectionMenu>(this, &OptionSelectionMenu::HandleSelect);
    NavShortcuts[Nav_Back].pNotify = new FunctionNotifySimple<OptionSelectionMenu>(this, &OptionSelectionMenu::HandleBack);
    ToggleShortcut.pNotify = new FunctionNotifySimple<OptionSelectionMenu>(this, &OptionSelectionMenu::HandleMenuToggle);
    ToggleSingleItemShortcut.pNotify = new FunctionNotifySimple<OptionSelectionMenu>(this, &OptionSelectionMenu::HandleSingleItemToggle);

    // Bind keys and buttons to menu navigation actions.
    NavShortcuts[Nav_Up].AddShortcut(ShortcutKey(Key_Up, ShortcutKey::Shift_Modify));
    NavShortcuts[Nav_Up].AddShortcut(Gamepad_Up);

    NavShortcuts[Nav_Down].AddShortcut(ShortcutKey(Key_Down, ShortcutKey::Shift_Modify));
    NavShortcuts[Nav_Down].AddShortcut(Gamepad_Down);

    NavShortcuts[Nav_Left].AddShortcut(ShortcutKey(Key_Left));
    NavShortcuts[Nav_Left].AddShortcut(Gamepad_Left);

    NavShortcuts[Nav_Right].AddShortcut(ShortcutKey(Key_Right));
    NavShortcuts[Nav_Right].AddShortcut(Gamepad_Right);

    NavShortcuts[Nav_Select].AddShortcut(ShortcutKey(Key_Return));
    NavShortcuts[Nav_Select].AddShortcut(Gamepad_A);

    NavShortcuts[Nav_Back].AddShortcut(ShortcutKey(Key_Escape));
    NavShortcuts[Nav_Back].AddShortcut(Gamepad_B);

    ToggleShortcut.AddShortcut(ShortcutKey(Key_Tab, ShortcutKey::Shift_Ignore));
    ToggleShortcut.AddShortcut(Gamepad_Start);

    ToggleSingleItemShortcut.AddShortcut(ShortcutKey(Key_Backspace, ShortcutKey::Shift_Ignore));
}

OptionSelectionMenu::~OptionSelectionMenu()
{
    for (UInt32 i = 0; i < Items.GetSize(); i++)
        delete Items[i];
}

bool OptionSelectionMenu::OnKey(OVR::KeyCode key, int chr, bool down, int modifiers)
{
    bool shift = ((modifiers & Mod_Shift) != 0);
       
    if (down)
    {
        String s = ProcessShortcutKey(key, shift);
        if (!s.IsEmpty())
        {
            PopupMessage  = s;
            PopupMessageTimeout = ovr_GetTimeInSeconds() + 4.0f;
            PopupMessageBorder = false;
            return true;
        }
    }

    if (GetSubmenu() != NULL)
    {
        return GetSubmenu()->OnKey(key, chr, down, modifiers);
    }

    if (down)
    {
        if (ToggleShortcut.MatchKey(key, shift))
            return true;

        if (ToggleSingleItemShortcut.MatchKey(key, shift))
            return true;

        if (DisplayState == Display_None)
            return false;

        for (int i = 0; i < Nav_LAST; i++)
        {
            if (NavShortcuts[i].MatchKey(key, shift))
                return true;
        }
    }

    // Let the caller process keystroke
    return false;
}

bool OptionSelectionMenu::OnGamepad(UInt32 buttonMask)
{
    // Check global shortcuts first.
    String s = ProcessShortcutButton(buttonMask);
    if (!s.IsEmpty())
    {
        PopupMessage  = s;
        PopupMessageTimeout = ovr_GetTimeInSeconds() + 4.0f;
        return true;
    }

    if (GetSubmenu() != NULL)
    {
        return GetSubmenu()->OnGamepad(buttonMask);
    }

    if (ToggleShortcut.MatchGamepadButton(buttonMask))
        return true;

    if (DisplayState == Display_None)
        return false;

    for (int i = 0; i < Nav_LAST; i++)
    {
        if (NavShortcuts[i].MatchGamepadButton(buttonMask))
            return true;
    }

    // Let the caller process keystroke
    return false;
}

String OptionSelectionMenu::ProcessShortcutKey(OVR::KeyCode key, bool shift)
{
    String s;

    for (UPInt i = 0; (i < Items.GetSize()) && s.IsEmpty(); i++)
    {
        s = Items[i]->ProcessShortcutKey(key, shift);
    }

    return s;
}

String OptionSelectionMenu::ProcessShortcutButton(UInt32 buttonMask)
{
    String s;

    for (UPInt i = 0; (i < Items.GetSize()) && s.IsEmpty(); i++)
    {
        s = Items[i]->ProcessShortcutButton(buttonMask);
    }

    return s;
}

// Fills in inclusive character range; returns false if line not found.
bool FindLineCharRange(const char* text, int searchLine, UPInt charRange[2])
{
    UPInt i    = 0;
    
    for (int line = 0; line <= searchLine; line ++)
    {
        if (line == searchLine)
        {
            charRange[0] = i;
        }

        // Find end of line.
        while (text[i] != '\n' && text[i] != 0)
        {
            i++;
        }

        if (line == searchLine)
        {            
            charRange[1] = (charRange[0] == i) ? charRange[0] : i-1;
            return true;
        }

        if (text[i] == 0)
            break;
        // Skip newline
        i++;
    }

    return false;
}


void OptionSelectionMenu::Render(RenderDevice* prender, String title)
{
    // If we are invisible, render shortcut notifications.
    // Both child and parent have visible == true even if only child is shown.
    if (DisplayState == Display_None)
    {
        renderShortcutChangeMessage(prender);
        return;
    }

    title += Label;

    // Delegate to sub-menu if active.
    if (GetSubmenu() != NULL)
    {
        if (title.GetSize() > 0)
            title += " > ";

        GetSubmenu()->Render(prender, title);
        return;
    }

    Color focusColor(180, 80, 20, 210);
    Color pickedColor(120, 55, 10, 140);
    Color titleColor(0x18, 0x1A, 0x4D, 210);
    Color titleOutlineColor(0x18, 0x18, 0x18, 240);

    float    labelsSize[2]     = {0.0f, 0.0f};
    float    bufferSize[2]     = {0.0f, 0.0f};
    float    valuesSize[2]     = {0.0f, 0.0f};
    float    maxValueWidth     = 0.0f;

    UPInt    selection[2] = { 0, 0 };
    Vector2f labelSelectionRect[2];
    Vector2f valueSelectionRect[2];
    bool     havelLabelSelection = false;
    bool     haveValueSelection = false;

    float textSize = 22.0f;
    prender->MeasureText(&DejaVu, "      ", textSize, bufferSize);

    String values;
    String menuItems;

    int highlightIndex = 0;
    if (DisplayState == Display_Menu)
    {
        highlightIndex = SelectedIndex;
        for (UInt32 i = 0; i < Items.GetSize(); i++)
        {
            if (i > 0)
                values += "\n";
            values += Items[i]->GetValue();
        }

        for (UInt32 i = 0; i < Items.GetSize(); i++)
        {
            if (i > 0)
                menuItems += "\n";
            menuItems += Items[i]->GetLabel();
        }
    }
    else
    {
        values = Items[SelectedIndex]->GetValue();
        menuItems = Items[SelectedIndex]->GetLabel();
    }

    // Measure labels
    const char* menuItemsCStr = menuItems.ToCStr();
    havelLabelSelection = FindLineCharRange(menuItemsCStr, highlightIndex, selection);
    prender->MeasureText(&DejaVu, menuItemsCStr, textSize, labelsSize,
                         selection, labelSelectionRect);

    // Measure label-to-value gap
    const char* valuesCStr = values.ToCStr();
    haveValueSelection = FindLineCharRange(valuesCStr, highlightIndex, selection);
    prender->MeasureText(&DejaVu, valuesCStr, textSize, valuesSize, selection, valueSelectionRect);

    // Measure max value size (absolute size varies, so just use a reasonable max)
    maxValueWidth = prender->MeasureText(&DejaVu, "Max value width", textSize);
    maxValueWidth = Alg::Max(maxValueWidth, valuesSize[0]);

    Vector2f borderSize(4.0f, 4.0f);
    Vector2f totalDimensions = borderSize * 2 + Vector2f(bufferSize[0], 0) + Vector2f(maxValueWidth, 0)
                                + Vector2f(labelsSize[0], labelsSize[1]);
    
    Vector2f fudgeOffset= Vector2f(10.0f, 25.0f);  // This offset looks better
    Vector2f topLeft    = (-totalDimensions / 2.0f)  + fudgeOffset;    
    Vector2f bottomRight = topLeft + totalDimensions;

    // If displaying a single item, shift it down.
    if (DisplayState == Display_SingleItem)
    {
        topLeft.y     += textSize * 7;
        bottomRight.y += textSize * 7;
    }

    prender->FillRect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y, Color(40,40,100,210));

    Vector2f labelsPos = topLeft + borderSize;
    Vector2f valuesPos = labelsPos + Vector2f(labelsSize[0], 0) + Vector2f(bufferSize[0], 0);

    // Highlight selected label
    Vector2f selectionInset = Vector2f(0.3f, 2.0f);
    if (DisplayState == Display_Menu)
    {
        Vector2f labelSelectionTopLeft = labelsPos + labelSelectionRect[0] - selectionInset;
        Vector2f labelSelectionBottomRight = labelsPos + labelSelectionRect[1] + selectionInset;

        prender->FillRect(labelSelectionTopLeft.x, labelSelectionTopLeft.y,
                          labelSelectionBottomRight.x, labelSelectionBottomRight.y,
                          SelectionActive ? pickedColor : focusColor);
    }

    // Highlight selected value if active
    if (SelectionActive)
    {
        Vector2f valueSelectionTopLeft = valuesPos + valueSelectionRect[0] - selectionInset;
        Vector2f valueSelectionBottomRight = valuesPos + valueSelectionRect[1] + selectionInset;
        prender->FillRect(valueSelectionTopLeft.x, valueSelectionTopLeft.y,
                          valueSelectionBottomRight.x, valueSelectionBottomRight.y,
                          focusColor);
    }

    // Measure and draw title
    if (DisplayState == Display_Menu && title.GetLength() > 0)
    {
        Vector2f titleDimensions;
        prender->MeasureText(&DejaVu, title.ToCStr(), textSize, &titleDimensions.x);
        Vector2f titleTopLeft = topLeft - Vector2f(0, borderSize.y) * 2 - Vector2f(0, titleDimensions.y);
        titleDimensions.x = totalDimensions.x;
        
        prender->FillRect(titleTopLeft.x, titleTopLeft.y,
                          titleTopLeft.x + totalDimensions.x,
                          titleTopLeft.y + titleDimensions.y + borderSize.y * 2,
                          titleOutlineColor);
        
        prender->FillRect(titleTopLeft.x + borderSize.x / 2, titleTopLeft.y + borderSize.y / 2,
                          titleTopLeft.x + totalDimensions.x - borderSize.x / 2,
                          titleTopLeft.y + borderSize.y / 2 + titleDimensions.y,
                          titleColor);
                          
        prender->RenderText(&DejaVu, title.ToCStr(), titleTopLeft.x + borderSize.x,
                            titleTopLeft.y + borderSize.y, textSize, Color(255,255,0,210));
    }

    prender->RenderText(&DejaVu, menuItemsCStr, labelsPos.x, labelsPos.y, textSize, Color(255,255,0,210));

    prender->RenderText(&DejaVu, valuesCStr, valuesPos.x, valuesPos.y, textSize, Color(255,255,0,210));
}


void OptionSelectionMenu::renderShortcutChangeMessage(RenderDevice* prender)
{
    if (ovr_GetTimeInSeconds() < PopupMessageTimeout)
    {
        DrawTextBox(prender, 0, 120, 22.0f, PopupMessage.ToCStr(),
                    DrawText_Center | (PopupMessageBorder ? DrawText_Border : 0));
    }
}


void OptionSelectionMenu::SetPopupMessage(const char* format, ...)
{
    //Lock::Locker lock(pManager->GetHandlerLock());
    char textBuff[2048];
    va_list argList;
    va_start(argList, format);
    OVR_vsprintf(textBuff, sizeof(textBuff), format, argList);
    va_end(argList);

    // Message will time out in 4 seconds.
    PopupMessage = textBuff;
    PopupMessageTimeout = ovr_GetTimeInSeconds() + 4.0f;
    PopupMessageBorder = false;
}

void OptionSelectionMenu::SetPopupTimeout(double timeoutSeconds, bool border)
{
    PopupMessageTimeout = ovr_GetTimeInSeconds() + timeoutSeconds;
    PopupMessageBorder = border;
}



void OptionSelectionMenu::AddItem(OptionMenuItem* menuItem)
{
    String ns = PopNamespaceFrom(menuItem);

    if (ns.GetLength() == 0)
    {
        Items.PushBack(menuItem);
    }
    else
    {
        // Item is part of a submenu, add it to that instead.
        GetOrCreateSubmenu(ns)->AddItem(menuItem);
    }
}

//virtual 
void OptionSelectionMenu::Select()
{
    SelectedIndex = 0;
    SelectionActive = false;
    DisplayState = Display_Menu;
}


OptionSelectionMenu* OptionSelectionMenu::GetSubmenu()
{
    if (!SelectionActive || !Items[SelectedIndex]->IsMenu())
        return NULL;

    OptionSelectionMenu* submenu = static_cast<OptionSelectionMenu*>(Items[SelectedIndex]);
    return submenu;
}


OptionSelectionMenu* OptionSelectionMenu::GetOrCreateSubmenu(String submenuName)
{
    for (UInt32 i = 0; i < Items.GetSize(); i++)
    {
        if (!Items[i]->IsMenu())
            continue;

        OptionSelectionMenu* submenu = static_cast<OptionSelectionMenu*>(Items[i]);

        if (submenu->Label == submenuName)
        {
            return submenu;
        }
    }

    // Submenu doesn't exist, create it.
    OptionSelectionMenu* newSubmenu = new OptionSelectionMenu(this);
    newSubmenu->Label = submenuName;
    Items.PushBack(newSubmenu);
    return newSubmenu;
}

void OptionSelectionMenu::HandleUp(bool* pFast)
{
    int numItems = (int)Items.GetSize();
    if (SelectionActive)
        Items[SelectedIndex]->NextValue(pFast);
    else
        SelectedIndex = ((SelectedIndex - 1 + numItems) % numItems);
}

void OptionSelectionMenu::HandleDown(bool* pFast)
{
    if (SelectionActive)
        Items[SelectedIndex]->PrevValue(pFast);
    else
        SelectedIndex = ((SelectedIndex + 1) % Items.GetSize());
}

void OptionSelectionMenu::HandleLeft()
{
    if (DisplayState != Display_Menu)
        return;

    if (SelectionActive)
        SelectionActive = false;
    else if (ParentMenu)
    {
        // Escape to parent menu
        ParentMenu->SelectionActive = false;
        DisplayState = Display_Menu;
    }
}

void OptionSelectionMenu::HandleRight()
{
    if (DisplayState != Display_Menu)
        return;

    if (!SelectionActive)
    {
        SelectionActive = true;
        Items[SelectedIndex]->Select();
    }
}

void OptionSelectionMenu::HandleSelect()
{
    if (!SelectionActive)
    {
        SelectionActive = true;
        Items[SelectedIndex]->Select();
    }
    else
    {
        Items[SelectedIndex]->NextValue();
    }
}

void OptionSelectionMenu::HandleBack()
{
    if (DisplayState != Display_Menu)
        return;

    if (!SelectionActive)
        DisplayState = Display_None;
    else
        SelectionActive = false;
}

void OptionSelectionMenu::HandleMenuToggle()
{
    // Mark this & parent With correct visibility.
    OptionSelectionMenu* menu = this;
    
    if (DisplayState == Display_Menu)
        DisplayState = Display_None;
    else
        DisplayState = Display_Menu;

    while (menu)
    {
        menu->DisplayState = DisplayState;
        menu = menu->ParentMenu;
    }
    // Hide message
    PopupMessageTimeout = 0;
}

void OptionSelectionMenu::HandleSingleItemToggle()
{
    // Mark this & parent With correct visibility.
    OptionSelectionMenu* menu = this;
    
    if (DisplayState == Display_SingleItem)
        DisplayState = Display_None;
    else
    {
        DisplayState = Display_SingleItem;
        SelectionActive = true;
    }

    while (menu)
    {
        menu->DisplayState = DisplayState;
        menu = menu->ParentMenu;
    }
    // Hide message
    PopupMessageTimeout = 0;
}


//-------------------------------------------------------------------------------------
// **** Text Rendering / Management 

void DrawTextBox(RenderDevice* prender, float x, float y,
                 float textSize, const char* text, unsigned centerType)
{
    float ssize[2] = {0.0f, 0.0f};

    prender->MeasureText(&DejaVu, text, textSize, ssize);

    // Treat 0 a VCenter.
    if (centerType & DrawText_HCenter)
    {
        x -= ssize[0]/2;
    }
    if (centerType & DrawText_VCenter)
    {
        y -= ssize[1]/2;
    }

    const float borderSize = 4.0f;
    float       linesHeight = 0.0f;

    if (centerType & DrawText_Border)    
        linesHeight = 10.0f;    

    prender->FillRect(x-borderSize, y-borderSize - linesHeight,
                      x+ssize[0]+borderSize, y+ssize[1]+borderSize + linesHeight,
                      Color(40,40,100,210));

    if (centerType & DrawText_Border)
    {
        // Add top & bottom lines
        float topLineY      = y-borderSize - linesHeight * 0.5f,
              bottomLineY   = y+ssize[1]+borderSize + linesHeight * 0.5f;

        prender->FillRect(x-borderSize * 0.5f, topLineY,
                          x+ssize[0]+borderSize * 0.5f, topLineY + 2.0f,
                          Color(255,255,0,210));
        prender->FillRect(x-borderSize * 0.5f, bottomLineY,
                          x+ssize[0]+borderSize * 0.5f, bottomLineY  + 2.0f,
                          Color(255,255,0,210));
    }

    prender->RenderText(&DejaVu, text, x, y, textSize, Color(255,255,0,210));
}

void CleanupDrawTextFont()
{
    if (DejaVu.fill)
    {
        DejaVu.fill->Release();
        DejaVu.fill = 0;
    }
}
