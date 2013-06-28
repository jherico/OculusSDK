################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf
BUILD_DIR := ../build/Debug
ARTIFACT := ${BUILD_DIR}/libCommonSrc.a

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
Render/Render_Device.cpp \
Render/Render_GL_Device.cpp \
Render/Render_GL_GLUT_Device.cpp \
Render/Render_LoadTextureDDS.cpp \
Render/Render_LoadTextureTGA.cpp \
Render/Render_XmlSceneLoader.cpp \
Platform/GLUT_Platform.cpp \
Platform/Platform.cpp \
Platform/SDL_Gamepad.cpp 

OBJS += \
${BUILD_DIR}/Render/Render_Device.o \
${BUILD_DIR}/Render/Render_GL_Device.o \
${BUILD_DIR}/Render/Render_GL_GLUT_Device.o \
${BUILD_DIR}/Render/Render_LoadTextureDDS.o \
${BUILD_DIR}/Render/Render_LoadTextureTGA.o \
${BUILD_DIR}/Render/Render_XmlSceneLoader.o \
${BUILD_DIR}/Platform/GLUT_Platform.o \
${BUILD_DIR}/Platform/Platform.o \
${BUILD_DIR}/Platform/SDL_Gamepad.o 

CPP_DEPS += \
${BUILD_DIR}/Render/Render_Device.d \
${BUILD_DIR}/Render/Render_GL_Device.d \
${BUILD_DIR}/Render/Render_GL_GLUT_Device.d \
${BUILD_DIR}/Render/Render_LoadTextureDDS.d \
${BUILD_DIR}/Render/Render_LoadTextureTGA.d \
${BUILD_DIR}/Render/Render_XmlSceneLoader.d \
${BUILD_DIR}/Platform/GLUT_Platform.d \
${BUILD_DIR}/Platform/Platform.d \
${BUILD_DIR}/Platform/SDL_Gamepad.d 


# Each subdirectory must supply rules for building sources it contributes
${BUILD_DIR}/Render/%.o: Render/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I"../../LibOVR" -I"../../LibOVR/Include" -I"../../LibOVR/Src" -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


# Each subdirectory must supply rules for building sources it contributes
${BUILD_DIR}/Platform/%.o: Platform/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I"../../LibOVR" -I"../../LibOVR/Include" -I"../../LibOVR/Src" -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif


-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: $(ARTIFACT)

${BUILD_DIR}/Render:
	mkdir -p ${BUILD_DIR}/Render

${BUILD_DIR}/Platform: 
	mkdir -p ${BUILD_DIR}/Platform

# Tool invocations
$(ARTIFACT): ${BUILD_DIR}/Render ${BUILD_DIR}/Platform $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC Archiver'
	ar -r  $(ARTIFACT) $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C++_DEPS)$(C_DEPS)$(CC_DEPS)$(ARCHIVES)$(CPP_DEPS)$(CXX_DEPS)$(C_UPPER_DEPS) $(ARTIFACT)
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets


# Every subdirectory with source files must be described here
SUBDIRS := \
Render \
Platform \

################################################################################
# Automatically-generated file. Do not edit!
################################################################################

USER_OBJS :=

LIBS :=
################################################################################
# Automatically-generated file. Do not edit!
################################################################################

