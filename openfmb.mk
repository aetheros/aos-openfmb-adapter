ROOT_DIR ?= $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

all: openfmb-all

-include protoc.mk

-include openfmb-shared.mk

OPENFMB_REPO ?= https://gitlab.com/openfmb/psm/ops/protobuf/openfmb-ops-protobuf.git

PROTOC ?= protoc

$(OPENFMB_GENERATED_SOURCES) $(OPENFMB_GENERATED_HEADERS): $(OPENFMB_PROTOBUF_SOURCES) $(PROTOC) $(OPENFMB_BUILD_DIR)
	$(PROTOC) --proto_path=$(OPENFMB_PROTOBUF_INCLUDE) --cpp_out=$(OPENFMB_BUILD_DIR) $(OPENFMB_PROTOBUF_SOURCES)

$(OPENFMB_SOURCE_DIR):
	#git submodule update --init $@
	git clone --branch=$(OPENFMB_VERSION) $(OPENFMB_REPO) $@

$(OPENFMB_PROTOBUF_SOURCES): $(OPENFMB_SOURCE_DIR)

$(OPENFMB_BUILD_DIR): $(OPENFMB_SOURCE_DIR)
	mkdir -p $@

openfmb-all: $(OPENFMB_GENERATED_SOURCES) $(OPENFMB_GENERATED_SOURCES)

openfmb-clean: protobuf-clean
	rm -Rf $(OPENFMB_BUILD_DIR)

clean: openfmb-clean

.PHONY: all clean openfmb-all openfmb-clean
