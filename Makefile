ROOT        := $(abspath .)
ENGINE_DIR  := $(ROOT)/engine
BUILD       := $(ROOT)/build
WEB_DIR     := $(ROOT)/web
PORT        ?= 8080

.PHONY: all deps build cli server web demo test run serve clean help

all: build

help:
	@echo "CodeTrust targets:"
	@echo "  make deps     Pull gcc:14 image and install web deps"
	@echo "  make build    Build C++ binaries + production UI"
	@echo "  make cli      Build CLI only"
	@echo "  make server   Build server only"
	@echo "  make demo     Run the add sample through the CLI"
	@echo "  make test     Run engine self-tests (needs Docker)"
	@echo "  make run      Build then start API+UI on port $(PORT)"
	@echo "  make serve    Start server only (no rebuild)"
	@echo "  make clean    Remove build artifacts"

deps:
	docker pull gcc:14
	docker pull python:3.12-slim
	docker pull eclipse-temurin:17-jdk
	docker pull node:20-slim
	docker pull golang:1.22-alpine
	@if [ -f "$(WEB_DIR)/package.json" ]; then \
		cd "$(WEB_DIR)" && npm install; \
	fi

cli:
	$(MAKE) -C "$(ENGINE_DIR)" cli

server:
	$(MAKE) -C "$(ENGINE_DIR)" server

web:
	cd "$(WEB_DIR)" && npm install && npm run build

build: cli server
	@if [ -f "$(WEB_DIR)/package.json" ]; then \
		$(MAKE) web; \
	fi

demo: cli
	"$(BUILD)/codetrust-cli" \
		--root "$(ROOT)" \
		--source "$(ROOT)/samples/add/submission.cpp" \
		--tests "$(ROOT)/samples/add/testcases.txt" \
		--label "demo-add"

test: cli
	@echo "== compile-ok / all pass =="
	"$(BUILD)/codetrust-cli" --root "$(ROOT)" \
		--source "$(ROOT)/samples/add/submission.cpp" \
		--tests "$(ROOT)/samples/add/testcases.txt" \
		--label "selftest-pass"
	@echo "== compile error =="
	@set +e; \
	"$(BUILD)/codetrust-cli" --root "$(ROOT)" \
		--source "$(ROOT)/tests/fixtures/bad_compile.cpp" \
		--mode compile_only \
		--label "selftest-compile"; \
	status=$$?; \
	if [ $$status -eq 0 ]; then echo "Expected compile failure"; exit 1; fi
	@echo "== wrong output =="
	@set +e; \
	"$(BUILD)/codetrust-cli" --root "$(ROOT)" \
		--source "$(ROOT)/tests/fixtures/wrong_output.cpp" \
		--tests "$(ROOT)/tests/fixtures/add_tests.txt" \
		--label "selftest-wrong"; \
	status=$$?; \
	if [ $$status -eq 0 ]; then echo "Expected test failure"; exit 1; fi
	@echo "== timeout =="
	@set +e; \
	"$(BUILD)/codetrust-cli" --root "$(ROOT)" \
		--source "$(ROOT)/tests/fixtures/timeout.cpp" \
		--tests "$(ROOT)/tests/fixtures/add_tests.txt" \
		--timeout 1 \
		--label "selftest-timeout"; \
	status=$$?; \
	if [ $$status -eq 0 ]; then echo "Expected timeout failure"; exit 1; fi
	@echo "All self-tests behaved as expected."

run: build
	@set -a; \
	if [ -f "$(ROOT)/.env" ]; then . "$(ROOT)/.env"; fi; \
	set +a; \
	"$(BUILD)/codetrust-server" --root "$(ROOT)" --port $(PORT)

serve: server
	@set -a; \
	if [ -f "$(ROOT)/.env" ]; then . "$(ROOT)/.env"; fi; \
	set +a; \
	"$(BUILD)/codetrust-server" --root "$(ROOT)" --port $(PORT)

clean:
	$(MAKE) -C "$(ENGINE_DIR)" clean
	rm -rf "$(WEB_DIR)/dist" "$(WEB_DIR)/node_modules"
	rm -rf "$(ROOT)/data/jobs"
