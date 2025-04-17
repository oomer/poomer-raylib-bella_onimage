# Project configuration
BELLA_SDK_NAME    = bella_engine_sdk
EXECUTABLE_NAME   = poomer-raylib-bella_onimage
PLATFORM          = $(shell uname)
BUILD_TYPE        ?= release# Default to release build if not specified

# Common paths
BELLA_SDK_PATH    = ../bella_engine_sdk
RAYGUI_PATH       = ../raygui
RAYLIB_PATH       = ../raylib
# Path to dynamic raylib library
RAYLIB_DYLIB_PATH = ../raylib/build/raylib
OBJ_DIR           = obj/$(PLATFORM)/$(BUILD_TYPE)
BIN_DIR           = bin/$(PLATFORM)/$(BUILD_TYPE)
OUTPUT_FILE       = $(BIN_DIR)/$(EXECUTABLE_NAME)

# Platform-specific configuration
ifeq ($(PLATFORM), Darwin)
    # macOS configuration
    SDK_LIB_EXT          = dylib
    SDK_LIB_FILE         = lib$(BELLA_SDK_NAME).$(SDK_LIB_EXT)
    # Dynamic raylib library
    RAYLIB_LIB_NAME      = libraylib.$(SDK_LIB_EXT)
    MACOS_SDK_PATH       = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    
    # Compiler settings
    CC                   = clang
    CXX                  = clang++
    
    # Architecture flags
    ARCH_FLAGS           = -arch x86_64 -arch arm64 -mmacosx-version-min=15.0 -isysroot $(MACOS_SDK_PATH)
    
    # Include paths
    INCLUDE_PATHS        = -I$(BELLA_SDK_PATH)/src \
                           -I$(RAYGUI_PATH)/src \
                           -I$(RAYLIB_PATH)/src
    
    # Library paths
    SDK_LIB_PATH         = $(BELLA_SDK_PATH)/lib
    # Updated path to dynamic raylib library
    RAYLIB_LIB_PATH      = $(RAYLIB_DYLIB_PATH)
    LIB_PATHS            = -L$(SDK_LIB_PATH) -L$(RAYLIB_LIB_PATH)
    
    # Platform-specific libraries
    # For dynamic library, we don't need to explicitly link all of raylib's dependencies
    LIBRARIES            = -l$(BELLA_SDK_NAME) \
                           -lraylib \
                           -lm \
                           -ldl \
                           -framework OpenGL \
                           -framework CoreFoundation \
                           -framework AppKit \
                           -framework IOKit \
                           -framework CoreGraphics \
                           -framework CoreVideo \
                           -framework Foundation
    
    # Linking flags
    LINKER_FLAGS         = -mmacosx-version-min=15.0 \
                           -isysroot $(MACOS_SDK_PATH) \
                           -framework Cocoa \
                           -framework IOKit \
                           -framework CoreVideo \
                           -framework CoreFoundation \
                           -framework Accelerate \
                           -fvisibility=hidden \
                           -O5 \
                           -rpath @executable_path \
                           -rpath @loader_path \
                           -weak_library $(SDK_LIB_PATH)/libvulkan.dylib
else
    # Linux configuration
    SDK_LIB_EXT          = so
    SDK_LIB_FILE         = lib$(BELLA_SDK_NAME).$(SDK_LIB_EXT)
    # Dynamic raylib library
    RAYLIB_LIB_NAME      = libraylib.$(SDK_LIB_EXT)
    
    # Compiler settings
    CC                   = gcc
    CXX                  = g++
    
    # Architecture flags
    ARCH_FLAGS           = -m64 -D_FILE_OFFSET_BITS=64
    
    # Include paths
    INCLUDE_PATHS        = -I$(BELLA_SDK_PATH)/src \
                           -I$(RAYGUI_PATH)/src \
                           -I$(RAYLIB_PATH)/src
    
    # Library paths
    SDK_LIB_PATH         = $(BELLA_SDK_PATH)/lib
    # Updated path to dynamic raylib library
    RAYLIB_LIB_PATH      = $(RAYLIB_DYLIB_PATH)
    SYSTEM_LIB_PATH      = /usr/lib/x86_64-linux-gnu/
    LIB_PATHS            = -L$(SDK_LIB_PATH) -L$(RAYLIB_LIB_PATH)
    
    # Platform-specific libraries
    # For dynamic library, we don't need to explicitly link all dependencies of raylib
    LIBRARIES            = -l$(BELLA_SDK_NAME) \
                           -lraylib \
                           -lm \
                           -ldl \
                           -lrt \
                           -lpthread \
                           -lX11 \
                           -lGL \
                           -lvulkan
    
    # Linking flags
    LINKER_FLAGS         = -m64 \
                           -fvisibility=hidden \
                           -O3 \
                           -Wl,-rpath,'$$ORIGIN' \
                           -Wl,-rpath,'$$ORIGIN/lib'
