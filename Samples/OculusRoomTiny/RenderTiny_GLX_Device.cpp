#include "RenderTiny_GL_Device.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

extern Display * x_display;

namespace OVR { namespace RenderTiny { namespace GL {


void RenderDevice::Present() {
    glXSwapBuffers(x_display, (Window)oswnd);
}

} // Namespace GL

// Implement static initializer function to create this class.
RenderTiny::RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* oswnd)
{
    int attr[16];
    int nattr = 2;

    attr[0] = GLX_RGBA;
    attr[1] = GLX_DOUBLEBUFFER;
    attr[nattr++] = GLX_DEPTH_SIZE;
    attr[nattr++] = 24;
    attr[nattr] = 0;
    int screenNumber = DefaultScreen(x_display);
    XVisualInfo* Vis = glXChooseVisual(x_display, screenNumber, attr);
    GLXContext context = glXCreateContext(x_display, Vis, 0, GL_TRUE);

    if (!glXMakeCurrent(x_display, (Window)oswnd, context))
    {
        glXDestroyContext(x_display, context);
        return NULL;
    }

    return new GL::RenderDevice(rp, oswnd);
}

} // Namespace RenderTiny
} // Namespace OVR

//
//
//Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* oswnd)
//{
//
//    if (!context)
//        return NULL;
//
//    if (!glXMakeCurrent(PC->Disp, PC->Win, context))
//    {
//        glXDestroyContext(PC->Disp, context);
//        return NULL;
//    }
//
//    XMapRaised(PC->Disp, PC->Win);
//
//    return new Render::GL::Linux::RenderDevice(rp, PC->Disp, PC->Win, context);
//}
//
//void RenderDevice::Present()
//{
//    glXSwapBuffers(Disp, Win);
//}
//
//void RenderDevice::Shutdown()
//{
//    if (Context)
//    {
//        glXMakeCurrent(Disp, 0, NULL);
//        glXDestroyContext(Disp, Context);
//        Context = NULL;
//        Win = 0;
//    }
//}
