/************************************************************************************

Filename    :   Util_SystemGUI.cpp
Content     :   OS GUI access, usually for diagnostics.
Created     :   October 20, 2014
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

#include "Util_SystemGUI.h"
#include "../Kernel/OVR_UTF8Util.h"
#include <stdio.h>

#if defined(OVR_OS_MS)
    #include <Windows.h>
#endif


namespace OVR { namespace Util {


#if defined(OVR_OS_MS)

    // On Windows we implement a manual dialog message box. The reason for this is that there's no way to 
    // have a message box like this without either using MFC or WinForms or relying on Windows Vista+.

    bool DisplayMessageBox(const char* pTitle, const char* pText)
    {
        #define ID_EDIT 100

        struct Dialog
        {
            static size_t LineCount(const char* pText)
            {
                size_t count = 0;
                while(*pText)
                {
                    if(*pText++ == '\n')
                        count++;
                }
                return count;
            }

            static WORD* WordUp(WORD* pIn){ return (WORD*)((((uintptr_t)pIn + 3) >> 2) << 2); }

            static BOOL CALLBACK Proc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
            {
                switch (iMsg)
	            {
                    case WM_INITDIALOG:
                    {
                        HWND hWndEdit = GetDlgItem(hDlg, ID_EDIT);

                        const char* pText = (const char*)lParam;
                        SetWindowTextA(hWndEdit, pText);

                        HFONT hFont = CreateFontW(-11, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Courier New");
                        if(hFont)
                            SendMessage(hWndEdit, WM_SETFONT, WPARAM(hFont), TRUE);

                        SendMessage(hWndEdit, EM_SETSEL, (WPARAM)0, (LPARAM)0);

                        return TRUE;
                    }

                    case WM_COMMAND:
                        switch (LOWORD(wParam))
                        {
				            case ID_EDIT:
                            {
                                // Handle messages from the edit control here.
                                HWND hWndEdit = GetDlgItem(hDlg, ID_EDIT);
                                SendMessage(hWndEdit, EM_SETSEL, (WPARAM)0, (LPARAM)0);
					            return TRUE;
                            }

		                    case IDOK:
                                EndDialog(hDlg, 0);
			                    return TRUE;
		                }
                        break;
                }

                return FALSE;
            }
        };


        char dialogTemplateMemory[1024];
        memset(dialogTemplateMemory, 0, sizeof(dialogTemplateMemory));
        DLGTEMPLATE* pDlg = (LPDLGTEMPLATE)dialogTemplateMemory;

        const size_t textLineCount = Dialog::LineCount(pText);

        // Sizes are in Windows dialog units, which are relative to a character size. Depends on the font and environment settings. Often the pixel size will be ~3x the dialog unit x size. Often the pixel size will be ~3x the dialog unit y size.
        const int    kGutterSize   =  6; // Empty border space around controls within the dialog
        const int    kButtonWidth  = 24;
        const int    kButtonHeight = 10;
        const int    kDialogWidth  = 600; // To do: Clip this against screen bounds.
        const int    kDialogHeight = ((textLineCount > 100) ? 400 : ((textLineCount > 25) ? 300 : 200));

        // Define a dialog box.
        pDlg->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
        pDlg->cdit  = 2;    // Control count
        pDlg->x     = 10;   // X position To do: Center the dialog.
        pDlg->y     = 10;
        pDlg->cx    = (short)kDialogWidth;
        pDlg->cy    = (short)kDialogHeight;
        WORD* pWord = (WORD*)(pDlg + 1);
        *pWord++ = 0;   // No menu
        *pWord++ = 0;   // Default dialog box class

        WCHAR* pWchar = (WCHAR*)pWord;
        const size_t titleLength = strlen(pTitle);
        size_t wcharCount = OVR::UTF8Util::DecodeString(pWchar, pTitle, (titleLength > 128) ? 128 : titleLength);
        pWord += wcharCount + 1;

        // Define an OK button.
        pWord = Dialog::WordUp(pWord);

        DLGITEMTEMPLATE* pDlgItem = (DLGITEMTEMPLATE*)pWord;
        pDlgItem->x     = pDlg->cx - (kGutterSize + kButtonWidth);
        pDlgItem->y     = pDlg->cy - (kGutterSize + kButtonHeight);
        pDlgItem->cx    = kButtonWidth;
        pDlgItem->cy    = kButtonHeight;
        pDlgItem->id    = IDOK;
        pDlgItem->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;

        pWord   = (WORD*)(pDlgItem + 1);
        *pWord++ = 0xFFFF;
        *pWord++ = 0x0080; // button class

        pWchar     = (WCHAR*)pWord;
        pWchar[0] = 'O'; pWchar[1] = 'K'; pWchar[2] = '\0'; // Not currently localized.
        pWord     += 3; // OK\0
        *pWord++   = 0; // no creation data

        // Define an EDIT contol.
        pWord = Dialog::WordUp(pWord);

        pDlgItem = (DLGITEMTEMPLATE*)pWord;
        pDlgItem->x  = kGutterSize;
        pDlgItem->y  = kGutterSize;
        pDlgItem->cx = pDlg->cx - (kGutterSize + kGutterSize);
        pDlgItem->cy = pDlg->cy - (kGutterSize + kButtonHeight + kGutterSize + (kGutterSize / 2));
        pDlgItem->id = ID_EDIT;
        pDlgItem->style = ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | ES_READONLY | WS_VSCROLL | WS_BORDER | WS_TABSTOP | WS_CHILD | WS_VISIBLE;

        pWord = (WORD*)(pDlgItem + 1);
        *pWord++ = 0xFFFF;
        *pWord++ = 0x0081;  // edit class atom
        *pWord++ = 0;       // no creation data

        LRESULT ret = DialogBoxIndirectParam(NULL, (LPDLGTEMPLATE)pDlg, NULL, (DLGPROC)Dialog::Proc, (LPARAM)pText);

        return (ret != 0);
    }
#elif defined(OVR_OS_MAC)
    // For Apple we use the Objective C implementation in Util_GUI.mm
#else
    // To do.
    bool DisplayMessageBox(const char* pTitle, const char* pText)
    {
        printf("\n\nMessageBox\n%s\n", pTitle);
        printf("%s\n\n", pText);
        return false;
    }
#endif


} } // namespace OVR::Util




