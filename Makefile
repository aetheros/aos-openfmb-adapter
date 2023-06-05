
APP_VERSION=1.0

# must match manifest.json
EXE = aos_openfmb_adapter

ROOT_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

-include config.mk

APP_BUILD_DIR ?= .

all: $(EXE)

-include openfmb-shared.mk

CXXFLAGS += $(OPENFMB_CXXFLAGS)
LDFLAGS += $(OPENFMB_LDFLAGS)

CXXFLAGS += -std=c++14
CXXFLAGS += -MMD -MP

LIBS += -luuid -lsdk_m2m -lsdk_aos -lm2mgen -lxsd -lxsd_mtrsvc -lcoap -lcommon -lappfw $(OPENFMB_LIBS)

SRCS = $(wildcard src/*.cpp)

OBJS = $(SRCS:.cpp=.o)
OBJS += $(OPENFMB_GENERATED_SOURCES:.cc=.o)

DEPS = $(SRCS:.cpp=.d)
DEPS += $(OPENFMB_GENERATED_SOURCES:.cc=.d)

$(info OBJS=${OBJS})

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

-include $(DEPS)

clean:
	rm -f $(EXE) $(OBJS) $(DEPS) *.aos

cleanall: clean
	rm -Rf $(OPENFMB_SOURCE_DIR)
	rm -Rf $(PROTOBUF_SOURCE_DIR)

.PHONY: all clean cleanall pkg package

