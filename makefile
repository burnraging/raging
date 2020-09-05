


###################################################################################################
#	Variables:
###################################################################################################

BUILD_DIR=build
TEST_DIR=$(BUILD_DIR)/tests

#	Disco Directories
DISCO_DIR=$(BUILD_DIR)/disco
DISCO_DEBUG_DIR=$(DISCO_DIR)/debug
DISCO_SIZE_DIR=$(DISCO_DIR)/size
DISCO_SPEED_DIR=$(DISCO_DIR)/speed

#	Qemu Directories
QEMU_DIR=$(BUILD_DIR)/qemu
QEMU_DEBUG_DIR=$(QEMU_DIR)/debug
QEMU_SIZE_DIR=$(QEMU_DIR)/size
QEMU_SPEED_DIR=$(QEMU_DIR)/speed

#	Simple Directories
SMPL_DIR=$(BUILD_DIR)/msp430-simple
SMPL_DEBUG_DIR=$(SMPL_DIR)/debug
SMPL_SIZE_DIR=$(SMPL_DIR)/size
SMPL_SPEED_DIR=$(SMPL_DIR)/speed

#	Tool Directories
SIM_DIR=$(BUILD_DIR)/sim/debug
SANITY_DIR=$(BUILD_DIR)/ssp-sanity/debug
GATEWAY_DIR=$(BUILD_DIR)/gateway/debug


ARM_TOOLCHAIN=-D "CMAKE_TOOLCHAIN_FILE=CMake/GNU-ARM-Toolchain.cmake"
MSP_TOOLCHAIN=-D "CMAKE_TOOLCHAIN_FILE=CMake/GNU-MSP430-Toolchain.cmake"

DEBUG_BUILD=-DCMAKE_BUILD_TYPE=Debug
SIZE_BUILD=-DCMAKE_BUILD_TYPE=Size
SPEED_BUILD=-DCMAKE_BUILD_TYPE=Speed

CMAKE_LIST=CMakeLists.txt
DISCO_CMAKE_LIST=CMakeTargets/DISCO_CMakeLists.txt
QEMU_CMAKE_LIST=CMakeTargets/QEMU_CMakeLists.txt
SMPL_CMAKE_LIST=CMakeTargets/MSP430SIMPLE_CMakeLists.txt
SIM_CMAKE_LIST=CMakeTargets/SIM_CMakeLists.txt
UT_CMAKE_LIST=CMakeTargets/UNIT_TEST_CMakeLists.txt
SANITY_CMAKE_LIST=CMakeTargets/SSP-SANITY_CMakeLists.txt
GATEWAY_CMAKE_LIST=CMakeTargets/GATEWAY_CMakeLists.txt

CMAKE=cmake -GNinja

NINJA=ninja
NINJA_NORMAL=$(NINJA) bin size list
NINJA_NORMAL_HEX=$(NINJA_NORMAL) hex
NINJA_DUMP=$(NINJA_NORMAL_HEX) dump
NINJA_VERBOSE=$(NINJA) -v bin hex size list dump

CCT=gcovr
CCTFLAGS=-r . --html -o coverage.html --html-details #--gcov-filter='.*nufr-kernel'
RUN_TESTS=./unit_tests.elf

###################################################################################################
#
#	Targets
#
#
###################################################################################################

