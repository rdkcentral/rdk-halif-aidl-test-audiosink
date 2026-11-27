#* ******************************************************************************
#*  If not stated otherwise in this file or this component's LICENSE
#*  file the following copyright and licenses apply:
#*
#*  Copyright 2025 RDK Management
#*
#*  Licensed under the Apache License, Version 2.0 (the License);
#*  you may not use this file except in compliance with the License.
#*  You may obtain a copy of the License at
#*
#*  http://www.apache.org/licenses/LICENSE-2.0
#*
#*  Unless required by applicable law or agreed to in writing, software
#*  distributed under the License is distributed on an AS IS BASIS,
#*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#*  See the License for the specific language governing permissions and
#*  limitations under the License.
#*
#* ******************************************************************************

.PHONY: all clean check-libs check-headers

MOCK_SERVICE_BIN := MockAudioSinkService

# BUILD_DIR is the top-level repo build/ directory. When invoked as
# `make -C vts_hal_test_components/audiosink/mockservice`, CURDIR is that
# mockservice/ directory, so ../../../build resolves to <repo-root>/build.
BUILD_DIR ?= $(CURDIR)/../../../build

# Headers/libraries come from the AIDL codegen output produced by
# ./build.sh (rdk-halif-aidl/out/...), not from any audiosink_gen/
# staging directory (that path is never created by the build).
RDK_HAL_DIR ?= $(BUILD_DIR)/../rdk-halif-aidl
HALIF_LIB_DIR ?= $(RDK_HAL_DIR)/out/target/lib/rdk-halif-aidl

# Header directories are versioned (e.g. .../audiosink/0.2.0.0/include)
# with no "current" alias, so pick whichever version directory build.sh
# produced.
AUDIOSINK_HEADERS ?= $(firstword $(wildcard $(RDK_HAL_DIR)/out/build/include/audiosink/*/include))
AUDIODECODER_HEADERS ?= $(firstword $(wildcard $(RDK_HAL_DIR)/out/build/include/audiodecoder/*/include))
AVBUFFER_HEADERS ?= $(firstword $(wildcard $(RDK_HAL_DIR)/out/build/include/avbuffer/*/include))
AVCLOCK_HEADERS ?= $(firstword $(wildcard $(RDK_HAL_DIR)/out/build/include/avclock/*/include))
VIDEODECODER_HEADERS ?= $(firstword $(wildcard $(RDK_HAL_DIR)/out/build/include/videodecoder/*/include))
COMMON_HEADERS ?= $(firstword $(wildcard $(RDK_HAL_DIR)/out/build/include/common/*/include))

MOCK_SERVICE_DIR ?= $(CURDIR)/src

MOCK_SERVICE_BIN_SRC := $(MOCK_SERVICE_DIR)/AudioSinkService.cpp
MOCK_SERVICE_BIN_SRC += $(MOCK_SERVICE_DIR)/AudioSink.cpp

MOCK_SERVICE_INC_FLAGS := -I$(AUDIODECODER_HEADERS)
MOCK_SERVICE_INC_FLAGS += -I$(AUDIOSINK_HEADERS)
MOCK_SERVICE_INC_FLAGS += -I$(AVBUFFER_HEADERS)
MOCK_SERVICE_INC_FLAGS += -I$(AVCLOCK_HEADERS)
MOCK_SERVICE_INC_FLAGS += -I$(VIDEODECODER_HEADERS)
MOCK_SERVICE_INC_FLAGS += -I$(COMMON_HEADERS)
MOCK_SERVICE_INC_FLAGS += -I$(BUILD_DIR)/usr/include

MOCK_SERVICE_LD_LIBS := -lbinder -lutils

MOCK_SERVICE_LDFLAGS := -L$(HALIF_LIB_DIR)
MOCK_SERVICE_LDFLAGS += -Wl,-rpath-link=$(HALIF_LIB_DIR)
MOCK_SERVICE_LDFLAGS += -L$(BUILD_DIR)/usr/lib
MOCK_SERVICE_LDFLAGS += -Wl,-rpath-link=$(BUILD_DIR)/usr/lib
MOCK_SERVICE_LDFLAGS += -Wl,--copy-dt-needed-entries
# -rpath-link only helps the linker resolve symbols at build time; it is not
# embedded in the binary. Embed a $ORIGIN-relative RUNPATH so the binary can
# find its .so dependencies at runtime as long as they are copied alongside
# it (portable across build host and any deployed target, e.g. QEMU).
MOCK_SERVICE_LDFLAGS += -Wl,-rpath,'$$ORIGIN' -Wl,--disable-new-dtags

