INSTALL_PATH ?= $(HOME)
BUILD_DIR    ?= build
OS           := $(shell uname)

.PHONY: install uninstall test

all: install

install:
	@if [ "$(OS)" = "Darwin" ]; then \
		echo "-- Building the QRM for macOS"; \
		cmake -B$(BUILD_DIR) \
			-DBUILD_WITH_DOCS=OFF \
			-DCMAKE_INSTALL_PREFIX=$(INSTALL_PATH) && \
		echo "-- Installing the QRM for macOS"; \
		cmake --build $(BUILD_DIR) --target install --config Release -v; \
    else \
		echo "-- Building the QRM for Linux"; \
		CMAKE_PREFIX_PATH=$$(llvm-config --libdir)/cmake/llvm cmake -B$(BUILD_DIR) \
			-DBUILD_WITH_DOCS=OFF \
			-DCMAKE_INSTALL_PREFIX=$(INSTALL_PATH) && \
		echo "-- Installing the QRM for Linux"; \
		cmake --build $(BUILD_DIR) --target install --config Release; \
		if [ -n "$$CI" ]; then \
			ldconfig; \
		else \
			sudo ldconfig; \
		fi; \
	fi

	@echo ""
	@echo "-- Please add $(INSTALL_PATH)/bin to the PATH environment variable:"
	@echo "-- export PATH=\$$PATH:$(INSTALL_PATH)/bin"
	@echo ""

uninstall:
	@if [ -d "$(BUILD_DIR)" ]; then \
    	cd build && make uninstall && cd ..; \
    	rm -rf $(BUILD_DIR); \
	fi;
	@echo "-- QRM uninstalled successfully"

test:
	cd build/ && \
    ctest -C Release -VV run_tests

pre-commit:
	pre-commit run --all-files

format:
	pre-commit run clang-format --all-files
