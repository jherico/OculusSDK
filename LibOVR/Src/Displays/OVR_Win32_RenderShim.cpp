/************************************************************************************

Filename    :   OVR_Win32_DisplayShim.cpp
Content     :   Shared static functions for inclusion that allow for an application
		        to inject the usermode driver into an application
Created     :   March 21, 2014
Authors     :   Dean Beeler

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus Inc license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

************************************************************************************/

#include <windows.h>
#include <DbgHelp.h>
#include <AtlBase.h>
#include <AtlConv.h>

#include "OVR_Win32_Dxgi_Display.h"

#if AVOID_LIB_OVR
#define IN_COMPATIBILITY_MODE() (0)
#else
#include "OVR_Win32_Display.h"
#define IN_COMPATIBILITY_MODE() OVR::Display::InCompatibilityMode()
#endif

#pragma comment(lib, "DbgHelp.lib")

// Forward declarations
// These functions are implemented in OVR_Win32_DisplayDevice.cpp.

BOOL WINAPI OVRIsInitializingDisplay( PVOID context, UINT width, UINT height );
BOOL WINAPI OVRIsCreatingBackBuffer( PVOID context );
BOOL WINAPI OVRShouldVSync( );
ULONG WINAPI OVRRiftForContext( PVOID context, HANDLE driverHandle );
BOOL WINAPI OVRCloseRiftForContext( PVOID context, HANDLE driverHandle, ULONG rift );
BOOL WINAPI OVRWindowDisplayResolution( PVOID context, UINT* width, UINT* height,
									   UINT* titleHeight, UINT* borderWidth,
									   BOOL* vsyncEnabled );
BOOL WINAPI OVRExpectedResolution( PVOID context, UINT* width, UINT* height, UINT* rotationInDegrees );
BOOL WINAPI OVRShouldEnableDebug();
BOOL WINAPI OVRMirroringEnabled( PVOID context );
HWND WINAPI OVRGetWindowForContext(PVOID context);
BOOL WINAPI OVRShouldPresentOnContext(PVOID context);

static const char* GFX_DRIVER_KEY_FMT = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\%04d";
#ifdef _WIN64
static const char* RTFilter = "OVRDisplayRT64.dll";
static const char* UMFilter = "OVRDisplay64.dll";
#else
static const char* RTFilter = "OVRDisplayRT32.dll";
static const char* UMFilter = "OVRDisplay32.dll";
#endif
static const char* OptimusDrivers = "nvumdshimx.dll nvumdshim.dll";

typedef enum OVRTargetAPI
{
	DirectX,
	OpenGL
};

static PVOID lastContext = NULL;
LINK_APPLICATION_DRIVER appDriver = {0};
static INT apiVersion = 10;

static CHAR* ReadRegStr(HKEY keySub, const char* keyName, const char* valName)
{
	CHAR *val = NULL;
	REGSAM access = KEY_READ;
	HKEY hKey;

TryAgainWOW64:
	NTSTATUS res = RegOpenKeyExA( keySub, keyName, 0, access, &hKey );
	if ( res == ERROR_SUCCESS ) 
	{ 
		DWORD valLen;
		res = RegQueryValueExA( hKey, valName, NULL, NULL, NULL, &valLen );
		if( res == ERROR_SUCCESS ) {
			val = (CHAR*)calloc( valLen + 1, sizeof(CHAR) );
			res = RegQueryValueExA( hKey, valName, NULL, NULL, (LPBYTE)val, &valLen );

			if( res == ERROR_SUCCESS )
			{
				CHAR* byte = val;
				for( DWORD j = 0; j < valLen; ++j )
				{
					if( byte[j] == 0 )
						byte[j] = ' ';
				}
			}
			else
			{
				free( val );
				val = NULL;
			}
		}
		RegCloseKey( hKey ); 
	}

	if( res == ERROR_FILE_NOT_FOUND && keySub == HKEY_LOCAL_MACHINE && access == KEY_READ  ) {
#ifdef _WIN64
		access = KEY_READ | KEY_WOW64_32KEY;
#else
		access = KEY_READ | KEY_WOW64_64KEY;
#endif
		goto TryAgainWOW64;
	}
	return val;
}

