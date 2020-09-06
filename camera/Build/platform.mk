# Add inputs and outputs from these tool invocations to the build variables

SYSROOT = $(SBI_SYSROOT)

#USR_INCS := $(addprefix -I "$(SYSROOT),$(PLATFORM_INCS_EX))
USR_INCS1 := $(addsuffix ",$(PLATFORM_INCS_EX))
USR_INCS := $(addprefix -I "$(SYSROOT),$(USR_INCS1))

ifeq ($(strip $(PLATFORM_LIB_PATHS)),)
RS_LIB_PATHS := "$(SYSROOT)/usr/lib"
else
RS_LIB_PATHS := $(addprefix -L,$(PLATFORM_LIB_PATHS))
endif

RS_LIBRARIES := $(addprefix -l,$(RS_LIBRARIES_EX))

PLATFORM_INCS = $(USR_INCS) -I "$(SDK_PATH)/library"
