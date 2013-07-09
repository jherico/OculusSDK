DEBUG         = 0
OVR_ROOT      = ../..
SAMPLE_NAME	  = CommonSrc
DEFINES	      = -DGL_GLEXT_PROTOTYPES

include $(OVR_ROOT)/LibOVR/Projects/Linux/Makefile.common
include $(OVR_ROOT)/Samples/Makefile.common

LINK		  = $(LINK_ARCHIVE)
INCPATH       = $(LIBOVRINCPATH)
SRCPATH       = .
TARGET        = $(TARGETPATH)/libCommonSrc.a
CXXBUILD      = $(CXX) -c $(CXXFLAGS) $(LIBOVRINCPATH) -o $(OBJPATH)/

CPP_SRCS = $(filter-out \
       $(SRCPATH)/Render/Render_D3D%.cpp \
       $(SRCPATH)/Render/Render_GL_GLUT%.cpp \
       $(SRCPATH)/Render/Render_GL_Win32%.cpp \
       $(SRCPATH)/Render/Render_SDL%.cpp \
       $(SRCPATH)/Platform/OSX_%.cpp \
       $(SRCPATH)/Platform/Win32_%.cpp \
       $(SRCPATH)/Platform/SDL_%.cpp \
       $(SRCPATH)/Platform/GLUT_%.cpp \
       , $(shell find ${SRCPATH} -name "*.cpp"))

OBJECTS = $(patsubst ${SRCPATH}/%.cpp, ${OBJPATH}/%.o, ${CPP_SRCS})
DIRS = $(subst /,/,$(sort $(dir $(OBJECTS)))) $(TARGETPATH)

all:    $(TARGET)

$(TARGET): $(OBJECTS) $(LIBOVRARCHIVE)
	$(MD) -p $(DIRS)
	$(LINK) $(TARGET) $(OBJECTS) $(LIBOVRARCHIVE)

$(OBJPATH)/%.o: $(SRCPATH)/%.cpp
	$(MD) -p $(DIRS)
	$(CXXBUILD) -o "$@" "$<"

# Other Targets
clean:
	$(DELETEFILE) -r $(OBJPATH) $(TARGETPATH) 
	@echo ' '
