/************************************************************************************

Filename    :   Render_GL_Win32 Device.cpp
Content     :   Win32 OpenGL Device implementation
Created     :   September 10, 2012
Authors     :   Andrew Reisse, Michael Antonov

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

************************************************************************************/

#include "Render_GL_GLUT_Device.h"
#include <GL/glut.h>

namespace OVR { namespace Render { namespace GL { namespace GLUT {


// ***** GL::GLUT::RenderDevice

// Implement static initializer function to create this class.
Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams& p, void* oswnd)
{
    return new RenderDevice(p, *static_cast<int*>(oswnd));
}


void RenderDevice::Present()
{
    glutSwapBuffers();
}

}}}}

