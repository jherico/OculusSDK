
# Makefile for LibOVR
# Miguel Leitao, 2017


INCDIR=../Include
INSTALL_DIR=/usr/local/lib64
TARGET=libOVR.so

${TARGET}: OVR_CAPIShim.o OVR_CAPI_Util.o OVR_StereoProjection.o
	gcc -o $@ -shared $^

OVR_CAPIShim.o: OVR_CAPIShim.c OVR_CAPI_Prototypes.h
	gcc -I ${INCDIR} -c -fPIC $<

OVR_CAPI_Util.o: OVR_CAPI_Util.cpp
	g++ -I ${INCDIR} -c -std=gnu++0x -fPIC -Wno-delete-incomplete $<

OVR_StereoProjection.o: OVR_StereoProjection.cpp
	g++ -I ${INCDIR} -c -std=gnu++0x -fPIC $<

.PHONY: install clean
install: $(TARGET)
	mkdir -p ${INSTALL_DIR}
	cp -p $^ ${INSTALL_DIR}

clean:
	rm -f ${TARGET} OVR_CAPIShim.o OVR_CAPI_Util.o OVR_StereoProjection.o

