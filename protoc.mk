ROOT_DIR ?= $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

all: protoc

-include protobuf-target-shared.mk

PROTOC_BUILD_EXECUTABLE = $(PROTOC_INSTALL_DIR)/bin/protoc
PROTOC_MAKEFILE ?= $(PROTOC_BUILD_DIR)/Makefile

$(PROTOC_BUILD_DIR): $(PROTOBUF_SOURCE_DIR)
	mkdir -p $@

$(PROTOC_INSTALL_DIR): $(PROTOBUF_SOURCE_DIR)
	mkdir -p $@

$(PROTOC_MAKEFILE): $(PROTOBUF_CONFIGURE) $(PROTOC_BUILD_DIR) $(PROTOC_INSTALL_DIR)
	cd $(PROTOC_BUILD_DIR) ; \
	$(PROTOBUF_CONFIGURE) --host=x86_64 --prefix=$(abspath $(PROTOC_INSTALL_DIR))

$(PROTOC_BUILD_EXECUTABLE): $(PROTOC_MAKEFILE) $(PROTOC_INSTALL_DIR)
	make -C $(PROTOC_BUILD_DIR) install

protoc: $(PROTOC_BUILD_EXECUTABLE)

protobuf-clean: protoc-clean

protoc-clean:
	rm -

target-protobuf-clean:

PROTOC ?= $(PROTOC_BUILD_EXECUTABLE)

# libprotobuf

$(PROTOC_BUILD_DIR):
	mkdir -p $@

protobuf-clean:
	rm -Rf $(PROTOC_BUILD_DIR)

clean: protobuf-clean

.PHONY: all clean protoc protoc-clean
