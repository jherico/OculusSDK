/************************************************************************************

Filename    :   OptionMenu.h
Content     :   Option selection and editing for OculusWorldDemo
Created     :   March 7, 2014
Authors     :   Michael Antonov, Caleb Leak

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

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


static float Menu_ColorGammaCurve = 1.0f;
static Vector3f Menu_Brightness = Vector3f(1.0f, 1.0f, 1.0f);

void Menu_SetColorGammaCurveAndBrightness(float colorGammaCurve, Vector3f brightness)
{
    Menu_ColorGammaCurve = colorGammaCurve;
    Menu_Brightness = brightness;
}

//-------------------------------------------------------------------------------------
bool OptionShortcut::MatchKey(OVR::KeyCode key, bool shift) const
{
    for (size_t i = 0; i < Keys.size(); i++)
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

bool OptionShortcut::MatchGamepadButton(uint32_t gamepadButtonMask) const
{
    for (size_t i = 0; i < GamepadButtons.size(); i++)
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
std::string OptionMenuItem::PopNamespaceFrom(OptionMenuItem* menuItem)
{
    std::string label = menuItem->Label;
    for (size_t i = 0; i < label.length(); i++)
    {
        if (label.at(i) == '.')
        {
            std::string ns = label.substr(0, i);
            menuItem->Label = label.substr(i + 1, label.length());
            return ns;
        }
    }
    return "";
}

OptionMenuItem *OptionSelectionMenu::FindMenuItem(std::string menuItemLabel)
{
    std::string menuName = menuItemLabel;
    std::string rest = "";
    // Split at the first . (if there is one)
    for (uint32_t i = 0; i < (int) menuItemLabel.length(); i++)
    {
        if (menuItemLabel.at(i) == '.')
        {
            menuName = menuItemLabel.substr(0, i);
            rest = menuItemLabel.substr(i + 1, menuItemLabel.length());
            break;
        }
    }

    // And now go find that submenu.
    for (size_t i = 0; i < Items.size(); i++)
    {
        OptionMenuItem *subItem = Items[i];
        std::string subName = subItem->Label;
        size_t menuNameLength = menuName.length();
        bool namesMatch = false;
        if (0 == strncmp ( subName.c_str(), menuName.c_str(), menuNameLength ) )
        {
            // The actual name may have a keyboard shortcut after it, which we want to ignore.
            // So we need to make "Hello" match "Hello 'Shift+H'" but not "HelloWorld"
            if ( subName.length() > menuNameLength )
            {
                if ( ( ( subName[menuNameLength] == ' ' ) && ( subName[menuNameLength+1] == '\'' ) ) ||
                     ( subName[menuNameLength] == '\'' ) )
                {
                    namesMatch = true;
                }
            }
            else
            {
                namesMatch = true;
            }
        }

        if (namesMatch)
        {
            if ( subItem->IsMenu() )
            {
                OptionSelectionMenu *subMenu = static_cast<OptionSelectionMenu*>(subItem);
                return subMenu->FindMenuItem(rest);
            }
            else
            {
                return subItem;
            }
        }
    }
    return nullptr;
}

//-------------------------------------------------------------------------------------

std::string OptionVar::FormatEnum(OptionVar* var)
{
    uint32_t index = var->GetEnumIndex();
    if (index < var->EnumValues.size())
        return var->EnumValues[index].Name;
    return std::string("<Bad enum index>");    
}

std::string OptionVar::FormatInt(OptionVar* var)
{
    char buff[64];
    snprintf(buff, sizeof(buff), var->FormatString, *var->AsInt());
    return std::string(buff);
}

std::string OptionVar::FormatFloat(OptionVar* var)
{
    char buff[64];
    snprintf(buff, sizeof(buff), var->FormatString, *var->AsFloat() * var->FormatScale);
    return std::string(buff);
}

std::string OptionVar::FormatBool(OptionVar* var)
{
    return *var->AsBool() ? "On" : "Off";
}

std::string OptionVar::FormatTrigger(OptionVar* var)
{
	OVR_UNUSED(var);
    return "[Trigger]";
}


OptionVar::OptionVar(const char* name, void* pvar, VarType type,
                     FormatFunction formatFunction,
                     UpdateFunction updateFunction)
{
    Label       = name;
    Type        = type;
    this->pVar  = pvar;
    fFormat     = ((Type == Type_Trigger) && !formatFunction) ? FormatTrigger : formatFunction;
    fUpdate     = updateFunction;
    pNotify     = 0;
    FormatString= 0;

    MaxFloat    = FLT_MAX;
    MinFloat    = -FLT_MAX;
    StepFloat   = 1.0f;
    FormatScale = 1.0f;

    MaxInt      = 0x7FFFFFFF;
    MinInt      = -(MaxInt) - 1;
    StepInt     = 1;

    SelectedIndex = 0;

    ShortcutUp.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::NextValue);
    ShortcutDown.pNotify = new FunctionNotifyContext<OptionVar, bool>(this, &OptionVar::PrevValue);
}


OptionVar::OptionVar(const char* name, int32_t* pvar,
                     int32_t min, int32_t max, int32_t stepSize,
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

    MaxFloat    = FLT_MAX;
    MinFloat    = -FLT_MAX;
    StepFloat   = 1.0f;
    FormatScale = 1.0f;

    MinInt      = min;
    MaxInt      = max;
    StepInt     = stepSize;

    SelectedIndex = 0;

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

    MaxInt      = 0x7FFFFFFF;
    MinInt      = -(MaxInt) - 1;
    StepInt     = 1;

    SelectedIndex = 0;

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
        *AsInt() = EnumValues[((GetEnumIndex() + 1) % EnumValues.size())].Value;
        break;

    case Type_Int:
        *AsInt() = Alg::Min<int32_t>(*AsInt() + StepInt * (fastStep ? 5 : 1), MaxInt);
        break;

    case Type_Float:
        // TODO: Will behave strange with NaN values.
        *AsFloat() = Alg::Min<float>(*AsFloat() + StepFloat * (fastStep ? 5.0f : 1.0f), MaxFloat);
        break;

    case Type_Bool:
        *AsBool() = !*AsBool();
        break;

    case Type_Trigger:
        break;  // nothing to do

    default: OVR_ASSERT(false); break; // unhandled
    }

    SignalUpdate();
}

void OptionVar::PrevValue(bool* pFastStep)
{
    bool fastStep = (pFastStep != NULL && *pFastStep);
    switch (Type)
    {
    case Type_Enum:
    {
        uint32_t size = (uint32_t)(EnumValues.size() ? EnumValues.size() : 1);
        *AsInt() = EnumValues[((GetEnumIndex() + (size - 1)) % size)].Value;
        break;
    }
    
    case Type_Int:
        *AsInt() = Alg::Max<int32_t>(*AsInt() - StepInt * (fastStep ? 5 : 1), MinInt);
        break;

    case Type_Float:
        // TODO: Will behave strange with NaN values.
        *AsFloat() = Alg::Max<float>(*AsFloat() - StepFloat * (fastStep ? 5.0f : 1.0f), MinFloat);
        break;

    case Type_Bool:
        *AsBool() = !*AsBool();
        break;

    case Type_Trigger:
        break;  // nothing to do

    default: OVR_ASSERT(false); break; // unhandled
    }

    SignalUpdate();
}

// Set value from a string. Returns true on success.
bool OptionVar::SetValue(std::string newVal)
{
    bool success = false;
    switch (Type)
    {
    case Type_Enum:
    {
        success = false;
        for ( auto enumVal : EnumValues )
        {
            if ( 0 == _stricmp (enumVal.Name.c_str(), newVal.c_str() ) )
            {
                *AsInt() = enumVal.Value;
                success = true;
                break;
            }
        }
        break;
    }
    
    case Type_Int:
    {
        int newIntVal = std::stoi ( newVal, nullptr, 10 );
        *AsInt() = newIntVal;
        success = true;
        break;
    }

    case Type_Float:
    {
        float newFloatVal = std::stof ( newVal, nullptr );
        *AsFloat() = newFloatVal;
        success = true;
        break;
    }

    case Type_Bool:
        if ( ( 0 == _stricmp (newVal.c_str(), "false" ) ) ||
             ( 0 == _stricmp (newVal.c_str(), "0" ) ) ||
             ( 0 == _stricmp (newVal.c_str(), "" ) ) )
        {
            *AsBool() = false;
        }
        else
        {
            *AsBool() = true;
        }
        success = true;
        break;

    case Type_Trigger:
        // Nothing to do except cause the trigger (which is still important).
        break;

    default: OVR_ASSERT(false); break; // unhandled
    }

    SignalUpdate();
    return success;
}


std::string OptionVar::HandleShortcutUpdate()
{
    if(Type != Type_Trigger)
    {
        return Label + " - " + GetValue();
    }
    else
    {
        // Avoid double trigger (shortcut key already triggers NextValue())
        return std::string("Triggered: ") + Label;
    }
}

std::string OptionVar::ProcessShortcutKey(OVR::KeyCode key, bool shift)
{
    if (ShortcutUp.MatchKey(key, shift) || ShortcutDown.MatchKey(key, shift))
    {
        return HandleShortcutUpdate();
    }

    return std::string();
}

std::string OptionVar::ProcessShortcutButton(uint32_t buttonMask)
{
    if (ShortcutUp.MatchGamepadButton(buttonMask) || ShortcutDown.MatchGamepadButton(buttonMask))
    {
        return HandleShortcutUpdate();
    }
    return std::string();
}


OptionVar& OptionVar::AddEnumValue(const char* displayName, int32_t value)
{
    EnumEntry entry;
    entry.Name = displayName;
    entry.Value = value;
    EnumValues.push_back(entry);
    return *this;
}

std::string OptionVar::GetValue()
{
    if(fFormat == NULL)
        return std::string();
    else
        return fFormat(this);
}

uint32_t OptionVar::GetEnumIndex()
{
    OVR_ASSERT(Type == Type_Enum);
    OVR_ASSERT(EnumValues.size() > 0);

    // TODO: Change this from a linear search to binary or a hash.
    for (uint32_t i = 0; i < EnumValues.size(); i++)
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

    RenderShortcutChangeMessages = true;

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

    ToggleShortcut.AddShortcut(ShortcutKey(Key_Tab, ShortcutKey::Shift_RequireOff));
    ToggleShortcut.AddShortcut(Gamepad_Start);

    ToggleSingleItemShortcut.AddShortcut(ShortcutKey(Key_Backspace, ShortcutKey::Shift_Ignore));
}

OptionSelectionMenu::~OptionSelectionMenu()
{
    for (size_t i = 0; i < Items.size(); i++)
        delete Items[i];
}

bool OptionSelectionMenu::OnKey(OVR::KeyCode key, int chr, bool down, int modifiers)
{
    bool shift = ((modifiers & Mod_Shift) != 0);
       
    if (down)
    {
        std::string s = ProcessShortcutKey(key, shift);
        if (!s.empty())
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

bool OptionSelectionMenu::OnGamepad(uint32_t buttonMask)
{
    // Check global shortcuts first.
    std::string s = ProcessShortcutButton(buttonMask);
    if (!s.empty())
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

std::string OptionSelectionMenu::ProcessShortcutKey(OVR::KeyCode key, bool shift)
{
    std::string s;

    for (size_t i = 0; (i < Items.size()) && s.empty(); i++)
    {
        s = Items[i]->ProcessShortcutKey(key, shift);
    }

    return s;
}

std::string OptionSelectionMenu::ProcessShortcutButton(uint32_t buttonMask)
{
    std::string s;

    for (size_t i = 0; (i < Items.size()) && s.empty(); i++)
    {
        s = Items[i]->ProcessShortcutButton(buttonMask);
    }

    return s;
}

// Fills in inclusive character range; returns false if line not found.
bool FindLineCharRange(const char* text, int searchLine, size_t charRange[2])
{
    size_t i    = 0;
    
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

Color ApplyGammaCurveAndBrightness(Color inColor, float gammaCurve, Vector3f brightness)
{
    inColor.R = (uint8_t)OVR::Alg::Clamp(pow(OVR::Alg::Clamp(((float)inColor.R) / 255.999f, 0.0f, 1.0f), gammaCurve) * brightness.x * 255.999f, 0.0f, 255.0f);
    inColor.G = (uint8_t)OVR::Alg::Clamp(pow(OVR::Alg::Clamp(((float)inColor.G) / 255.999f, 0.0f, 1.0f), gammaCurve) * brightness.y * 255.999f, 0.0f, 255.0f);
    inColor.B = (uint8_t)OVR::Alg::Clamp(pow(OVR::Alg::Clamp(((float)inColor.B) / 255.999f, 0.0f, 1.0f), gammaCurve) * brightness.z * 255.999f, 0.0f, 255.0f);
    // leave inColor.A intact
    return inColor;
}

Recti OptionSelectionMenu::Render(RenderDevice* prender, std::string title, float textSize, float centerX, float centerY)
{
    // If we are invisible, render shortcut notifications.
    // Both child and parent have visible == true even if only child is shown.
    if ( DisplayState == Display_None )
    {
        if ( RenderShortcutChangeMessages )
        {
            return renderShortcutChangeMessage(prender, textSize, centerX, centerY);
        }
        return Recti ( 0, 0, 0, 0);
    }

    title += Label;

    // Delegate to sub-menu if active.
    if (GetSubmenu() != NULL)
    {
        if (title.size() > 0)
            title += " > ";

        return GetSubmenu()->Render(prender, title, textSize, centerX, centerY);
    }

    Color focusColor(180, 80, 20, 210);
    Color pickedColor(120, 55, 10, 140);
    Color titleColor(0x18, 0x1A, 0x4D, 210);
    Color titleOutlineColor(0x18, 0x18, 0x18, 240);
    Color blueRectColor(40, 40, 100, 210);
    Color textColor(255,255,0,210);

    // convert all colors to requested srgb space
    focusColor          = ApplyGammaCurveAndBrightness(focusColor,           Menu_ColorGammaCurve, Menu_Brightness);
    pickedColor         = ApplyGammaCurveAndBrightness(pickedColor,          Menu_ColorGammaCurve, Menu_Brightness);
    titleColor          = ApplyGammaCurveAndBrightness(titleColor,           Menu_ColorGammaCurve, Menu_Brightness);
    titleOutlineColor   = ApplyGammaCurveAndBrightness(titleOutlineColor,    Menu_ColorGammaCurve, Menu_Brightness);
    blueRectColor       = ApplyGammaCurveAndBrightness(blueRectColor,        Menu_ColorGammaCurve, Menu_Brightness);
    textColor           = ApplyGammaCurveAndBrightness(textColor,            Menu_ColorGammaCurve, Menu_Brightness);

    float    labelsSize[2]     = {0.0f, 0.0f};
    float    bufferSize[2]     = {0.0f, 0.0f};
    float    valuesSize[2]     = {0.0f, 0.0f};
    float    maxValueWidth     = 0.0f;

    size_t   selection[2] = { 0, 0 };
    Vector2f labelSelectionRect[2];
    Vector2f valueSelectionRect[2];

    prender->MeasureText(&DejaVu, "      ", textSize, bufferSize);

    std::string values;
    std::string menuItems;

    int highlightIndex = 0;
    if (DisplayState == Display_Menu)
    {
        highlightIndex = SelectedIndex;
        for (size_t i = 0; i < Items.size(); i++)
        {
            if (i > 0)
                values += "\n";
            values += Items[i]->GetValue();
        }

        for (size_t i = 0; i < Items.size(); i++)
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
    const char* menuItemsCStr = menuItems.c_str();
    bool havelLabelSelection = FindLineCharRange(menuItemsCStr, highlightIndex, selection);
	OVR_UNUSED(havelLabelSelection);
    prender->MeasureText(&DejaVu, menuItemsCStr, textSize, labelsSize,
                         selection, labelSelectionRect);

    // Measure label-to-value gap
    const char* valuesCStr = values.c_str();
    bool haveValueSelection = FindLineCharRange(valuesCStr, highlightIndex, selection);
	OVR_UNUSED(haveValueSelection);
    prender->MeasureText(&DejaVu, valuesCStr, textSize, valuesSize, selection, valueSelectionRect);

    // Measure max value size (absolute size varies, so just use a reasonable max)
    maxValueWidth = prender->MeasureText(&DejaVu, "Max value width", textSize);
    maxValueWidth = Alg::Max(maxValueWidth, valuesSize[0]);

    Vector2f borderSize(4.0f, 4.0f);
    Vector2f totalDimensions = borderSize * 2 + Vector2f(bufferSize[0], 0) + Vector2f(maxValueWidth, 0)
                                + Vector2f(labelsSize[0], labelsSize[1]);
    
    Vector2f fudgeOffset= Vector2f(10.0f, 25.0f);  // This offset looks better
    fudgeOffset.x += centerX;
    fudgeOffset.y += centerY;
    Vector2f topLeft    = (-totalDimensions / 2.0f)  + fudgeOffset;    
    Vector2f bottomRight = topLeft + totalDimensions;

    // If displaying a single item, shift it down.
    if (DisplayState == Display_SingleItem)
    {
        topLeft.y     += textSize * 7;
        bottomRight.y += textSize * 7;
    }

    prender->FillRect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y, blueRectColor);
    Recti bounds;
    bounds.x = (int)floor(topLeft.x);
    bounds.y = (int)floor(topLeft.y);
    bounds.w = (int)ceil(totalDimensions.x);
    bounds.h = (int)ceil(totalDimensions.y);

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
    if ( title.length() == 0 )
    {
        title = "Main menu";
    }
    if (DisplayState == Display_Menu && title.length() > 0)
    {
        Vector2f titleDimensions;
        prender->MeasureText(&DejaVu, title.c_str(), textSize, &titleDimensions.x);
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
                          
        prender->RenderText(&DejaVu, title.c_str(), titleTopLeft.x + borderSize.x,
                            titleTopLeft.y + borderSize.y, textSize, textColor);


        float extraHeight = topLeft.y - titleTopLeft.y;
        bounds.y -= (int)ceil(extraHeight);
        bounds.h += (int)ceil(extraHeight);
    }

    prender->RenderText(&DejaVu, menuItemsCStr, labelsPos.x, labelsPos.y, textSize, textColor);

    prender->RenderText(&DejaVu, valuesCStr, valuesPos.x, valuesPos.y, textSize, textColor);

    return bounds;
}


Recti OptionSelectionMenu::renderShortcutChangeMessage(RenderDevice* prender, float textSize, float centerX, float centerY)
{
    if (ovr_GetTimeInSeconds() < PopupMessageTimeout)
    {
        return DrawTextBox(prender, centerX, centerY + 120.0f, textSize, PopupMessage.c_str(),
                           DrawText_Center | (PopupMessageBorder ? DrawText_Border : 0));
    }
    return Recti(0, 0, 0, 0);
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


void OptionSelectionMenu::SetShortcutChangeMessageEnable ( bool enabled )
{
    RenderShortcutChangeMessages = enabled;
}



void OptionSelectionMenu::AddItem(OptionMenuItem* menuItem)
{
    std::string ns = PopNamespaceFrom(menuItem);

    if (ns.length() == 0)
    {
        Items.push_back(menuItem);
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


OptionSelectionMenu* OptionSelectionMenu::GetOrCreateSubmenu(std::string submenuName)
{
    for (size_t i = 0; i < Items.size(); i++)
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
    Items.push_back(newSubmenu);
    return newSubmenu;
}

void OptionSelectionMenu::HandleUp(bool* pFast)
{
    int numItems = (int)Items.size();
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
        SelectedIndex = ((SelectedIndex + 1) % Items.size());
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

// Returns bounds of render.
Recti DrawTextBox(RenderDevice* prender, float x, float y,
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

    Color rectColor = Color(40, 40, 100, 210);
    Color textColor = Color(255,255,0,210);

    rectColor = ApplyGammaCurveAndBrightness(rectColor, Menu_ColorGammaCurve, Menu_Brightness);
    textColor = ApplyGammaCurveAndBrightness(textColor, Menu_ColorGammaCurve, Menu_Brightness);

    float l = x-borderSize;
    float t = y-borderSize - linesHeight;
    float r = x+ssize[0]+borderSize;
    float b = y+ssize[1]+borderSize + linesHeight;
    prender->FillRect(l, t, r, b, rectColor);

    if (centerType & DrawText_Border)
    {
        // Add top & bottom lines
        float topLineY      = y-borderSize - linesHeight * 0.5f;
        float bottomLineY   = y+ssize[1]+borderSize + linesHeight * 0.5f;

        prender->FillRect(x-borderSize * 0.5f, topLineY,
                          x+ssize[0]+borderSize * 0.5f, topLineY + 2.0f,
                          textColor);
        prender->FillRect(x-borderSize * 0.5f, bottomLineY,
                          x+ssize[0]+borderSize * 0.5f, bottomLineY  + 2.0f,
                          textColor);
    }

    Recti bounds;
    bounds.x = (int)floorf(l);
    bounds.y = (int)floorf(t);
    bounds.w = (int)ceilf(r - l);
    bounds.h = (int)ceilf(b - t);

    prender->RenderText(&DejaVu, text, x, y, textSize, textColor);

    return bounds;
}


Sizef DrawTextMeasure(RenderDevice* prender, float textSize, const char* text)
{
    float ssize[2] = { 0.0f, 0.0f };

    prender->MeasureText(&DejaVu, text, textSize, ssize);
    OVR_UNUSED(prender);

    return Sizef(ssize[0] + 8.0f, ssize[1] + 8.0f);
}


void CleanupDrawTextFont()
{
    if (DejaVu.fill)
    {
        DejaVu.fill->Release();
        DejaVu.fill = 0;
    }
}
