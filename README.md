OculusSDK
=========

Oculus SDK with additional community supported features and bug-fixes.  

Goals
-----

* Providing new features
    * C bindings
    * Python bindings
    * Java bindings
* Providing new tools
    * Command line diagnostics for people having trouble with Rift detection
* Providing new sample code
    * Alternative mechanisms for rendering the Rift distortion
    * Sample code for the non-C++ language bindings 
* Helping developers write better or easier Oculus Rift compatible software 

Licensing
---------

All code not explicitly marked with another license is made available under the
terms of the Oculus SDK license, included in this kit as LICENSE.TXT and 
available online at https://developer.oculusvr.com/license and is Copyright 2013
Oculus VR, Inc.

Branches
--------

### official
This branch will track the SDK releases from Oculus VR.  It should contain only files that are included in the official 
SDK, as well as the minimal additional files to get it to play well with GitHub.  Specifically this README file for the
uninitiated and a .gitignore file to exclude artifacts from the official SDK that we do not wish to track in GitHub.  
The latter mostly consists of the binary files distributed with the SDK and documentation or any other artifacts 
with unknown or suspect licensing that would prohibit reproduction on GitHub. 

### stable
Contains features that are considered complete and safe to use.

### unstable
The tip of shared development.  Contains features that may not have been merged into stable.  Not guaranteed to work 
at any given time.

### feature-XXX
Feature branches for changes that will require significant time and codebase changes to function, but 
which may be being developed in a team or made available for testing to the general public.


