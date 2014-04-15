/************************************************************************************

Filename    :   Util_ImageWindow.cpp
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
#include "../../Include/OVR.h"

#include "Util_ImageWindow.h"

#include <Windows.h>

typedef HRESULT (WINAPI *D2D1CreateFactoryFn)(
	_In_      D2D1_FACTORY_TYPE,
	_In_      REFIID,
	_In_opt_  const D2D1_FACTORY_OPTIONS*,
	_Out_     ID2D1Factory **
	);


namespace OVR { namespace Util {
	
ID2D1Factory* ImageWindow::pD2DFactory = NULL;
ImageWindow* ImageWindow::globalWindow = NULL;

LRESULT CALLBACK MainWndProc(
	HWND hwnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (uMsg) 
	{ 
	case WM_CREATE: 
		return 0; 

	case WM_PAINT: 
		{
			LONG_PTR ptr = GetWindowLongPtr( hwnd, GWLP_USERDATA );
			if( ptr )
			{
				ImageWindow* iw = (ImageWindow*)ptr;
				iw->OnPaint();
			}
		}
		
		return 0; 

	case WM_SIZE: 
		// Set the size and position of the window. 
		return 0; 

	case WM_DESTROY: 
		// Clean up window-specific data objects. 
		return 0; 

		// 
		// Process other messages. 
		// 

	default: 
		return DefWindowProc(hwnd, uMsg, wParam, lParam); 
	} 
	//return 0; 
}

ImageWindow::ImageWindow() :
	frontBufferMutex( new Mutex() )
{

	HINSTANCE hInst = LoadLibrary( L"d2d1.dll" );

	D2D1CreateFactoryFn createFactory = NULL;

	if( hInst )
	{
		createFactory = (D2D1CreateFactoryFn)GetProcAddress( hInst, "D2D1CreateFactory" );
	}

	globalWindow = this;

	int width = 752;
	int height = 480;

	if( pD2DFactory == NULL && createFactory )
	{
		createFactory( 
			D2D1_FACTORY_TYPE_MULTI_THREADED,
			__uuidof(ID2D1Factory),
			NULL,
			&pD2DFactory
			);
	}

	resolution = D2D1::SizeU( width, height );

	SetWindowLongPtr( hWindow, GWLP_USERDATA, (LONG_PTR)this );

	pRT = NULL;
	greyBitmap = NULL;
	colorBitmap = NULL;
}

ImageWindow::ImageWindow( UINT width, UINT height ) :
	frontBufferMutex( new Mutex() )
{


	HINSTANCE hInstance = GetModuleHandle( NULL );

	WNDCLASS wc;
	wc.lpszClassName = L"ImageWindowClass";
	wc.lpfnWndProc = MainWndProc;
	wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)( COLOR_WINDOW+1 );
	wc.lpszMenuName = L"";
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;

	RegisterClass(&wc);

	hWindow = CreateWindow(
		L"ImageWindowClass", 
		L"ImageWindow", 
		WS_OVERLAPPEDWINDOW & ~WS_SYSMENU, 
		CW_USEDEFAULT, 
		CW_USEDEFAULT, 
		width, 
		height, 
		NULL, 
		NULL, 
		hInstance, 
		NULL);

	resolution = D2D1::SizeU( width, height );

	SetWindowLongPtr( hWindow, GWLP_USERDATA, (LONG_PTR)this );

	ShowWindow( hWindow, SW_SHOW );

	RECT rc = {0};
	GetClientRect( hWindow, &rc );

	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
	D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
		hWindow,
		resolution
		);

	ID2D1HwndRenderTarget* hwndTarget = NULL;
	// Create a Direct2D render target			
	pRT = NULL;			
	pD2DFactory->CreateHwndRenderTarget(
		&props,
		&hwndProps,
		&hwndTarget
		);

	pRT = hwndTarget;

	D2D1_SIZE_U size = D2D1::SizeU( width, height );

	D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
		DXGI_FORMAT_A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED
		);

	D2D1_BITMAP_PROPERTIES bitmapProps;
	bitmapProps.dpiX = 72;
	bitmapProps.dpiY = 72;
	bitmapProps.pixelFormat = pixelFormat;

	HRESULT result = pRT->CreateBitmap( size, bitmapProps, &greyBitmap );
	result = pRT->CreateBitmap( size, bitmapProps, &colorBitmap );
}

ImageWindow::~ImageWindow()
{
	if( greyBitmap )
		greyBitmap->Release();

	if( colorBitmap )
		colorBitmap->Release();

	if( pRT )
		pRT->Release();

	delete frontBufferMutex;

	ShowWindow( hWindow, SW_HIDE );
	DestroyWindow( hWindow );
}

void ImageWindow::AssociateSurface( void* surface )
{
	// Assume an IUnknown
	IUnknown* unknown = (IUnknown*)surface;

	IDXGISurface *pDxgiSurface = NULL;
	HRESULT hr = unknown->QueryInterface(&pDxgiSurface);
	if( hr == S_OK )
	{
		D2D1_RENDER_TARGET_PROPERTIES props =
			D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
			96,
			96
			);


		pRT = NULL;			
		ID2D1RenderTarget* tmpTarget;

		hr = pD2DFactory->CreateDxgiSurfaceRenderTarget( pDxgiSurface, &props, &tmpTarget );

		if( hr == S_OK )
		{
			DXGI_SURFACE_DESC desc = {0};
			pDxgiSurface->GetDesc( &desc );
			int width = desc.Width;
			int height = desc.Height;

			D2D1_SIZE_U size = D2D1::SizeU( width, height );

			D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
				DXGI_FORMAT_A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED
				);

			D2D1_PIXEL_FORMAT colorPixelFormat = D2D1::PixelFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED
				);

			D2D1_BITMAP_PROPERTIES bitmapProps;
			bitmapProps.dpiX = 96;
			bitmapProps.dpiY = 96;
			bitmapProps.pixelFormat = pixelFormat;

			D2D1_BITMAP_PROPERTIES colorBitmapProps;
			colorBitmapProps.dpiX = 96;
			colorBitmapProps.dpiY = 96;
			colorBitmapProps.pixelFormat = colorPixelFormat;

			HRESULT result = tmpTarget->CreateBitmap( size, bitmapProps, &greyBitmap );
			if( result != S_OK )
			{
				tmpTarget->Release();
				tmpTarget = NULL;
			}

			result = tmpTarget->CreateBitmap( size, colorBitmapProps, &colorBitmap );
			if( result != S_OK )
			{
				greyBitmap->Release();
				greyBitmap = NULL;

				tmpTarget->Release();
				tmpTarget = NULL;
			}
			pRT = tmpTarget;
		}
	}
}

void ImageWindow::Process()
{
	if( pRT && greyBitmap )
	{
		OnPaint();
	}
}

void ImageWindow::Complete()
{
	Mutex::Locker locker( frontBufferMutex  );

	if( frames.IsEmpty() )
		return;

	if( frames.PeekBack(0).ready )
		return;

	Frame& frame = frames.PeekBack(0);

	frame.ready = true;
}

void ImageWindow::OnPaint()
{
	static float mover = -752.0f;

	Mutex::Locker locker( frontBufferMutex  );

	// Nothing to do
	if( frames.IsEmpty() )
		return;

	if( !frames.PeekFront(0).ready )
		return;

	Frame currentFrame = frames.PopFront();
	Frame dummyFrame = {0};

	Frame& nextFrame = dummyFrame;

	if( !frames.IsEmpty() )
		nextFrame = frames.PeekFront(0);
	
	while( nextFrame.ready )
	{
		// Free up the current frame since it's been removed from the deque
		free( currentFrame.imageData );
		if( currentFrame.colorImageData )
			free( currentFrame.colorImageData );

		currentFrame = frames.PopFront();

		if( frames.IsEmpty() )
			return;

		nextFrame = frames.PeekFront(0);
	}

	if( currentFrame.imageData )
		greyBitmap->CopyFromMemory( NULL, currentFrame.imageData, currentFrame.width );

	if( currentFrame.colorImageData )
		colorBitmap->CopyFromMemory( NULL, currentFrame.colorImageData, currentFrame.colorPitch );

	pRT->BeginDraw();

	pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED); 

	pRT->Clear( D2D1::ColorF(D2D1::ColorF::Black) );

	// This will mirror our image
	D2D1_MATRIX_3X2_F m;
	m._11 = -1; m._12 = 0;
	m._21 = 0; m._22 = 1;
	m._31 = 0; m._32 = 0;
	pRT->SetTransform( m );

	ID2D1SolidColorBrush* whiteBrush;

	pRT->CreateSolidColorBrush( D2D1::ColorF(D2D1::ColorF::White, 1.0f), &whiteBrush );

	if( currentFrame.imageData )
	{
		pRT->FillOpacityMask( greyBitmap, whiteBrush, 
			D2D1_OPACITY_MASK_CONTENT_TEXT_NATURAL, 
			D2D1::RectF( -(FLOAT)resolution.width, 0.0f, (FLOAT)0.0f, (FLOAT)resolution.height ), 
			D2D1::RectF( 0.0f, 0.0f, (FLOAT)resolution.width, (FLOAT)resolution.height ) );
	}
	else if( currentFrame.colorImageData )
	{
		pRT->DrawBitmap( colorBitmap,
			D2D1::RectF( -(FLOAT)resolution.width, 0.0f, (FLOAT)0.0f, (FLOAT)resolution.height ) );

	}

	pRT->SetTransform(D2D1::Matrix3x2F::Identity());

	whiteBrush->Release();

	Array<CirclePlot>::Iterator it;

	for( it = currentFrame.plots.Begin(); it != currentFrame.plots.End(); ++it )
	{
		ID2D1SolidColorBrush* aBrush;

		pRT->CreateSolidColorBrush( D2D1::ColorF( it->r, it->g, it->b), &aBrush );

		D2D1_ELLIPSE ellipse;
		ellipse.point.x = it->x;
		ellipse.point.y = it->y;
		ellipse.radiusX = it->radius;
		ellipse.radiusY = it->radius;

		if( it->fill )
			pRT->FillEllipse( &ellipse, aBrush );
		else
			pRT->DrawEllipse( &ellipse, aBrush );

		aBrush->Release();
	}

	pRT->EndDraw();

	if( currentFrame.imageData )
		free( currentFrame.imageData );
	if( currentFrame.colorImageData )
		free( currentFrame.colorImageData );
}

void ImageWindow::UpdateImageBW( const UINT8* imageData, UINT width, UINT height )
{
	if( pRT && greyBitmap )
	{
		Mutex::Locker locker( frontBufferMutex );

		Frame frame = {0};
		frame.imageData = malloc( width * height );
		frame.width = width;
		frame.height = height;
		memcpy( frame.imageData, imageData, width * height );

		frames.PushBack( frame );
	}
}

void ImageWindow::UpdateImageRGBA( const UINT8* imageData, UINT width, UINT height, UINT pitch )
{
	if( pRT && colorBitmap )
	{
		Mutex::Locker locker( frontBufferMutex );

		Frame frame = {0};
		frame.colorImageData = malloc( pitch * height );
		frame.width = width;
		frame.height = height;
		frame.colorPitch = pitch;
		memcpy( frame.colorImageData, imageData, pitch * height );

		frames.PushBack( frame );
	}
}

void ImageWindow::addCircle( float x, float y, float radius, float r, float g, float b, bool fill )
{
	if( pRT )
	{
		CirclePlot cp;

		cp.x = x;
		cp.y = y;
		cp.radius = radius;
		cp.r = r;
		cp.g = g;
		cp.b = b;
		cp.fill = fill;

		Mutex::Locker locker( frontBufferMutex );
		Frame& frame = frames.PeekBack( 0 );
		frame.plots.PushBack( cp );
	}

}

}}
