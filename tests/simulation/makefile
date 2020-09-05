include ../../makefiles/common.mk

###############################################################################
#
#   Build Directories
#
###############################################################################

BUILD_DIR = ../../build
TGT_DIR = $(BUILD_DIR)/sim

BUILD_DEBUG_DIR = $(TGT_DIR)/debug
BUILD_SIZE_DIR = $(TGT_DIR)/size
BUILD_FAST_DIR = $(TGT_DIR)/fast

###############################################################################
#
#   Include Directories
#
###############################################################################

INCLUDES +=-I"includes"
INCLUDES +=-I"platform/tests/qemu"
INCLUDES +=-I"nufr-platform/small-soc"
INCLUDES +=-I"tests/qemu"


all:
	@$(MKDIR) $(BUILD_DEBUG_DIR)


	@$(MKDIR) $(BUILD_SIZE_DIR)
	

	@$(MKDIR) $(BUILD_FAST_DIR)


clean:
	@$(RM) $(TGT_DIR)
