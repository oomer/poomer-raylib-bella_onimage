SDKNAME			=bella_engine_sdk
OUTNAME			=poomer-raylib-bella_onimage
UNAME           =$(shell uname)

ifeq ($(UNAME), Darwin)

	SDKBASE		= ../bella_engine_sdk

	SDKFNAME    = lib$(SDKNAME).dylib
	INCLUDEDIRS	= -I$(SDKBASE)/src
	INCLUDEDIRS2	= -I../raygui/src 
	INCLUDEDIRS3	= -I../raylib/src
	RAYLIBDIR		= ../raylib/src
	RAYLIBNAME    = libraylib.dylib
	LIBDIR		= $(SDKBASE)/lib
	LIBDIRS		= -L$(LIBDIR) -L$(RAYLIBDIR)
	OBJDIR		= obj/$(UNAME)
	BINDIR		= bin/$(UNAME)
	OUTPUT      = $(BINDIR)/$(OUTNAME)

	ISYSROOT	= /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk

	CC			= clang
	CXX			= clang++

	CCFLAGS		= -arch x86_64\
				  -arch arm64\
				  -mmacosx-version-min=11.0\
				  -isysroot $(ISYSROOT)\
				  -fvisibility=hidden\
				  -O3\
				  $(INCLUDEDIRS)\
				  $(INCLUDEDIRS2)\
				  $(INCLUDEDIRS3)

	CFLAGS		= $(CCFLAGS)\
				  -std=c11

	CXXFLAGS    = $(CCFLAGS)\
				  -std=c++11
				
	CPPDEFINES  = -DNDEBUG=1\
				  -DDL_USE_SHARED

	LIBS		= -l$(SDKNAME)\
				  -lraylib\
				  -lm\
				  -ldl\
				  -framework OpenGL\
				  -framework CoreFoundation\
				  -framework AppKit\
				  -framework IOKit\
				  -framework CoreGraphics\
				  -framework Foundation

	LINKFLAGS   = -mmacosx-version-min=11.0\
				  -isysroot $(ISYSROOT)\
				  -framework Cocoa\
				  -framework IOKit\
				  -framework CoreVideo\
				  -framework CoreFoundation\
				  -framework Accelerate\
				  -fvisibility=hidden\
				  -O5\
				  -rpath @executable_path\
				  -weak_library $(LIBDIR)/libvulkan.dylib
else

	SDKBASE		= ../bella_engine_sdk

	SDKFNAME    = lib$(SDKNAME).so
	INCLUDEDIRS	= -I$(SDKBASE)/src
	INCLUDEDIRS2    = -I../raygui/src
	INCLUDEDIRS3    = -I../raylib/src
	LIBDIR		= $(SDKBASE)/lib
	RAYLIBDIR		= ../raylib/src
	RAYLIBNAME    = libraylib.so
	SODDIR		= /usr/lib/x86_64-linux-gnu/
	LIBDIRS		= -L$(LIBDIR) -L$(RAYLIBDIR)
	OBJDIR		= obj/$(UNAME)
	BINDIR		= bin/$(UNAME)
	OUTPUT      = $(BINDIR)/$(OUTNAME)

	CC			= gcc
	CXX			= g++

	CCFLAGS		= -m64\
				  -Wall\
				  -fvisibility=hidden\
				  -D_FILE_OFFSET_BITS=64\
				  -O3\
				  $(INCLUDEDIRS)\
				  $(INCLUDEDIRS2)\
                  $(INCLUDEDIRS3)

	CFLAGS		= $(CCFLAGS)\
				  -std=c11

	CXXFLAGS    = $(CCFLAGS)\
				  -std=c++11
				
	CPPDEFINES  = -DNDEBUG=1\
				  -DDL_USE_SHARED

	LIBS		= -l$(SDKNAME)\
				  -lraylib\
				  -lm\
				  -ldl\
				  -lrt\
				  -lpthread\
				  -lX11\
				  -lGL\
				  -lvulkan

	LINKFLAGS   = -m64\
				  -fvisibility=hidden\
				  -O3\
				  -Wl,-rpath,'$$ORIGIN'\
				  -Wl,-rpath,'$$ORIGIN/lib'
endif

OBJS = poomer-raylib-bella_onimage.o 
OBJ = $(patsubst %,$(OBJDIR)/%,$(OBJS))

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPDEFINES)

$(OUTPUT): $(OBJ)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(LINKFLAGS) $(LIBDIRS) $(LIBS)
	@cp $(LIBDIR)/$(SDKFNAME) $(BINDIR)/$(SDKFNAME)

.PHONY: clean
clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(OUTPUT)
	rm -f $(BINDIR)/$(SDKFNAME)
	rm -f $(BINDIR)/$(RAYLIBNAME)
