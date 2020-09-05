
###############################################################################
#
#   Makefile includes
#
###############################################################################

include ../../makefiles/common.mk

###############################################################################
#
#   Build Directories
#
###############################################################################

BUILD_DIR = ../../build
SAN_TGT_DIR = $(BUILD_DIR)/tools/sanity
GAT_TGT_DIR = $(BUILD_DIR)/tools/gateway

SAN_BUILD_DEBUG_DIR = $(SAN_TGT_DIR)/debug
GAT_BUILD_DEBUG_DIR = $(GAT_TGT_DIR)/debug

###############################################################################
#
#   Binary Names
#
###############################################################################

SAN_APPNAME = ssp-sanity
GAT_APPNAME = ssp-gateway

###############################################################################
#
#   Include Directories
#
###############################################################################

INCLUDES +=-I"../../includes"
INCLUDES +=-I"../../tools/linux"


###############################################################################
#
#   Build paths
#
###############################################################################

SRC=../../sources
T_SRC=../../tools/linux

###############################################################################
#
#   Sources
#
###############################################################################

#   Raging Utilities
SAN_SRCS += $(SAN_TGT_DIR)/raging-utils.c
SAN_SRCS += $(SAN_TGT_DIR)/raging-utils-mem.c
SAN_SRCS += $(SAN_TGT_DIR)/raging-utils-crc.c
SAN_SRCS += $(SAN_TGT_DIR)/raging-utils-scan-print.c

#  SSP-Sanity 
SAN_SRCS += $(SAN_TGT_DIR)/ssp-framer.cpp
SAN_SRCS += $(SAN_TGT_DIR)/ssp-packet.cpp
SAN_SRCS += $(SAN_TGT_DIR)/ssp-sanity.cpp
SAN_SRCS += $(SAN_TGT_DIR)/linux-utils.cpp

SAN_OBJECTS += $(SAN_SRCS:%.c=%.o)
SAN_OBJECTS += $(SAN_SRCS:%.cpp=%.o)


#   Raging Utilities
GAT_SRCS += $(GAT_TGT_DIR)/raging-utils.c
GAT_SRCS += $(GAT_TGT_DIR)/raging-utils-crc.c
GAT_SRCS += $(GAT_TGT_DIR)/raging-utils-scan-print.c

#  SSP-gateway
GAT_SRCS += $(GAT_TGT_DIR)/ssp-framer.cpp
GAT_SRCS += $(GAT_TGT_DIR)/ssp-gateway.cpp
GAT_SRCS += $(GAT_TGT_DIR)/ssp-packet.cpp

GAT_OBJECTS += $(GAT_SRCS:%.c=%.o)
GAT_OBJECTS += $(GAT_SRCS:%.cpp=%.o)
###############################################################################
#
#   Over-ridden Tool definitions
#
###############################################################################


###############################################################################
#
#   Targets
#
###############################################################################

debug: all
	
	
all: sanity gateway 
	

sanity: Directories Copies $(SAN_OBJECTS)
	@echo ==================================================
	@echo [ LD ] Linking Debug $(SAN_APPNAME)
	@$(RM) $(SAN_BUILD_DEBUG_DIR)/ssp-gateway.o
	@$(NAT_LD) $(NAT_LD_FLAGS) -pthread -o $(SAN_BUILD_DEBUG_DIR)/$(SAN_APPNAME) $(SAN_BUILD_DEBUG_DIR)/*.o
	@echo ==================================================

gateway: Directories Copies $(GAT_OBJECTS)
	@echo ==================================================
	@echo [ LD ] Linking Debug $(GAT_APPNAME)
	@$(RM) $(GAT_BUILD_DEBUG_DIR)/ssp-sanity.o
	@$(NAT_LD) $(NAT_LD_FLAGS) -pthread -o $(GAT_BUILD_DEBUG_DIR)/$(GAT_APPNAME) $(GAT_BUILD_DEBUG_DIR)/*.o
	@echo ==================================================
	
clean:
	@$(RM) $(SAN_TGT_DIR)
	@$(RM) $(GAT_TGT_DIR)

.PHONY: Directories
Directories: 
	@echo ==================================================
	@echo [ !! ] Checking Directories
	@$(MKDIR) $(SAN_BUILD_DEBUG_DIR)
	@$(MKDIR) $(GAT_BUILD_DEBUG_DIR)
	
.PHONY: Copies
Copies:
	@echo [ !! ] Copying files
	@cp $(SRC)/*.c $(SAN_TGT_DIR)
	@cp $(SRC)/*.c $(GAT_TGT_DIR)
	@cp $(T_SRC)/*.cpp $(SAN_TGT_DIR)	
	@cp $(T_SRC)/*.cpp $(GAT_TGT_DIR)
	

%.o: %.c		
	@$(NAT_CC) $(NAT_CC_FLAGS) $(INCLUDES) -o $(SAN_BUILD_DEBUG_DIR)/$(@F) $<		
	@echo [ CC ] $(<F)
	@$(NAT_CC) $(NAT_CC_FLAGS) $(INCLUDES) -o $(GAT_BUILD_DEBUG_DIR)/$(@F) $<		
	@echo [ CC ] $(<F)	

%.o: %.cpp		
	@$(NAT_CPP) $(NAT_CPP_FLAGS) $(INCLUDES) -o $(SAN_BUILD_DEBUG_DIR)/$(@F) $<		
	@echo [ C++] $(<F)
	@$(NAT_CPP) $(NAT_CPP_FLAGS) $(INCLUDES) -o $(GAT_BUILD_DEBUG_DIR)/$(@F) $<		
	@echo [ C++] $(<F)



	
