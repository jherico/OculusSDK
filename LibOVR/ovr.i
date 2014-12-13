%include "enums.swg"
%javaconst(1);
%module ovr
%{
/* Includes the header in the wrapper code */
#define OVR_ALIGNAS(T) 
#include "Include/OVR_Version.h"
#include "SRC/OVR_CAPI.h"
#include "SRC/OVR_CAPI_GL.h"
%}
 
/* Parse the header file to generate wrappers */
%define OVR_ALIGNAS(T)
%enddef 
%include "Include/OVR_Version.h"
%include "SRC/OVR_CAPI.h"
%include "SRC/OVR_CAPI_GL.h"
