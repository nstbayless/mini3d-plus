HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = 3DLibrary.pdx

SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
	SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

VPATH += mini3d-plus

# List C source files here
SRC = \
	main.c \
	mini3d-plus/mini3d.c \
	mini3d-plus/3dmath.c \
	mini3d-plus/scene.c \
	mini3d-plus/shape.c \
	mini3d-plus/imposter.c \
	mini3d-plus/render.c \
	mini3d-plus/collision.c \
	luaglue.c

ASRC = setup.s

# List all user directories here
UINCDIR = mini3d-plus

# List all user C define here, like -D_DEBUG=1
UDEFS =

# Define ASM defines here
UADEFS =

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS =

#CLANGFLAGS = -fsanitize=address

include $(SDK)/C_API/buildsupport/common.mk
