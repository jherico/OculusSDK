/************************************************************************************

Filename    :   Util_ImageWindow.h
Content     :   An output object for windows that can display raw images for testing
Created     :   March 13, 2014
Authors     :   Dean Beeler

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

*************************************************************************************/

#ifndef UTIL_IMAGEWINDOW_H
#define UTIL_IMAGEWINDOW_H

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <d2d1.h>

#include "../../Include/OVR.h"
#include "../Kernel/OVR_Hash.h"
#include "../Kernel/OVR_Array.h"
#include "../Kernel/OVR_Threads.h"
#include "../Kernel/OVR_Deque.h"

#include <stdint.h>

namespace OVR { namespace Util {

class ImageWindow
{
	typedef struct 
	{
		float x;
		float y;
		float radius;
		float r;
		float g;
		float b;
		bool  fill;
	} CirclePlot;

	typedef struct  
	{
		float x;
		float y;
		float r;
		float g;
		float b;
		WCHAR* text;
	} TextPlot;

	typedef struct
	{
		Array<CirclePlot> plots;
		void*			  imageData;
		void*			  colorImageData;
		int				  width;
		int				  height;
		int				  colorPitch;
		bool			  ready;
	} Frame;

	static ID2D1Factory* pD2DFactory;

	HWND hWindow;
	ID2D1RenderTarget* pRT;
	D2D1_SIZE_U resolution;

	Mutex*						frontBufferMutex;

	InPlaceMutableDeque<Frame>	frames;

	ID2D1Bitmap*				greyBitmap;
	ID2D1Bitmap*				colorBitmap;

public:
	// constructors
	ImageWindow();
	ImageWindow( UINT width, UINT height );
	virtual ~ImageWindow();

	void OnPaint(); // Called by Windows when it receives a WM_PAINT message

	void UpdateImage( const UINT8* imageData, UINT width, UINT height ) { UpdateImageBW( imageData, width, height ); }
	void UpdateImageBW( const UINT8* imageData, UINT width, UINT height );
	void UpdateImageRGBA( const UINT8* imageData, UINT width, UINT height, UINT pitch );
	void Complete(); // Called by drawing thread to submit a frame

	void Process(); // Called by rendering thread to do window processing

	void AssociateSurface( void* surface );

	void addCircle( float x , float y, float radius, float r, float g, float b, bool fill );

	static ImageWindow*			GlobalWindow() { return globalWindow; }

private:


	static ImageWindow*			globalWindow;

	static bool running;
};

}} // namespace OVR::Util

#endif