endif

# Build type specific flags
ifeq ($(BUILD_TYPE), debug)
    ifeq ($(PLATFORM), Darwin)
        CPP_DEFINES          = -D_DEBUG -DDL_USE_SHARED
    else
        CPP_DEFINES          = -D_DEBUG -DDL_USE_SHARED
    endif
    COMMON_FLAGS         = $(ARCH_FLAGS) -fvisibility=hidden -g -O0 $(INCLUDE_PATHS)
else
    ifeq ($(PLATFORM), Darwin)
        CPP_DEFINES          = -DNDEBUG=1 -DDL_USE_SHARED
    else
        CPP_DEFINES          = -DNDEBUG=1 -DDL_USE_SHARED
    endif
    COMMON_FLAGS         = $(ARCH_FLAGS) -fvisibility=hidden -O3 $(INCLUDE_PATHS)
endif

# Language-specific flags
C_FLAGS                  = $(COMMON_FLAGS) -std=c11
CXX_FLAGS                = $(COMMON_FLAGS) -std=c++17

# Objects
OBJECTS                  = $(EXECUTABLE_NAME).o
OBJECT_FILES             = $(patsubst %,$(OBJ_DIR)/%,$(OBJECTS))

# Build rules
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) -c -o $@ $< $(CXX_FLAGS) $(CPP_DEFINES)

$(OUTPUT_FILE): $(OBJECT_FILES)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(LINKER_FLAGS) $(LIB_PATHS) $(LIBRARIES)
	@echo "Copying libraries to $(BIN_DIR)..."
	@cp $(SDK_LIB_PATH)/$(SDK_LIB_FILE) $(BIN_DIR)/$(SDK_LIB_FILE)
ifeq ($(PLATFORM), Darwin)
	@# Copy raylib dylib files preserving symbolic links
	@cp -P $(RAYLIB_LIB_PATH)/libraylib.5.5.0.dylib $(BIN_DIR)/
	@cp -P $(RAYLIB_LIB_PATH)/libraylib.550.dylib $(BIN_DIR)/
	@cp -P $(RAYLIB_LIB_PATH)/libraylib.dylib $(BIN_DIR)/
else
	@cp $(RAYLIB_LIB_PATH)/$(RAYLIB_LIB_NAME) $(BIN_DIR)/$(RAYLIB_LIB_NAME)
endif
	@echo "Build complete: $(OUTPUT_FILE)"

# Add default target
all: $(OUTPUT_FILE)

.PHONY: clean cleanall all
clean:
	rm -f $(OBJ_DIR)/*.o
	rm -f $(OUTPUT_FILE)
	rm -f $(BIN_DIR)/$(SDK_LIB_FILE)
ifeq ($(PLATFORM), Darwin)
	rm -f $(BIN_DIR)/libraylib*.dylib
else
	rm -f $(BIN_DIR)/$(RAYLIB_LIB_NAME)
endif
	rmdir $(OBJ_DIR) 2>/dev/null || true
	rmdir $(BIN_DIR) 2>/dev/null || true

cleanall:
	rm -f obj/*/release/*.o
	rm -f obj/*/debug/*.o
	rm -f bin/*/release/$(EXECUTABLE_NAME)
	rm -f bin/*/debug/$(EXECUTABLE_NAME)
	rm -f bin/*/release/$(SDK_LIB_FILE)
	rm -f bin/*/debug/$(SDK_LIB_FILE)
	rm -f bin/*/release/*.$(SDK_LIB_EXT)
	rm -f bin/*/debug/*.$(SDK_LIB_EXT)
	rmdir obj/*/release 2>/dev/null || true
	rmdir obj/*/debug 2>/dev/null || true
	rmdir bin/*/release 2>/dev/null || true
	rmdir bin/*/debug 2>/dev/null || true
	rmdir obj/* 2>/dev/null || true
	rmdir bin/* 2>/dev/null || true
	rmdir obj 2>/dev/null || true
	rmdir bin 2>/dev/null || true
