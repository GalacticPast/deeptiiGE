obj_dir := obj
src_dir := testbed
bin_dir := bin
cc := clang

ifeq ($(OS),Windows_NT)

vulkan_sdk := $(shell echo %VULKAN_SDK%)

DIR := $(subst /,\,${CURDIR})

assembly := learningVulkan
extension := .exe
defines := -DDEBUG -DDPLATFORM_WINDOWS
include_flags := -Iengine\src -I$(vulkan_sdk)\Include
linker_flags := -g -lengine -L$(obj_dir)\engine -L$(bin_dir) #-Wl,-rpath,.
compiler_flags := -Wall -Wextra -g3 -Wconversion -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion -fsanitize=undefined -fsanitize-trap
build_platform := windows

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

src_files := $(call rwildcard,$(src_dir)/,*.c) # Get all .c files
directories := \$(src_dir)\src $(subst $(DIR),,$(shell dir $(src_dir)\src /S /AD /B | findstr /i src)) # Get all directories under src.
obj_files := $(src_files:%=$(obj_dir)/%.o) # Get all compiled .c.o objects for tesbed


else

is_linux := $(shell uname -s)

ifeq ($(is_linux),Linux)

linux_platform := $(shell echo "$$XDG_SESSION_TYPE")

assembly := learningVulkan
extension := 
include_flags := -Iengine/src -I$(VULKAN_SDK)/include
compiler_flags := -Wall -Wextra -g3 -WconINCLUDE_FLAGS version -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion -fsanitize=undefined -fsanitize-trap
linker_flags := -l./$(build_dir)/ -lengine -wl,-rpath,.

ifeq ($(linux_platform),wayland)		

linker_flags := -lvulkan -lwayland-client -lm
defines := -DDEBUG -DPLATFORM_LINUX_WAYLAND

else ifeq ($(linux_platform),x11)		

defines := -D_DEBUG -DPLATFORM_LINUX_X11 
linker_flags := -lvulkan -lX11 -lxcb -lX11-xcb -L/usr/X11R6/lib -lm

endif

src_files := $(shell find $(src_dir) -type f -name '*.c')
dependencies := $(shell find $(src_dir) -type d)
obj_files := $(patsubst %.c, $(obj_dir)/%.o, $(src_files))

endif 

endif 

all: scaffold compile link

.PHONY: scaffold
scaffold: 
ifeq ($(build_platform),windows)
	@echo scaffolding project structure 
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(obj_dir) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addsuffix \$(src_dir),$(obj_dir)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(obj_dir), $(directories)) 2>NUL || cd .
else
	@mkdir -p $(obj_dir)
	@mkdir -p $(dir $(obj_files))
endif

$(obj_dir)/%.c.o: %.c # compile .c to .c.o object
	@echo   $<...
	@clang $< $(compiler_flags) -c -o $@ $(defines) $(include_flags)

.PHONY: compile
compile: #compile .c files
	@echo Compiling...

.PHONY: link
link: scaffold $(obj_files)
	@echo Linking $(assembly)
	@clang $(obj_files) -o $(bin_dir)\$(assembly)$(extension) $(linker_flags)

.PHONY: clean
clean: # clean build directory
ifeq ($(build_platform),windows)
	if exist $(bin_dir)\$(assembly)$(extension) del $(bin_dir)\$(assembly)$(extension)
	rmdir /s /q $(obj_dir)\$(assembly)
else
	rm -rf $(bin_dir)/$(assembly)
	rm -rf $(obj_dir)/$(assembly)
endif

