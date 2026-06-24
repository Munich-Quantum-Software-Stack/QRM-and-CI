.PHONY: build invoke clean

build:
	@./scripts/build.sh

invoke:
	@./scripts/start.sh

clean:
	@./scripts/clean.sh