#define OLD_DATA_BACKUP_SIZE 16

WinLoadLibraryA			oldProcA = NULL;
WinLoadLibraryExA		oldProcExA = NULL;
WinLoadLibraryW			oldProcW = NULL;
WinLoadLibraryExW		oldProcExW = NULL;
WinGetModuleHandleExA	oldProcModExA = NULL;
WinGetModuleHandleExW	oldProcModExW = NULL;
WinDirect3DCreate9		oldDirectX9Create = NULL;
BYTE					oldDirectX9CreateData[OLD_DATA_BACKUP_SIZE];
WinDirect3DCreate9Ex	oldDirectX9ExCreate = NULL;
BYTE					oldDirectX9ExCreateData[OLD_DATA_BACKUP_SIZE];
WinCreateDXGIFactory	oldCreateDXGIFactory = NULL;
BYTE					oldCreateDXGIFactoryData[OLD_DATA_BACKUP_SIZE];
WinCreateDXGIFactory1	oldCreateDXGIFactory1 = NULL;
BYTE					oldCreateDXGIFactory1Data[OLD_DATA_BACKUP_SIZE];
WinCreateDXGIFactory2   oldCreateDXGIFactory2 = NULL;
BYTE					oldCreateDXGIFactory2Data[OLD_DATA_BACKUP_SIZE];

static bool checkForOverride( LPCSTR libFileName, OVRTargetAPI& targetApi )
{
	for (int i=0; ; i++)
	{
		CHAR keyString[256] = {0};

		sprintf_s( keyString, 256, GFX_DRIVER_KEY_FMT, i );

		CHAR* providerName = ReadRegStr( HKEY_LOCAL_MACHINE, keyString, "ProviderName" );

		// No provider name means we're out of display enumerations
		if( providerName == NULL )
			break;

		free( providerName );

		// Check 64-bit driver names followed by 32-bit driver names
		const char* driverKeys[] = {"UserModeDriverName", "UserModeDriverNameWoW", "OpenGLDriverName", "OpenGLDriverNameWoW", "InstalledDisplayDrivers" };
		for( int j = 0; j < 6; ++j )
		{
			CHAR userModeList[4096] = {0};
			
			switch(j)
			{
				case 5:
					strcpy_s( userModeList, 4095, OptimusDrivers );
					break;
				default:
					{
						CHAR* regString = ReadRegStr( HKEY_LOCAL_MACHINE, keyString, driverKeys[j] );
						if( regString )
						{
							strcpy_s( userModeList, 4095, regString );
							free( regString );
						}

					}
					break;
			}

			char *nextToken = NULL;

			if( userModeList )
			{
				char* first = strtok_s( userModeList, " ", &nextToken );
				while( first )
				{
					if( strstr( libFileName, first ) != 0 )
					{
						if( j < 2 )
							targetApi = DirectX;
						else
							targetApi = OpenGL;

						return true;
					}
					first = strtok_s( NULL, " ", &nextToken );
				}
			}
		}
	}

	return false;
}

static HMODULE createShim( LPCSTR lpLibFileName, OVRTargetAPI targetAPI )
{
	//Sleep(10000);
	if( IN_COMPATIBILITY_MODE() )
	{
		return (*oldProcA)( lpLibFileName );
	}

	UNREFERENCED_PARAMETER( targetAPI );

	HMODULE result = NULL;

	result = (*oldProcA)( UMFilter );

	if( result )
	{
		PreloadLibraryFn loadFunc = (PreloadLibraryFn)GetProcAddress( result, "PreloadLibrary" );
		if( loadFunc )
		{
			HRESULT localRes = (*loadFunc)( oldProcA, lpLibFileName, &appDriver );
			if( localRes != S_OK )
				result = NULL;
		}
	}

	if( !result )
	{
		OutputDebugString( L"createShim:  unable to load usermode filter\n" );
		result = (*oldProcA)( lpLibFileName );
	}
	return result;
}

static HMODULE
	WINAPI
	OVRLoadLibraryA(
	__in LPCSTR lpLibFileName
	)
{
	OVRTargetAPI targetAPI = DirectX;
	bool needShim = checkForOverride( lpLibFileName, targetAPI );
	if( !needShim )
		return (*oldProcA)( lpLibFileName );

	return createShim( lpLibFileName, targetAPI );
}

