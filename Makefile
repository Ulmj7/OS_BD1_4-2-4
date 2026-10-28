CC ?= cc
AR ?= ar
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Isrc/shim -Isrc/engine -Isrc/kernel
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -Werror
BUILD := build
CORE := $(BUILD)/kshim.o $(BUILD)/kds_core.o
SAN_BUILD := $(BUILD)/sanitized
SAN_CORE := $(SAN_BUILD)/kshim.o $(SAN_BUILD)/kds_core.o
SAN_ENGINE := $(SAN_BUILD)/kds_engine
TEST_SOURCES := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/tests/%,$(TEST_SOURCES))
SAN_FLAGS := $(shell printf 'int main(void){return 0;}\n' | $(CC) -x c - -fsanitize=address,undefined -o /tmp/kds-sanitize-$$$$ 2>/dev/null && rm -f /tmp/kds-sanitize-$$$$ && printf '%s' '-fsanitize=address,undefined -fno-omit-frame-pointer')
KDIR ?= /lib/modules/$(shell uname -r)/build
.PHONY: all test run module clean FORCE
all: $(BUILD)/kds_engine $(BUILD)/libkds.a
$(BUILD) $(SAN_BUILD):
	mkdir -p $@
$(BUILD)/.flags: FORCE | $(BUILD)
	@printf '%s\n' '$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp
$(SAN_BUILD)/.flags: FORCE | $(SAN_BUILD)
	@printf '%s\n' '$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) $(LDFLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@rm -f $@.tmp
FORCE:
$(BUILD)/kshim.o: src/shim/kshim.c src/shim/kshim.h $(BUILD)/.flags | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/kds_core.o: src/engine/kds_core.c src/engine/kds_core.h $(BUILD)/.flags | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/main.o: src/engine/main.c src/engine/kds_core.h $(BUILD)/.flags | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/libkds.a: $(CORE)
	$(AR) rcs $@ $^
$(BUILD)/kds_engine: $(BUILD)/main.o $(BUILD)/libkds.a
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
$(SAN_BUILD)/kshim.o: src/shim/kshim.c src/shim/kshim.h $(SAN_BUILD)/.flags | $(SAN_BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) -MMD -MP -c $< -o $@
$(SAN_BUILD)/kds_core.o: src/engine/kds_core.c src/engine/kds_core.h $(SAN_BUILD)/.flags | $(SAN_BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) -MMD -MP -c $< -o $@
$(SAN_BUILD)/main.o: src/engine/main.c src/engine/kds_core.h $(SAN_BUILD)/.flags | $(SAN_BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) -MMD -MP -c $< -o $@
$(SAN_BUILD)/libkds.a: $(SAN_CORE)
	$(AR) rcs $@ $^
$(SAN_ENGINE): $(SAN_BUILD)/main.o $(SAN_BUILD)/libkds.a
	$(CC) $(CFLAGS) $(SAN_FLAGS) $^ $(LDFLAGS) -o $@
$(BUILD)/tests/%: tests/%.c $(SAN_BUILD)/libkds.a $(SAN_BUILD)/.flags
	mkdir -p $(BUILD)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) -MMD -MP -MF $@.d -MT $@ $< $(SAN_BUILD)/libkds.a $(LDFLAGS) -o $@
# Server tests hardcode build/kds_engine; their copied root selects the sanitizer binary.
test: all $(SAN_ENGINE) $(TEST_BINS)
	@echo 'Sanitizers: $(if $(SAN_FLAGS),ASan + UBSan,unavailable; running without instrumentation)'
	@set -e; for test in $(TEST_BINS); do echo "Running $$test"; "$$test"; done; \
	if test -f tests/run_cases.py; then python3 tests/run_cases.py --engine $(SAN_ENGINE); fi; \
	if test -f tests/test_server.py; then \
		rm -rf $(SAN_BUILD)/server-suite; \
		mkdir -p $(SAN_BUILD)/server-suite/build; \
		cp -R tests $(SAN_BUILD)/server-suite/tests; \
		ln -s "$(CURDIR)/src" $(SAN_BUILD)/server-suite/src; \
		ln -s "$(abspath $(SAN_ENGINE))" $(SAN_BUILD)/server-suite/build/kds_engine; \
		python3 $(SAN_BUILD)/server-suite/tests/test_server.py; \
	fi; \
	echo 'All available tests passed.'
run: all
	@echo 'http://127.0.0.1:8080'
	python3 src/ui/server.py --engine $(BUILD)/kds_engine
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/src/kernel modules
clean:
	rm -rf $(BUILD) src/ui/__pycache__
	rm -f src/kernel/*.o src/kernel/*.ko src/kernel/*.mod src/kernel/*.mod.c src/kernel/Module.symvers src/kernel/modules.order src/kernel/.*.cmd
-include $(wildcard $(BUILD)/*.d $(SAN_BUILD)/*.d $(BUILD)/tests/*.d)
