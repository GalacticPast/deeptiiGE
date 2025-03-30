obj_dir := obj
src_dir := engine/src
bin_dir := bin
cc := clang

ifeq ($(OS),Windows_NT)

build_platform := windows
vulkan_sdk := $(shell echo %VULKAN_SDK%)

assembly := engine
extension := .dll
defines := -D_DEBUG _DDEXPORT -D_CRT_SECURE_NO_WARNINGS -DPLATFORM_WINDOWS
includes := -Iengine\src -I$(vulkan_sdk)\Include
linker_flags := -g -shared -luser32 -lvulkan-1 -L$(VULKAN_SDK)\Lib -L$(OBJ_DIR)\engine
compiler_flags := -g -fdeclspec -fPIC

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
src_files := $(call rwildcard,$(src_dir)/,*.c)
directories := $(shell dir src /b /a:d)
obj_files := $(patsubst %.c, $(obj_dir)/%.o, $(src_files))

else

is_linux := $(shell uname -s)

ifeq ($(is_linux),Linux)

assembly := engine
extension := .so
includes := -Iengine/src -I$(VULKAN_SDK)/include
compiler_flags := -g -fdeclspec -fPIC
defines := -D_DEBUG -DDEXPORT 

linux_platform := $(shell echo "$$XDG_SESSION_TYPE")

ifeq ($(linux_platform),wayland)		

defines += -DDPLATFORM_LINUX_WAYLAND
linker_flags := -lvulkan -lwayland-client -lm -lc

else ifeq ($(linux_platform),x11)		

defines += -DDPLATFORM_LINUX_X11
linker_flags := -lvulkan -lX11 -lxcb -lX11-xcb -L/usr/X11R6/lib -lm

endif

src_files := $(shell find $(assembly) -name *.c)		# .c files
directories := $(shell find $(assembly) -type d)		# directories with .h files
obj_files := $(src_files:%=$(obj_dir)/%.o)	

endif 

endif 

all: scaffold compile link

.PHONY: scaffold
scaffold: 
ifeq ($(build_platform),windows)
	@echo scaffolding project structure 
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(bin_dir) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(obj_dir) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addsuffix \$(src_dir),$(obj_dir)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(obj_dir)\$(src_dir)\,$(directories)) 2>NUL || cd .
else
	@mkdir -p $(obj_dir)
	@mkdir -p $(bin_dir)
	@mkdir -p $(addprefix $(obj_dir)/,$(directories))
endif


.PHONY: link
link: scaffold $(obj_files)
	@echo Linking $(assembly)
	@$(cc) $(obj_files) -o $(bin_dir)/lib$(assembly)$(extension) $(linker_flags) 

.PHONY: compile
compile: #compile .c files
ifeq ($(build_platform),windows)
	@echo Compiling for Windows...
else
	@echo Compiling for linux-$(linux_platform)...
endif

.PHONY: clean
clean: # clean build directory
ifeq ($(build_platform),windows)
	if exist $(bin_dir)\$(assembly)$(extension) del $(bin_dir)\$(assembly)$(extension)
	rmdir /s /q $(obj_dir)\$(assembly)
else
	rm -rf $(bin_dir)\$(assembly)
	rm -rf $(obj_dir)\$(assembly)
endif

$(obj_dir)/%.c.o : %.c 
	@echo $<...
	@$(cc) $< $(compiler_flags) -c  -o $@ $(defines) $(includes) 
