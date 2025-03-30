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
include_flags := -Iengine/src -Itestbed/src 
compiler_flags := -Wall -Wextra -g3 -WconINCLUDE_FLAGS version -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion -fsanitize=undefined -fsanitize-trap
linker_flags := -l./$(bin_dir)/ -lengine -wl,-rpath,.  
defines := -DDEBUG 

ifeq ($(linux_platform),wayland)		

linker_flags += -lvulkan -lwayland-client -lm
defines += -DPLATFORM_LINUX_WAYLAND


else ifeq ($(linux_platform),x11)		

defines += -DPLATFORM_LINUX_X11 
linker_flags += -lX11 -lxcb -lX11-xcb -L/usr/X11R6/lib 

endif

src_files := $(shell find $(src_dir) -name *.c)		# .c files
directories := $(shell find $(src_dir) -type d)		# directories with .h files
obj_files := $(src_files:%=$(obj_dir)/%.o)		# compiled .o objects


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
	@echo Scaffolding folder structure...
	@mkdir -p $(addprefix $(obj_dir)/,$(directories))
	@echo Done.
endif

$(obj_dir)/%.c.o: %.c # compile .c to .o object
	@echo   $<...
	@clang $< $(compiler_flags) -c -o $@ $(defines) $(include_flags)

-include $(obj_files:.o=.d)

.PHONY: compile
compile: #compile .c files
	@echo Compiling...

.PHONY: link
link: scaffold $(obj_files)
	@echo Linking $(assembly)
	@clang $(obj_files) -o $(bin_dir)/$(assembly)$(extension) $(linker_flags)

.PHONY: clean
clean: # clean build directory
ifeq ($(build_platform),windows)
	if exist $(bin_dir)\$(assembly)$(extension) del $(bin_dir)\$(assembly)$(extension)
	rmdir /s /q $(obj_dir)\$(assembly)
else
	rm -rf $(bin_dir)/$(assembly)
	rm -rf $(obj_dir)/$(assembly)
endif

