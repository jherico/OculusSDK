include LibOVR/Projects/Linux/LibOVRConfig.mk

DESTDIR =
PREFIX = $(DESTDIR)/usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include
SHRDIR = $(PREFIX)/share

CFLAGS += -DLIBDIR=$(LIBDIR) -DSHRDIR=$(SHRDIR)
CXXFLAGS += -DLIBDIR=$(LIBDIR) -DSHRDIR=$(SHRDIR)

CC  = gcc
CXX = g++

all: debug

release:
	CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" $(MAKE) -C Samples/OculusWorldDemo/Projects/Linux -f OculusWorldDemo.mk

debug:
	CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" $(MAKE) -C Samples/OculusWorldDemo/Projects/Linux -f OculusWorldDemo.mk DEBUG=1

clean:
	$(MAKE) -C Samples/OculusWorldDemo/Projects/Linux -f OculusWorldDemo.mk clean

install: release
	mkdir -p $(LIBDIR)
	install LibOVR/Lib/Linux/$(SYSARCH)/Release/libOVR.a $(LIBDIR)
	install LibOVR/Lib/Linux/$(SYSARCH)/Release/libOVRRT64_$(OVR_PRODUCT_VERSION).so.$(OVR_MAJOR_VERSION) $(LIBDIR)
	install LibOVR/Lib/Linux/$(SYSARCH)/Release/libOVRRT64_$(OVR_PRODUCT_VERSION).so.$(OVR_MAJOR_VERSION).$(OVR_MINOR_VERSION).$(OVR_PATCH_VERSION) $(LIBDIR)

	mkdir -p $(SHRDIR)/OculusWorldDemo/Assets/Tuscany
	install Samples/OculusWorldDemo/Assets/Tuscany/* $(SHRDIR)/OculusWorldDemo/Assets/Tuscany

	mkdir -p $(BINDIR)
	install Service/OVRServer/Bin/Linux/$(SYSARCH)/ReleaseStatic/ovrd $(BINDIR)
	install Samples/OculusWorldDemo/Bin/$(SYSARCH)/Release/OculusWorldDemo $(BINDIR)
	install Tools/RiftConfigUtil/Bin/Linux/$(SYSARCH)/ReleaseStatic/RiftConfigUtil $(BINDIR)

	mkdir -p $(INCDIR)/Extras
	install LibOVR/Include/OVR_CAPI_$(OVR_PRODUCT_VERSION)_$(OVR_MAJOR_VERSION)_$(OVR_MINOR_VERSION).h $(INCDIR)
	install LibOVR/Include/OVR_CAPI_GL.h $(INCDIR)
	install LibOVR/Include/OVR_CAPI.h $(INCDIR)
	install LibOVR/Include/OVR_CAPI_Keys.h $(INCDIR)
	install LibOVR/Include/OVR_CAPI_Util.h $(INCDIR)
	install LibOVR/Include/OVR_ErrorCode.h $(INCDIR)
	install LibOVR/Include/OVR.h $(INCDIR)
	install LibOVR/Include/OVR_Kernel.h $(INCDIR)
	install LibOVR/Include/OVR_Version.h $(INCDIR)
	install LibOVR/Include/Extras/OVR_Math.h $(INCDIR)/Extras

uninstall:
	rm -vf $(LIBDIR)/libOVR.a
	rm -vf $(LIBDIR)/libOVRRT64_$(OVR_PRODUCT_VERSION).so.$(OVR_MAJOR_VERSION)
	rm -vf $(LIBDIR)/libOVRRT64_$(OVR_PRODUCT_VERSION).so.$(OVR_MAJOR_VERSION).$(OVR_MINOR_VERSION).$(OVR_PATCH_VERSION)

	rm -vf $(BINDIR)/ovrd
	rm -vf $(BINDIR)/OculusWorldDemo

	rm -vf $(SHRDIR)/OculusWorldDemo/Assets/Tuscany/*
	rm -vfr $(SHRDIR)/OculusWorldDemo/Assets/Tuscany || true
	rm -vfr $(SHRDIR)/OculusWorldDemo/Assets || true
	rm -vfr $(SHRDIR)/OculusWorldDemo || true
	rm -vf $(SHRDIR)/RiftConfigUtil/Resources/DeskScene*.*
	rm -vfr $(SHRDIR)/RiftConfigUtil/Resources/DeskScene || true
	rm -vf $(SHRDIR)/RiftConfigUtil/Resources/*.*
	rm -vfr $(SHRDIR)/RiftConfigUtil/Resources || true
	rm -vfr $(SHRDIR)/RiftConfigUtil || true
	rm -vfr $(SHRDIR) || true

	rm -vf $(INCDIR)/OVR_CAPI_$(OVR_PRODUCT_VERSION)_$(OVR_MAJOR_VERSION)_$(OVR_PATCH_VERSION).h
	rm -vf $(INCDIR)/OVR_CAPI_GL.h
	rm -vf $(INCDIR)/OVR_CAPI.h
	rm -vf $(INCDIR)/OVR_CAPI_Keys.h
	rm -vf $(INCDIR)/OVR_CAPI_Util.h
	rm -vf $(INCDIR)/OVR_ErrorCode.h
	rm -vf $(INCDIR)/OVR.h
	rm -vf $(INCDIR)/OVR_Kernel.h
	rm -vf $(INCDIR)/OVR_Version.h
	rm -vf $(INCDIR)/Extras/OVR_Math.h
	rm -vfr $(INCDIR)/Extras || true
