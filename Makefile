# User can set VERBOSE variable to have all commands echoed to console for debugging purposes.
ifdef VERBOSE
    Q :=
else
    Q := @
endif


#######################################
#  Forwards Declaration of Main Rules
#######################################
.PHONY : all clean

all:
clean:


#  Names of tools for compiling binaries to run on this host system.
HOST_GCC := gcc
HOST_GPP := g++
HOST_AS  := gcc
HOST_LD  := g++
HOST_AR  := ar
QUIET    := > /dev/null 2>&1 ; exit 0
REMOVE   := rm
REMOVE_DIR := rm -r -f
MAKEDIR  = mkdir -p $(dir $@)

# Flags to use when compiling binaries to run on this host system.
HOST_GCCFLAGS := -O0 -g3 -Wall -Wextra -Werror -Wno-unused-parameter -MMD -MP
HOST_GCCFLAGS += -ffunction-sections -fdata-sections -fno-common
HOST_GPPFLAGS := $(HOST_GCCFLAGS)
HOST_GCCFLAGS += -std=gnu90
HOST_ASFLAGS  := -g -x assembler-with-cpp -MMD -MP

# Output directories for intermediate object files.
OBJDIR        := obj
HOST_OBJDIR   := $(OBJDIR)

# Start out with empty pre-req lists.  Add modules as we go.
ALL_TARGETS  :=

# Start out with an empty header file dependency list.  Add module files as we go.
DEPS :=

# Useful macros.
objs = $(addprefix $2/,$(addsuffix .o,$(basename $(wildcard $1/*.c $1/*.cpp $1/*.S))))
host_objs = $(call objs,$1,$(HOST_OBJDIR))
add_deps = $(patsubst %.o,%.d,$(HOST_$1_OBJ) $(ARMV7M_$1_OBJ) $(GCOV_HOST_$1_OBJ))
includes = $(patsubst %,-I%,$1)
define link_exe
	@echo Building $@
	$Q $(MAKEDIR)
	$Q $($1_LD) $($1_LDFLAGS) $^ -o $@
endef
define host_make_app # ,APP2BUILD,app_src_dirs,includes,other_libs
    HOST_$1_APP_OBJ        := $(foreach i,$2,$(call host_objs,$i))
    HOST_$1_APP_EXE        := $1
    DEPS                   += $$(call add_deps,$1_APP)
    ALL_TARGETS += $$(HOST_$1_APP_EXE)
    $$(HOST_$1_APP_EXE) : INCLUDES := $3
    $$(HOST_$1_APP_EXE) : $$(HOST_$1_APP_OBJ) $4
		$$(call link_exe,HOST)
endef


#######################################
# repro-129 Executable
$(eval $(call host_make_app,repro-129,Mac-src,,))



#######################################
#  Actual Definition of Main Rules
#######################################
all : $(ALL_TARGETS)

clean :
	@echo Cleaning repro-129
	$Q $(REMOVE_DIR) $(OBJDIR) $(QUIET)
	$Q $(REMOVE) $(ALL_TARGETS) $(QUIET)


# *** Pattern Rules ***
$(HOST_OBJDIR)/%.o : %.c
	@echo Compiling $<
	$Q $(MAKEDIR)
	$Q $(EXTRA_COMPILE_STEP)
	$Q $(HOST_GCC) $(HOST_GCCFLAGS) $(call includes,$(INCLUDES)) -c $< -o $@


# *** Pull in header dependencies if not performing a clean build. ***
ifneq "$(findstring clean,$(MAKECMDGOALS))" "clean"
    -include $(DEPS)
endif
