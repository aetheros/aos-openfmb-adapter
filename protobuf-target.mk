ROOT_DIR ?= $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

all: protobuf-target-all

-include protobuf-target-shared.mk

# libprotobuf

$(TARGET_PROTOBUF_BUILD_DIR):
	mkdir -p $@

$(TARGET_PROTOBUF_INSTALL_PREFIX):
	mkdir -p $@

clean:
	rm -Rf $(TARGET_PROTOBUF_BUILD_DIR)

$(TARGET_PROTOBUF_MAKEFILE): $(PROTOBUF_CONFIGURE) $(TARGET_PROTOBUF_BUILD_DIR) $(TARGET_PROTOBUF_INSTALL_PREFIX)
	cd $(TARGET_PROTOBUF_BUILD_DIR) ; \
	$(PROTOBUF_CONFIGURE) --host=x86_64 --prefix=$(abspath $(TARGET_PROTOBUF_INSTALL_PREFIX))

$(TARGET_PROTOBUF_LIB): $(TARGET_PROTOBUF_MAKEFILE)
	make -C $(TARGET_PROTOBUF_BUILD_DIR) install

protobuf-target-all: $(TARGET_PROTOBUF_LIB)

.PHONY: all clean protobuf-target-all