testrun:
	@mkdir -p $(TEST_DIR)
	@cp $(UT_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(TEST_DIR) && $(CMAKE) ../../
	@cd $(TEST_DIR) && $(NINJA)
	@rm $(CMAKE_LIST)
	@cd $(TEST_DIR) && $(RUN_TESTS)
	@cd $(TEST_DIR) && $(CCT) $(CCTFLAGS)
	@cd $(TEST_DIR) && xdg-open coverage.html


###################################################################################################
#	Disco Targets
###################################################################################################

disco:
	@mkdir -p $(DISCO_DEBUG_DIR)
	@cp $(DISCO_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(DISCO_DEBUG_DIR) && $(CMAKE) $(DEBUG_BUILD) $(ARM_TOOLCHAIN) ../../../
	@cd $(DISCO_DEBUG_DIR) && $(NINJA_DUMP)
	@rm $(CMAKE_LIST)

verbose-disco:
	@mkdir -p $(DISCO_DEBUG_DIR)
	@cp $(DISCO_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(DISCO_DEBUG_DIR) && $(CMAKE) $(DEBUG_BUILD) $(ARM_TOOLCHAIN) ../../../
	@cd $(DISCO_DEBUG_DIR) && $(NINJA_VERBOSE)
	@rm $(CMAKE_LIST)

size-disco:
	@mkdir -p $(DISCO_SIZE_DIR)
	@cp $(DISCO_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(DISCO_SIZE_DIR) && $(CMAKE) $(SIZE_BUILD) $(ARM_TOOLCHAIN) ../../../
	@cd $(DISCO_SIZE_DIR) && $(NINJA_NORMAL)
	@rm $(CMAKE_LIST)

speed-disco:
	@mkdir -p $(DISCO_SPEED_DIR)
	@cp $(DISCO_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(DISCO_SPEED_DIR) && $(CMAKE) $(SPEED_BUILD) $(ARM_TOOLCHAIN) ../../../
	@cd $(DISCO_SPEED_DIR) && $(NINJA_NORMAL)
	@rm $(CMAKE_LIST)

all-disco: disco size-disco speed-disco

###################################################################################################
#	Qemu Targets
###################################################################################################

qemu:
	@mkdir -p $(QEMU_DEBUG_DIR)
	@cp $(QEMU_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(QEMU_DEBUG_DIR) && $(CMAKE) $(DEBUG_BUILD) $(ARM_TOOLCHAIN) ../../../
	@cd $(QEMU_DEBUG_DIR) && $(NINJA_DUMP)
	@rm $(CMAKE_LIST)

verbose-qemu:
	@mkdir -p $(QEMU_DEBUG_DIR)
	@cp $(QEMU_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(QEMU_DEBUG_DIR) && $(CMAKE) $(ARM_TOOLCHAIN) ../../../
	@cd $(QEMU_DEBUG_DIR) && $(NINJA_VERBOSE)
	@rm $(CMAKE_LIST)

size-qemu:
	@mkdir -p $(QEMU_SIZE_DIR)
	@cp $(QEMU_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(QEMU_SIZE_DIR) && $(CMAKE) $(SIZE_BUILD) $(ARM_TOOLCHAIN) ../../../
	@cd $(QEMU_SIZE_DIR) && $(NINJA_NORMAL)
	@rm $(CMAKE_LIST)

speed-qemu:
	@mkdir -p $(QEMU_SPEED_DIR)
	@cp $(QEMU_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(QEMU_SPEED_DIR) && $(CMAKE) $(SPEED_BUILD) $(ARM_TOOLCHAIN) ../../../
	@cd $(QEMU_SPEED_DIR) && $(NINJA_NORMAL)
	@rm $(CMAKE_LIST)

all-qemu: qemu size-qemu speed-qemu

###################################################################################################
#	Simple Targets
###################################################################################################

msp430-simple:
	@mkdir -p $(SMPL_DEBUG_DIR)
	@cp $(SMPL_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SMPL_DEBUG_DIR) && $(CMAKE) $(DEBUG_BUILD) $(MSP_TOOLCHAIN) ../../../
	@cd $(SMPL_DEBUG_DIR) && $(NINJA_DUMP)
	@rm $(CMAKE_LIST)

verbose-msp430-simple:
	@mkdir -p $(SMPL_DEBUG_DIR)
	@cp $(SMPL_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SMPL_DEBUG_DIR) && $(CMAKE) $(DEBUG_BUILD) $(MSP_TOOLCHAIN) ../../../
	@cd $(SMPL_DEBUG_DIR) && $(NINJA_VERBOSE)
	@rm $(CMAKE_LIST)

size-msp430-simple:
	@mkdir -p $(SMPL_SIZE_DIR)
	@cp $(SMPL_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SMPL_SIZE_DIR) && $(CMAKE) $(SIZE_BUILD) $(MSP_TOOLCHAIN) ../../../
	@cd $(SMPL_SIZE_DIR) && $(NINJA_NORMAL)
	@rm $(CMAKE_LIST)

speed-msp430-simple:
	@mkdir -p $(SMPL_SPEED_DIR)
	@cp $(SMPL_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SMPL_SPEED_DIR) && $(CMAKE) $(SPEED_BUILD) $(MSP_TOOLCHAIN) ../../../
	@cd $(SMPL_SPEED_DIR) && $(NINJA_NORMAL)
	@rm $(CMAKE_LIST)

all-msp430-simple: msp430-simple size-msp430-simple speed-msp430-simple

###################################################################################################
#	Sim Targets
###################################################################################################

sim:
	@mkdir -p $(SIM_DIR)
	@cp $(SIM_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SIM_DIR) && $(CMAKE) ../../../
	@cd $(SIM_DIR) && $(NINJA)
	@rm $(CMAKE_LIST)


verbose-sim:
	@mkdir -p $(SIM_DIR)
	@cp $(SIM_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SIM_DIR) && $(CMAKE) ../../../
	@cd $(SIM_DIR) && $(NINJA_VERBOSE)
	@rm $(CMAKE_LIST)

###################################################################################################
#	Sanity Tool Targets
###################################################################################################

ssp-sanity:
	@mkdir -p $(SANITY_DIR)
	@cp $(SANITY_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SANITY_DIR) && $(CMAKE) ../../../
	@cd $(SANITY_DIR) && $(NINJA)
	@rm $(CMAKE_LIST)

verbose-ssp-sanity:
	@mkdir -p $(SANITY_DIR)
	@cp $(SANITY_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(SANITY_DIR) && $(CMAKE) ../../../
	@cd $(SANITY_DIR) && $(NINJA_VERBOSE)
	@rm $(CMAKE_LIST)

###################################################################################################
#	Gateway Tool Targets
###################################################################################################

gateway:
	@mkdir -p $(GATEWAY_DIR)
	@cp $(GATEWAY_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(GATEWAY_DIR) && $(CMAKE) ../../../
	@cd $(GATEWAY_DIR) && $(NINJA)
	@rm $(CMAKE_LIST)

verbose-gateway:
	@mkdir -p $(GATEWAY_DIR)
	@cp $(GATEWAY_CMAKE_LIST) $(CMAKE_LIST)
	@cd $(GATEWAY_DIR) && $(CMAKE) ../../../
	@cd $(GATEWAY_DIR) && $(NINJA_VERBOSE)
	@rm $(CMAKE_LIST)

###################################################################################################
#	PHONY Targets
###################################################################################################

.PHONY: all
all:	clean all-disco all-qemu all-msp430-simple sim testrun
#1Jan2020: sspsanitytool and gatewaytool are TBD, removing them to allow testrun to build
#all:	clean all-disco all-qemu all-msp430-simple sim sspsanitytool gatewaytool testrun

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