MOCK_SERVICE_CFLAGS := -Wall -Wextra -std=c++17 -ggdb

MOCK_SERVICE_BIN := $(BUILD_DIR)/bin/$(MOCK_SERVICE_BIN)

all: $(MOCK_SERVICE_BIN)

check-headers:
	@echo "Checking required header directories..."
	@test -d "$(AUDIODECODER_HEADERS)" || { echo "ERROR: missing audiodecoder headers under $(RDK_HAL_DIR)/out/build/include/audiodecoder/*/include - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -d "$(AUDIOSINK_HEADERS)" || { echo "ERROR: missing audiosink headers under $(RDK_HAL_DIR)/out/build/include/audiosink/*/include - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -d "$(AVBUFFER_HEADERS)" || { echo "ERROR: missing avbuffer headers under $(RDK_HAL_DIR)/out/build/include/avbuffer/*/include - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -d "$(VIDEODECODER_HEADERS)" || { echo "ERROR: missing videodecoder headers under $(RDK_HAL_DIR)/out/build/include/videodecoder/*/include - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -d "$(COMMON_HEADERS)" || { echo "ERROR: missing common headers under $(RDK_HAL_DIR)/out/build/include/common/*/include - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@echo "Header check passed"

check-libs:
	@echo "Checking required libraries..."
	@test -e "$(HALIF_LIB_DIR)/libaudiosink-cpp_vts.so" || { echo "ERROR: missing $(HALIF_LIB_DIR)/libaudiosink-cpp_vts.so - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -e "$(HALIF_LIB_DIR)/libaudiodecoder-cpp_vts.so" || { echo "ERROR: missing $(HALIF_LIB_DIR)/libaudiodecoder-cpp_vts.so - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -e "$(HALIF_LIB_DIR)/libavclock-cpp_vts.so" || { echo "ERROR: missing $(HALIF_LIB_DIR)/libavclock-cpp_vts.so - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -e "$(HALIF_LIB_DIR)/libavbuffer-cpp_vts.so" || { echo "ERROR: missing $(HALIF_LIB_DIR)/libavbuffer-cpp_vts.so - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -e "$(HALIF_LIB_DIR)/libcommon-cpp_vts.so" || { echo "ERROR: missing $(HALIF_LIB_DIR)/libcommon-cpp_vts.so - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -e "$(BUILD_DIR)/usr/lib/libbinder.so" || { echo "ERROR: missing $(BUILD_DIR)/usr/lib/libbinder.so - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@test -e "$(BUILD_DIR)/usr/lib/libutils.so" || { echo "ERROR: missing $(BUILD_DIR)/usr/lib/libutils.so - run ./build.sh Target=linux (or arm) first"; exit 1; }
	@echo "Library check passed"

$(MOCK_SERVICE_BIN): check-headers check-libs $(MOCK_SERVICE_BIN_SRC:%.cpp=%.o)
	@mkdir -p $(dir $@)
	g++ $(MOCK_SERVICE_LDFLAGS) \
		$(MOCK_SERVICE_BIN_SRC:%.cpp=%.o) -o $@ \
		$(MOCK_SERVICE_LD_LIBS) \
		-lpthread \
		-laudiosink-cpp_vts \
		-laudiodecoder-cpp_vts \
		-lavclock-cpp_vts \
		-lavbuffer-cpp_vts \
		-lcommon-cpp_vts \
		-lavbufferhelper

%.o: %.cpp
	g++ $(MOCK_SERVICE_CFLAGS) $(MOCK_SERVICE_INC_FLAGS) -fPIC -c $< -o $@

clean:
	rm -f $(MOCK_SERVICE_BIN_SRC:%.cpp=%.o)
	rm -f $(MOCK_SERVICE_BIN)