static HMODULE
	WINAPI
	OVRLoadLibraryW(
	__in LPCWSTR lpLibFileName
	)
{
	USES_CONVERSION;

	OVRTargetAPI targetAPI = DirectX;

	bool needShim = checkForOverride( W2A( lpLibFileName ), targetAPI );
	if( !needShim )	
		return (*oldProcW)( lpLibFileName );

	return createShim( W2A( lpLibFileName ), targetAPI );
}

static HMODULE
	WINAPI
	OVRLoadLibraryExA(
	__in       LPCSTR lpLibFileName,
	__reserved HANDLE hFile,
	__in       DWORD dwFlags

	)
{
	OVRTargetAPI targetAPI = DirectX;

	bool needShim = checkForOverride( lpLibFileName, targetAPI );
	if( !needShim )
		return (*oldProcExA)( lpLibFileName, hFile, dwFlags );

	// FIXME: Don't throw away the flags parameter
	return createShim( lpLibFileName, targetAPI );
}

static HMODULE
	WINAPI
	OVRLoadLibraryExW(
	__in       LPCWSTR lpLibFileName,
	__reserved HANDLE hFile,
	__in       DWORD dwFlags
	)
{
	USES_CONVERSION;

	OVRTargetAPI targetAPI = DirectX;

	bool needShim = checkForOverride( W2A( lpLibFileName ), targetAPI );
	if( !needShim )
		return (*oldProcExW)( lpLibFileName, hFile, dwFlags );

	// FIXME: Don't throw away the flags parameter
	return createShim( W2A( lpLibFileName ), targetAPI );
}

static BOOL WINAPI OVRGetModuleHandleExA(
	__in      DWORD dwFlags,
	__in_opt  LPCSTR lpModuleName,
	__out    HMODULE *phModule
	)
{
	OVRTargetAPI targetAPI = DirectX;

	bool needShim = checkForOverride( lpModuleName, targetAPI );
	if( !needShim )
	{
		return (*oldProcModExA)( dwFlags, lpModuleName, phModule );
	}
	
	*phModule = createShim( lpModuleName, targetAPI );

	return TRUE;
}

static BOOL WINAPI OVRGetModuleHandleExW(
	__in      DWORD dwFlags,
	__in_opt  LPCWSTR lpModuleName,
	__out    HMODULE *phModule
	)
{
	USES_CONVERSION;

	OVRTargetAPI targetAPI = DirectX;

	bool needShim = checkForOverride( W2A( lpModuleName ), targetAPI );
	if( !needShim )
	{
		return (*oldProcModExW)( dwFlags, lpModuleName, phModule );
	}

	*phModule = createShim( W2A( lpModuleName ), targetAPI );

	return TRUE;
}

#ifdef _AMD64_
static void restoreFunction( PROC pfnHookAPIAddr, PBYTE oldData )
{
	static const LONGLONG addressSize = sizeof(PROC);
	static const LONGLONG jmpSize = addressSize + 6;

	DWORD oldProtect;
	VirtualProtect((LPVOID)pfnHookAPIAddr, OLD_DATA_BACKUP_SIZE,                       
		PAGE_EXECUTE_READWRITE, &oldProtect);

	memcpy(pfnHookAPIAddr, oldData, OLD_DATA_BACKUP_SIZE);  

	VirtualProtect((LPVOID)pfnHookAPIAddr, OLD_DATA_BACKUP_SIZE, oldProtect, NULL);  
}

