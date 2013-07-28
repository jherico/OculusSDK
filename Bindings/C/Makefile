DEBUG         = 0
CXX           = g++
CXXFLAGS      = -fPIC -pipe $(DEFINES) -std=c++0x -std=gnu++0x
MD            = mkdir
DELETEFILE    = rm -f
SYSARCH       = $(shell uname -m)
CUSTOMPATH    = $(RELEASETYPE)/$(SYSARCH)

ifeq ($(DEBUG), 1)
	CXXFLAGS      += -DDEBUG -g3
	RELEASETYPE   = Debug
else
	CXXFLAGS      += -O2 
	RELEASETYPE   = Release
endif


TARGETPATH    = Build/$(CUSTOMPATH)
TARGET        = $(TARGETPATH)/libovr_rift.so
SRCPATH       = .
OBJPATH       = $(TARGETPATH)

# Filter out Windows and OSX files
SRCS = $(shell find . -name "*.cpp")
OBJS = $(patsubst ${SRCPATH}/%.cpp, ${OBJPATH}/%.o, ${SRCS})
DEPS = $(patsubst ${SRCPATH}/%.cpp, ${OBJPATH}/%.d, ${SRCS})
DIRS = $(subst /,/,$(sort $(dir $(OBJECTS)))) $(TARGETPATH)
PCH = OVR.h.gch
LIBS =  -ljson  -lboost_thread  -lboost_system

CPP_COMPILE = g++ -I$(OBJPATH) $(CXXFLAGS) -Wall -c -fmessage-length=0 -fPIC

# All Target
all: $(TARGET)

foo:
	@echo $(SRCS)

# Tool invocations
$(TARGET): $(OBJS) $(PCH)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++ -shared -o $(TARGET) $(OBJS) $(LIBS) 
	@echo 'Finished building target: $@'
	@echo ' '

$(OBJPATH)/%.o: ${SRCPATH}/%.cpp $(PCH)
	$(MD) -p $(DIRS)
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	$(CPP_COMPILE) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

$(PCH): OVR.h
	$(MD) -p $(DIRS)
	$(CPP_COMPILE) -o $(PCH) OVR.h

# Other Targets
clean:
	-$(RM) $(OBJS) $(DEPS) $(LIBRARIES) $(PCH) $(TARGET)
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

