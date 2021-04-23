-include protobuf-target-shared.mk

OPENFMB_SOURCE_DIR ?= openfmb-protobuf

OPENFMB_PROTOBUF_SOURCES=$(OPENFMB_SOURCE_DIR)/proto/openfmb/coordinationservicemodule/coordinationservicemodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/loadmodule/loadmodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/regulatormodule/regulatormodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/reclosermodule/reclosermodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/metermodule/metermodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/resourcemodule/resourcemodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/generationmodule/generationmodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/uml.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/solarmodule/solarmodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/optimizermodule/optimizermodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/breakermodule/breakermodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/interconnectionmodule/interconnectionmodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/switchmodule/switchmodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/essmodule/essmodule.proto \
                 $(OPENFMB_SOURCE_DIR)/proto/openfmb/commonmodule/commonmodule.proto

OPENFMB_BUILD_DIR=$(OPENFMB_SOURCE_DIR)/generated
OPENFMB_PROTOBUF_INCLUDE=$(OPENFMB_SOURCE_DIR)/proto/openfmb

OPENFMB_SOURCES_RELOCATED=$(subst $(OPENFMB_PROTOBUF_INCLUDE),$(OPENFMB_BUILD_DIR),$(OPENFMB_PROTOBUF_SOURCES))

$(info OPENFMB_SOURCES_RELOCATED=${OPENFMB_SOURCES_RELOCATED})

OPENFMB_GENERATED_SOURCES=$(OPENFMB_SOURCES_RELOCATED:.proto=.pb.cc)
OPENFMB_GENERATED_HEADERS=$(OPENFMB_SOURCES_RELOCATED:.proto=.pb.h)

$(info OPENFMB_GENERATED_SOURCES=${OPENFMB_GENERATED_SOURCES})
$(info OPENFMB_GENERATED_HEADERS=${OPENFMB_GENERATED_HEADERS})

OPENFMB_LIBS += $(TARGET_PROTOBUF_LIBS)

OPENFMB_CXXFLAGS ?=
OPENFMB_CXXFLAGS += $(shell PKG_CONFIG_PATH=$(TARGET_PROTOBUF_PKG_CONFIG_PATH) PKG_CONFIG_SYSROOT_DIR='' pkg-config --cflags protobuf)
OPENFMB_CXXFLAGS += -I$(OPENFMB_BUILD_DIR)

$(info OPENFMB_CXXFLAGS=$(OPENFMB_CXXFLAGS))

OPENFMB_LDFLAGS += $(shell PKG_CONFIG_PATH=$(TARGET_PROTOBUF_PKG_CONFIG_PATH) PKG_CONFIG_SYSROOT_DIR='' pkg-config --libs protobuf)
$(info OPENFMB_LDFLAGS=$(OPENFMB_LDFLAGS))