static void setFunction( PROC pfnHookAPIAddr, PROC replacementFunction, PBYTE oldData )
{
	static const LONGLONG addressSize = sizeof(PROC);
	static const LONGLONG jmpSize = addressSize + 6;

	INT_PTR jumpOffset = (INT_PTR)replacementFunction;

	DWORD oldProtect;
	VirtualProtect((LPVOID)pfnHookAPIAddr, OLD_DATA_BACKUP_SIZE,                       
		PAGE_EXECUTE_READWRITE, &oldProtect);

	memcpy(oldData, pfnHookAPIAddr, OLD_DATA_BACKUP_SIZE);

	PBYTE functionData = (PBYTE)pfnHookAPIAddr;
	functionData[0] = 0xff; // JMP [RIP+0]
	functionData[1] = 0x25; //
	functionData[2] = 0x00; //
	functionData[3] = 0x00; //
	functionData[4] = 0x00; //
	functionData[5] = 0x00; //
	memcpy( functionData + 6, &jumpOffset, sizeof( INT_PTR ) );

	VirtualProtect((LPVOID)pfnHookAPIAddr, OLD_DATA_BACKUP_SIZE, oldProtect, NULL);  
}
#else
static void restoreFunction( PROC pfnHookAPIAddr, PBYTE oldData )
{
	static const LONGLONG addressSize = sizeof(PROC);
	static const LONGLONG jmpSize = addressSize + 1;

	DWORD oldProtect;
	VirtualProtect((LPVOID)pfnHookAPIAddr, jmpSize,                       
		PAGE_EXECUTE_READWRITE, &oldProtect);

	memcpy(pfnHookAPIAddr, oldData, jmpSize);  

	VirtualProtect((LPVOID)pfnHookAPIAddr, jmpSize, oldProtect, NULL);  
}

static void setFunction( PROC pfnHookAPIAddr, PROC replacementFunction, PBYTE oldData )
{
	static const LONGLONG addressSize = sizeof(PROC);
	static const LONGLONG jmpSize = addressSize + 1;

	INT_PTR jumpOffset = (INT_PTR)replacementFunction - (INT_PTR)pfnHookAPIAddr - jmpSize;

	DWORD oldProtect;
	VirtualProtect((LPVOID)pfnHookAPIAddr, jmpSize,                       
		PAGE_EXECUTE_READWRITE, &oldProtect);

	memcpy(oldData, pfnHookAPIAddr, jmpSize);

	PBYTE functionData = (PBYTE)pfnHookAPIAddr;
	memcpy( oldData, functionData, jmpSize );
	functionData[0] = 0xe9;
	memcpy( functionData + 1, &jumpOffset, sizeof( INT_PTR ) );

	VirtualProtect((LPVOID)pfnHookAPIAddr, jmpSize, oldProtect, NULL);  
}
#endif

static BOOL WINAPI OVRLocalIsInitializingDisplay( PVOID context, UINT width, UINT height )
{
	UINT expectedWidth, expectedHeight, rotation;

	OVRExpectedResolution( context, &expectedWidth, &expectedHeight, &rotation );

	if( appDriver.pfnActiveAPIVersion )
		apiVersion = (*appDriver.pfnActiveAPIVersion)( context );

	switch( apiVersion )
	{
		case 1:  // OpenGL
		case 10: // DirectX 1X
			if( width == expectedWidth && height == expectedHeight )
				return TRUE;
			break;
		case 9: // DirectX 9
			if( rotation == 90 || rotation == 270 )
			{
				if( width == expectedHeight && height == expectedWidth )
					return TRUE;
			}
			else
			{
				if( width == expectedWidth && height == expectedHeight )
					return TRUE;
			}
			break;
		default:
			break;
	}

	return FALSE;
}


HRESULT APIENTRY OVRDirect3DCreate9Ex(UINT SDKVersion, void** aDevice)
{
	apiVersion = 9;

	HRESULT result = S_OK;

	restoreFunction( (PROC)oldDirectX9ExCreate, oldDirectX9ExCreateData );

    if (IN_COMPATIBILITY_MODE())
    {
        result = (*oldDirectX9ExCreate)(SDKVersion, aDevice);
    }
    else
    {
        HMODULE rtFilterModule = (*oldProcA)(RTFilter);
        WinDirect3DCreate9Ex createFunction = (WinDirect3DCreate9Ex)GetProcAddress(rtFilterModule, "Direct3DCreate9Ex");
        result = (*createFunction)(SDKVersion, aDevice);
    }

	setFunction( (PROC)oldDirectX9ExCreate, (PROC)OVRDirect3DCreate9Ex, oldDirectX9ExCreateData );

	printf("%s result 0x%x\n", __FUNCTION__, result);

	return result;
}

