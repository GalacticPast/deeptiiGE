obj_dir := obj
src_dir := testbed/src
bin_dir := bin
cc := clang

ifeq ($(OS),Windows_NT)

vulkan_sdk := $(shell echo %VULKAN_SDK%)

assembly := learningVulkan
extension := .exe
defines := -D_DEBUG -DPLATFORM_WINDOWS
includes := -Isrc -I$(vulkan_sdk)\Include
linker_flags := -g -lengine.lib -L$(obj_dir)\engine -L$(bin_dir) -luser32 -lvulkan-1 -L$(vulkan_sdk)\Lib 
compiler_flags := -Wall -Wextra -g3 -Wconversion -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion -fsanitize=undefined -fsanitize-trap
build_platform := windows

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
src_files := $(call rwildcard,$(src_dir)/,*.c)
directories := $(shell dir src /b /a:d)
obj_files := $(patsubst %.c, $(obj_dir)/%.o, $(src_files))

else

is_linux := $(shell uname -s)

ifeq ($(is_linux),Linux)

linux_platform := $(shell echo "$$XDG_SESSION_TYPE")

assembly := learningVulkan
extension := 
includes := -Iengine/src -I$(VULKAN_SDK)/include
compiler_flags := -Wall -Wextra -g3 -WconINCLUDE_FLAGS version -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion -fsanitize=undefined -fsanitize-trap
linker_flags := -l./$(build_dir)/ -lengine -wl,-rpath,.

ifeq ($(linux_platform),wayland)		

linker_flags := -lvulkan -lwayland-client -lm
defines := -D_DEBUG -DPLATFORM_LINUX_WAYLAND

else ifeq ($(linux_platform),x11)		

defines := -D_DEBUG -DPLATFORM_LINUX_X11 
linker_flags := -lvulkan -lX11 -lxcb -lX11-xcb -L/usr/X11R6/lib -lm

endif

src_files := $(shell find $(src_dir) -type f -name '*.c')
dependencies := $(shell find $(src_dir) -type d)
obj_files := $(patsubst %.c, $(obj_dir)/%.o, $(src_files))

endif 

endif 

all: scaffold link

.PHONY: scaffold
scaffold: 
ifeq ($(build_platform),windows)
	@echo scaffolding project structure 
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(obj_dir) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addsuffix \$(src_dir),$(obj_dir)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(obj_dir)\$(src_dir)\,$(directories)) 2>NUL || cd .
else
	@mkdir -p $(obj_dir)
	@mkdir -p $(dir $(obj_files))
endif

$(obj_dir)/%.o : %.c 
	@echo $<...
	@$(cc) $< $(compiler_flags) -c  -o $@ $(defines) $(includes) 

.PHONY: link
link: $(obj_files)
	@echo Linking 
	@$(cc) $(compile_flags) $^ -o $(bin_dir)/$(assembly)$(extension) $(includes) $(defines) $(linker_flags) 

.PHONY: clean
clean: # clean build directory
ifeq ($(build_platform),windows)
	if exist $(bin_dir)\$(assembly)$(extension) del $(bin_dir)\$(assembly)$(extension)
	rmdir /s /q $(obj_dir)\$(assembly)
else
	rm -rf $(bin_dir)/$(assembly)
	rm -rf $(obj_dir)/$(assembly)
endif

