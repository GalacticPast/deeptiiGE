obj_dir := obj
src_dir := engine
bin_dir := bin
cc := clang


ifeq ($(OS),Windows_NT)

build_platform := windows
vulkan_sdk := $(shell echo %VULKAN_SDK%)
DIR := $(subst /,\,${CURDIR})

assembly := engine
extension := .dll
defines := -D_CRT_SECURE_NO_WARNINGS -DDPLATFORM_WINDOWS $(addl_define_flags)
include_flags := -Iengine\src -I$(vulkan_sdk)\Include
linker_flags := -g -shared -luser32 -lvulkan-1 -L$(VULKAN_SDK)\Lib -L$(obj_dir)\engine
compiler_flags := -g -fdeclspec 

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

src_files := $(call rwildcard,$(src_dir)/,*.c)
directories := $(shell dir src /b /a:d)
obj_files := $(patsubst %.c, $(obj_dir)/%.o, $(src_files))

src_files:= $(call rwildcard,$(src_dir)/,*.c) # Get all .c files
directories:= \$(src_dir)\src $(subst $(DIR),,$(shell dir $(src_dir)\src /S /AD /B | findstr /i src)) # Get all directories under src.
obj_files:= $(src_files:%=$(obj_dir)/%.o) # Get all compiled .c.o objects for engine



else

is_linux := $(shell uname -s)

ifeq ($(is_linux),Linux)

assembly := engine
extension := .so
include_flags := -Iengine/src -I$(VULKAN_SDK)/include
compiler_flags := -g -fdeclspec -fPIC
defines := $(addl_define_flags) 
linker_flags :=-Wl,--no-undefined,--no-allow-shlib-undefined -shared -lm -L./$(bind_dir) -g -lvulkan 

linux_platform := $(shell echo "$$XDG_SESSION_TYPE")

ifeq ($(linux_platform),wayland)		

defines += -DDPLATFORM_LINUX_WAYLAND
linker_flags += -lwayland-client -lxkbcommon

else ifeq ($(linux_platform),x11)		

defines += -DDPLATFORM_LINUX_X11
linker_flags += -lX11 -lxcb -lX11-xcb -L/usr/X11R6/lib

endif

src_files := $(shell find $(src_dir) -name *.c)		# .c files
directories := $(shell find $(src_dir) -type d)		# directories with .h files
obj_files := $(src_files:%=$(obj_dir)/%.o)	

endif 

endif 

all: scaffold compile link

.PHONY: scaffold
scaffold: 
ifeq ($(build_platform),windows)
	@echo Scaffolding folder structure...
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(bin_dir) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(obj_dir) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(obj_dir), $(directories)) 2>NUL || cd .
	@echo Done.
else
	@mkdir -p $(obj_dir)
	@mkdir -p $(bin_dir)
	@mkdir -p $(addprefix $(obj_dir)/,$(directories))
endif


.PHONY: link
link: scaffold $(obj_files)
	@echo Linking $(assembly)
	@echo $(defines)
ifeq ($(build_platform),windows)
	@$(cc) $(obj_files) -o $(bin_dir)/$(assembly)$(extension) $(linker_flags) 
else
	@$(cc) $(obj_files) -o $(bin_dir)/lib$(assembly)$(extension) $(linker_flags) 
endif

.PHONY: compile
compile: #compile .c files
ifeq ($(build_platform),windows)
	@echo --- compiling shared libary $(assembly) for $(build_platform) ---
else
	@echo --- compiling shared libary $(assembly) for $(build_platform) ---
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
	@$(cc) $< $(compiler_flags) -c  -o $@ $(defines) $(include_flags) 
