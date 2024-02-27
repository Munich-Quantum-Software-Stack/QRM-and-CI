INSTALL_PATH  ?= $(HOME)
EXEC_PATH     := $(INSTALL_PATH)/bin/lib
BUILD_DIR     ?= build
RABBITMQ      := $(wildcard /usr/local/lib/librabbitmq.so)
DOXYGEN       := $(shell command -v doxygen 2> /dev/null)

.PHONY: install clean uninstall docs test

all: install docs clean

ifdef RABBITMQ
build_rabbitmq:
	@echo "RabbitMQ is already installed. Skipping installation."
else
build_rabbitmq:
	@echo "Installing RabbitMQ."
	curl -LO \
		https://github.com/alanxz/rabbitmq-c/archive/refs/tags/v0.13.0.tar.gz
	tar -xf v0.13.0.tar.gz
	cmake \
        \
        -B rabbitmq-c-0.13.0/build \
        -DBUILD_EXAMPLES=OFF \
        -DENABLE_SSL_SUPPORT=OFF \
        -S rabbitmq-c-0.13.0
	if [ -n "$$CI" ]; then \
		cmake --build rabbitmq-c-0.13.0/build --target install; \
		ldconfig; \
	else \
		sudo cmake --build rabbitmq-c-0.13.0/build --target install; \
		sudo ldconfig; \
	fi
endif

ifndef CI
configure_rabbitmq:
	@hosts_file="/etc/hosts"; \
	hostname_entry="127.0.0.1 rabbitmq"; \
	if grep -qF "$$hostname_entry" "$$hosts_file"; then \
		echo "RabbitMQ is already configured in this system."; \
	else \
		echo "$$hostname_entry" | cat - "$$hosts_file" > temp && sudo mv -f temp "$$hosts_file"; \
	fi
else
configure_rabbitmq:
	@echo "Using rabbitmq as a service."
endif

dependencies_qrm: build_rabbitmq configure_rabbitmq

install: dependencies_qrm
	CMAKE_PREFIX_PATH=$$(llvm-config --libdir)/cmake/llvm cmake -B$(BUILD_DIR) \
		-DBUILD_WITH_DOCS=OFF \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PATH) && \
	cmake --build $(BUILD_DIR) --target install --config Release && \
	if [ -n "$$CI" ]; then \
		ldconfig; \
	else \
		sudo ldconfig; \
	fi

	@echo ""
	@echo "Please add $(INSTALL_PATH)/bin to the PATH environment variable:"
	@echo "export PATH=\$$PATH:$(INSTALL_PATH)/bin"
	@echo ""

clean:
	@rm -rf rabbitmq-c-0.13.0 v0.13.0.tar.gz doxygen

uninstall: clean
	@if [ -d "$(BUILD_DIR)" ]; then \
    	cd build && make uninstall && cd ..; \
    	rm -rf $(BUILD_DIR); \
	fi;
	@echo "Quantum Resource Manager uninstalled successfully"

ifdef DOXYGEN
build_docs:
	@echo "Doxygen is already installed. Skipping installation."
else
build_docs:
	@echo "Installing Doxygen."
	#git clone https://github.com/doxygen/doxygen.git
	cmake -B doxygen/build -G "Unix Makefiles" -S doxygen
	cmake --build doxygen/build
	$(MAKE) -C doxygen install
endif

docs: dependencies_qrm build_docs
	CMAKE_PREFIX_PATH=$$(llvm-config --libdir)/cmake/llvm cmake -B$(BUILD_DIR) \
		-DBUILD_WITH_DOCS=ON \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PATH) && \
	cmake --build $(BUILD_DIR) --target install --config Release && \
	if [ -n "$$CI" ]; then \
		ldconfig; \
	else \
		sudo ldconfig; \
	fi

	@echo ""
	@echo "Please add $(INSTALL_PATH)/bin to the PATH environment variable:"
	@echo "export PATH=\$$PATH:$(INSTALL_PATH)/bin"
	@echo ""

run: #install
	@if [ "$$(echo $$PATH | tr ':' '\n' | grep -c "$(INSTALL_PATH)/bin")" -eq 0 ]; then \
    	export PATH=$$PATH:$(INSTALL_PATH)/bin; \
	fi; \
	qresourcemanager_d screen

kill_daemons:
	#if [ ! -n "$$CI" ]; then \
	#	bash scripts/kill_daemons.sh; \
	#fi

test: #kill_daemons run
	cd build/ && \
    ctest -C Release -VV run_tests

pre-commit:
	pre-commit run --all-files

format:
	pre-commit run clang-format --all-files