void* APIENTRY OVRDirect3DCreate9(UINT SDKVersion)
{
	void* result = NULL;

	OVRDirect3DCreate9Ex( SDKVersion, &result );

	return result;
}

HRESULT APIENTRY OVRCreateDXGIFactory(
	__in   REFIID riid,
	__out  void **ppFactory
	)
{
	HRESULT result = E_FAIL;

	restoreFunction( (PROC)oldCreateDXGIFactory, oldCreateDXGIFactoryData );

    if (IN_COMPATIBILITY_MODE())
    {
        result = (*oldCreateDXGIFactory)(riid, ppFactory);
    }
    else
    {
        HMODULE rtFilterModule = (*oldProcA)(RTFilter);
        WinCreateDXGIFactory createFunction = (WinCreateDXGIFactory)GetProcAddress(rtFilterModule, "CreateDXGIFactory");
        result = (*createFunction)(riid, ppFactory);
    }

	setFunction( (PROC)oldCreateDXGIFactory, (PROC)OVRCreateDXGIFactory, oldCreateDXGIFactoryData );

	printf("%s result 0x%x\n", __FUNCTION__, result);

	return result;
}

HRESULT APIENTRY OVRCreateDXGIFactory1(
	__in   REFIID riid,
	__out  void **ppFactory
	)
{
	HRESULT result = E_FAIL;

	restoreFunction( (PROC)oldCreateDXGIFactory1, oldCreateDXGIFactory1Data );

    if (IN_COMPATIBILITY_MODE())
    {
        result = (*oldCreateDXGIFactory1)(riid, ppFactory);
    }
    else
    {
        HMODULE rtFilterModule = (*oldProcA)(RTFilter);
        WinCreateDXGIFactory1 createFunction = (WinCreateDXGIFactory1)GetProcAddress(rtFilterModule, "CreateDXGIFactory1");
        result = (*createFunction)(riid, ppFactory);
    }

	setFunction( (PROC)oldCreateDXGIFactory1, (PROC)OVRCreateDXGIFactory1, oldCreateDXGIFactory1Data );

	printf("%s result 0x%x\n", __FUNCTION__, result);

	return result;
}

HRESULT APIENTRY OVRCreateDXGIFactory2(
	__in   UINT flags,
	__in   const IID &riid,
	__out  void **ppFactory
	)
{
	HRESULT result = E_FAIL;

	restoreFunction( (PROC)oldCreateDXGIFactory2, oldCreateDXGIFactory2Data );

    if (IN_COMPATIBILITY_MODE())
    {
        result = (*oldCreateDXGIFactory2)(flags, riid, ppFactory);
    }
    else
    {
        HMODULE rtFilterModule = (*oldProcA)(RTFilter);
        WinCreateDXGIFactory2 createFunction = (WinCreateDXGIFactory2)GetProcAddress(rtFilterModule, "CreateDXGIFactory2");
        result = (*createFunction)(flags, riid, ppFactory);
    }

	setFunction( (PROC)oldCreateDXGIFactory2, (PROC)OVRCreateDXGIFactory2, oldCreateDXGIFactory2Data );

	printf("%s result 0x%x\n", __FUNCTION__, result);

	return result;
}

static PROC SetProcAddressDirect(
	__in HINSTANCE hInstance,
	__in LPCSTR lpProcName,
	__in PROC  newFunction,
	__inout  BYTE* oldData
	)
{
	static const LONGLONG addressSize = sizeof(PROC);
	static const LONGLONG jmpSize = addressSize + 1;

	PROC result = NULL;

	PROC pfnHookAPIAddr = GetProcAddress( hInstance, lpProcName );

	if( pfnHookAPIAddr )
	{
		result = pfnHookAPIAddr;

		setFunction( pfnHookAPIAddr, newFunction, oldData );
	}

	return result;
}

