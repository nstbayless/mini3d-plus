ifeq ($(HEAP_SIZE),)
HEAP_SIZE      = 8388208
endif
ifeq ($(STACK_SIZE),)
STACK_SIZE     = 61800
endif

ifeq ($(PRODUCT),)
PRODUCT = 3DLibrary.pdx
endif

SELF_DIR := $(dir $(lastword $(MAKEFILE_LIST)))


SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

VPATH += $(SELF_DIR)/mini3d-plus

# List C source files here
SRC += \
	$(SELF_DIR)/main.c \
	$(SELF_DIR)/mini3d-plus/mini3d.c \
	$(SELF_DIR)/mini3d-plus/3dmath.c \
	$(SELF_DIR)/mini3d-plus/scene.c \
	$(SELF_DIR)/mini3d-plus/shape.c \
	$(SELF_DIR)/mini3d-plus/imposter.c \
	$(SELF_DIR)/mini3d-plus/render.c \
	$(SELF_DIR)/mini3d-plus/collision.c \
	$(SELF_DIR)/mini3d-plus/texture.c \
	$(SELF_DIR)/mini3d-plus/pattern.c \
	$(SELF_DIR)/mini3d-plus/image/miniz.c \
	$(SELF_DIR)/mini3d-plus/image/spng.c \
	$(SELF_DIR)/luaglue.c

# List all user directories here
UINCDIR += $(SELF_DIR)/mini3d-plus

# List all user C define here, like -D_DEBUG=1
UDEFS += -DPLAYDATE=1

# Define ASM defines here
UADEFS +=

# List the user directory to look for the libraries here
ULIBDIR +=

# List all user libraries here
ULIBS +=

#CLANGFLAGS = -fsanitize=address

ifneq ("$(wildcard $(SELF_DIR)/librif/src/)","")
SRC += \
	$(SELF_DIR)/librif/src/librif.c \
	$(SELF_DIR)/librif/src/playdate/librif_luaglue.c
CLANGFLAGS += -I$(SELF_DIR)/librif/src/ -DPLAYDATE=1
UDEFS += -DM3D_LIBRIF
endif

include $(SDK)/C_API/buildsupport/common.mk