static PROC SetProcAddressA(
	__in  HINSTANCE targetModule,
	__in  LPCSTR lpLibFileName,
	__in  LPCSTR lpProcName,
	__in  PROC  newFunction
	)
{
	PROC pfnHookAPIAddr = GetProcAddress( LoadLibraryA( lpLibFileName ), lpProcName );

	HINSTANCE hInstance = targetModule; 

	ULONG ulSize;
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = 
		(PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(
		hInstance,
		TRUE,
		IMAGE_DIRECTORY_ENTRY_IMPORT,
		&ulSize
		);

	while (pImportDesc->Name)
	{
		PSTR pszModName = (PSTR)((PBYTE) hInstance + pImportDesc->Name);
		if (_stricmp(pszModName, lpLibFileName) == 0) 
			break;   
		pImportDesc++;
	}

	PIMAGE_THUNK_DATA pThunk = 
		(PIMAGE_THUNK_DATA)((PBYTE) hInstance + pImportDesc->FirstThunk);

	while (pThunk->u1.Function)
	{
		PROC* ppfn = (PROC*) &pThunk->u1.Function;
		BOOL bFound = (*ppfn == pfnHookAPIAddr);

		if (bFound) 
		{
			MEMORY_BASIC_INFORMATION mbi;
			VirtualQuery(
				ppfn,
				&mbi,
				sizeof(MEMORY_BASIC_INFORMATION)
				);
			VirtualProtect(
				mbi.BaseAddress,
				mbi.RegionSize,
				PAGE_READWRITE,
				&mbi.Protect);

			*ppfn = *newFunction;

			DWORD dwOldProtect;
			VirtualProtect(
				mbi.BaseAddress,
				mbi.RegionSize,
				mbi.Protect,
				&dwOldProtect
				);
			break;
		}
		pThunk++;
	}

	return pfnHookAPIAddr;
}

enum ShimedLibraries
{
	ShimLibDXGI =		 0,
	ShimLibD3D9 =		 1,
	ShimLibD3D11 =		 2,
	ShimLibDXGIDebug =	 3,
	ShimLibD3D10Core =	 4,
	ShimLibD3D10 =		 5,
	ShimLibGL =			 6,
	ShimCountMax =		 7
};

void checkUMDriverOverrides( void* context )
{
	lastContext = context;
	if( oldProcA == NULL )
	{
		char* dllList[ShimCountMax] = { "dxgi.dll", "d3d9.dll", "d3d11.dll", "dxgidebug.dll", "d3d10core.dll", "d3d10.dll", "opengl32.dll" };

		PreloadLibraryRTFn loadFunc = NULL;

		for( int i = 0; i < ShimCountMax; ++i )
		{
			HINSTANCE hInst = GetModuleHandleA( dllList[i] );
			if( hInst == NULL )
			{
				try
				{
					hInst = LoadLibraryA(dllList[i]);
				}
				catch(...)
				{

				}
				
			}

			if( hInst )
			{
				ShimedLibraries libCount = (ShimedLibraries)i;
				switch( libCount )
				{
					case ShimLibDXGI:
						oldCreateDXGIFactory = (WinCreateDXGIFactory)SetProcAddressDirect( hInst, "CreateDXGIFactory", (PROC)OVRCreateDXGIFactory, oldCreateDXGIFactoryData );
						oldCreateDXGIFactory1 = (WinCreateDXGIFactory1)SetProcAddressDirect( hInst, "CreateDXGIFactory1", (PROC)OVRCreateDXGIFactory1, oldCreateDXGIFactory1Data );
						oldCreateDXGIFactory2 = (WinCreateDXGIFactory2)SetProcAddressDirect( hInst, "CreateDXGIFactory2", (PROC)OVRCreateDXGIFactory2, oldCreateDXGIFactory2Data );
						break;
					case ShimLibD3D9:
						oldDirectX9Create = (WinDirect3DCreate9)SetProcAddressDirect( hInst, "Direct3DCreate9", (PROC)OVRDirect3DCreate9, oldDirectX9CreateData );
						oldDirectX9ExCreate = (WinDirect3DCreate9Ex)SetProcAddressDirect( hInst, "Direct3DCreate9Ex", (PROC)OVRDirect3DCreate9Ex, oldDirectX9ExCreateData );
						break;
					default:
						break;
				}

				char* loaderLibraryList[4] = {"kernel32.dll", "api-ms-win-core-libraryloader-l1-2-0.dll", "api-ms-win-core-libraryloader-l1-1-0.dll", "api-ms-win-core-libraryloader-l1-1-1.dll"};

				for( int j = 0; j < 4; ++j )
				{
					char* loaderLibrary = loaderLibraryList[j];

					PROC temp = NULL;
					temp = SetProcAddressA( hInst, loaderLibrary, "LoadLibraryA", (PROC)OVRLoadLibraryA  );
					if( !oldProcA )
						oldProcA = (WinLoadLibraryA)temp;

					temp = SetProcAddressA( hInst, loaderLibrary, "LoadLibraryW", (PROC)OVRLoadLibraryW  );
					if( !oldProcW )
						oldProcW = (WinLoadLibraryW)temp;

					temp = SetProcAddressA( hInst, loaderLibrary, "LoadLibraryExA", (PROC)OVRLoadLibraryExA  );
					if( !oldProcExA )
						oldProcExA = (WinLoadLibraryExA)temp;

					temp = SetProcAddressA( hInst, loaderLibrary, "LoadLibraryExW", (PROC)OVRLoadLibraryExW  );
					if( !oldProcExW )
						oldProcExW = (WinLoadLibraryExW)temp;

					temp = SetProcAddressA( hInst, loaderLibrary, "GetModuleHandleExA", (PROC)OVRGetModuleHandleExA );
					if( !oldProcModExA )
						oldProcModExA = (WinGetModuleHandleExA)temp;

					temp = SetProcAddressA( hInst, loaderLibrary, "GetModuleHandleExW", (PROC)OVRGetModuleHandleExW );
					if( !oldProcModExW )
						oldProcModExW = (WinGetModuleHandleExW)temp;
				}

				if( loadFunc == NULL )
					loadFunc = (PreloadLibraryRTFn)GetProcAddress( hInst, "PreloadLibraryRT" );
			}
		}

		HMODULE rtFilterModule = (*oldProcA)( RTFilter );

		if( loadFunc == NULL )
			loadFunc = (PreloadLibraryRTFn)GetProcAddress( rtFilterModule, "PreloadLibraryRT" );
		IsCreatingBackBuffer backBufferFunc = (IsCreatingBackBuffer)GetProcAddress( rtFilterModule, "OVRIsCreatingBackBuffer" );
		ShouldVSync	shouldVSyncFunc = (ShouldVSync)GetProcAddress( rtFilterModule, "OVRShouldVSync" );

		if( loadFunc )
		{
			appDriver.version = 1;
			appDriver.context = lastContext;

//			appDriver.pfnInitializingDisplay        = OVRIsInitializingDisplay;
			appDriver.pfnInitializingDisplay        = OVRLocalIsInitializingDisplay;
			appDriver.pfnRiftForContext             = OVRRiftForContext;
			appDriver.pfnCloseRiftForContext        = OVRCloseRiftForContext;
			appDriver.pfnWindowDisplayResolution    = OVRWindowDisplayResolution;
			appDriver.pfnShouldEnableDebug          = OVRShouldEnableDebug;
			appDriver.pfnIsCreatingBackBuffer       = (backBufferFunc == NULL) ? OVRIsCreatingBackBuffer : backBufferFunc;
			appDriver.pfnShouldVSync                = (shouldVSyncFunc == NULL) ? OVRShouldVSync : shouldVSyncFunc;
			appDriver.pfnExpectedResolution			= OVRExpectedResolution;
			appDriver.pfnMirroringEnabled			= OVRMirroringEnabled;
			appDriver.pfnGetWindowForContext		= OVRGetWindowForContext;
			appDriver.pfnPresentRiftOnContext		= OVRShouldPresentOnContext;

			appDriver.pfnDirect3DCreate9    = oldDirectX9Create;
			appDriver.pfnDirect3DCreate9Ex  = oldDirectX9ExCreate;
			appDriver.pfnCreateDXGIFactory  = oldCreateDXGIFactory;
			appDriver.pfnCreateDXGIFactory1 = oldCreateDXGIFactory1;
			appDriver.pfnCreateDXGIFactory2 = oldCreateDXGIFactory2;

			(*loadFunc)( &appDriver );
		}
	}
}
