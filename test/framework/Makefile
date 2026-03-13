# =============================================================================
# Integration Test Makefile — ASPICE SWE.5
#
# Builds and runs integration tests that link multiple BSW modules together.
# Tests verify inter-module interfaces, safety paths, and communication chains.
#
# Usage:
#   make test                           Run all integration tests
#   make test-e2e_chain_asild           Run a single integration test
#   make test-asild                     Run only ASIL D integration tests
#   make test-asilc                     Run only ASIL C integration tests
#   make test-safety                    Run all safety integration tests (ASIL C-D)
#   make coverage                       Run with coverage instrumentation
#   make clean                          Remove build artifacts
#
# @aspice SWE.5 — Software Component Verification & Integration
# @iso    ISO 26262 Part 6, Section 10
# =============================================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c99 -pedantic -g
CFLAGS += -DUNIT_TEST -DINTEGRATION_TEST

# Coverage flags (set by coverage target)
COV_CFLAGS  =
COV_LDFLAGS =

# --- Directories ---
FW_DIR      = ../../firmware
BSW_DIR     = $(FW_DIR)/bsw

UNITY_DIR   = $(FW_DIR)/lib/vendor/unity
BUILD_DIR   = build

# --- Include paths ---
INCLUDES  = -I$(BSW_DIR)/include
INCLUDES += -I$(BSW_DIR)/mcal/Can/include -I$(BSW_DIR)/mcal/Spi/include -I$(BSW_DIR)/mcal/Adc/include
INCLUDES += -I$(BSW_DIR)/mcal/Pwm/include -I$(BSW_DIR)/mcal/Dio/include -I$(BSW_DIR)/mcal/Gpt/include
INCLUDES += -I$(BSW_DIR)/mcal/Uart/include
INCLUDES += -I$(BSW_DIR)/ecual/CanIf/include -I$(BSW_DIR)/ecual/PduR/include -I$(BSW_DIR)/ecual/IoHwAb/include
INCLUDES += -I$(BSW_DIR)/services/Com/include -I$(BSW_DIR)/services/Dcm/include -I$(BSW_DIR)/services/Dem/include
INCLUDES += -I$(BSW_DIR)/services/E2E/include -I$(BSW_DIR)/services/WdgM/include -I$(BSW_DIR)/services/BswM/include
INCLUDES += -I$(BSW_DIR)/services/NvM/include -I$(BSW_DIR)/services/SchM/include -I$(BSW_DIR)/services/Det/include
INCLUDES += -I$(BSW_DIR)/services/CanTp/include
INCLUDES += -I$(BSW_DIR)/rte/include
INCLUDES += -I$(BSW_DIR)/os/include
INCLUDES += -I$(UNITY_DIR)
INCLUDES += -I$(FW_DIR)/ecu/cvc/include
INCLUDES += -I$(FW_DIR)/ecu/fzc/include
INCLUDES += -I$(FW_DIR)/ecu/rzc/include
INCLUDES += -I.

# --- Unity source ---
UNITY_SRC = $(UNITY_DIR)/unity.c

# --- BSW Source Groups ---
# Communication stack: Can -> CanIf -> PduR -> Com
BSW_COM_STACK = $(BSW_DIR)/ecual/CanIf/src/CanIf.c $(BSW_DIR)/ecual/PduR/src/PduR.c $(BSW_DIR)/services/Com/src/Com.c

# Safety services (Dem depends on NvM for persistence)
BSW_SAFETY = $(BSW_DIR)/services/WdgM/src/WdgM.c $(BSW_DIR)/services/BswM/src/BswM.c $(BSW_DIR)/services/Dem/src/Dem.c $(BSW_DIR)/services/NvM/src/NvM.c

# Diagnostics (Dcm depends on BswM for ECUReset)
BSW_DIAG = $(BSW_DIR)/services/Dcm/src/Dcm.c $(BSW_DIR)/services/Dem/src/Dem.c $(BSW_DIR)/services/NvM/src/NvM.c $(BSW_DIR)/services/BswM/src/BswM.c

# E2E protection
BSW_E2E = $(BSW_DIR)/services/E2E/src/E2E.c

# RTE
BSW_RTE = $(BSW_DIR)/rte/src/Rte.c

# DET + SchM (required by all BSW modules after hardening)
BSW_CORE = $(BSW_DIR)/services/Det/src/Det.c $(BSW_DIR)/services/SchM/src/SchM.c

# --- Test source files ---
TEST_SRCS = $(wildcard test_int_*.c)
TEST_BINS = $(patsubst %.c,$(BUILD_DIR)/%,$(TEST_SRCS))

# --- ASIL-filtered test sets ---
TEST_ASILD  = $(filter %_asild, $(TEST_BINS))
TEST_ASILC  = $(filter %_asilc, $(TEST_BINS))
TEST_SAFETY = $(TEST_ASILD) $(TEST_ASILC)

# =============================================================================
# Targets
# =============================================================================

.PHONY: test test-asild test-asilc test-safety clean coverage

test: $(TEST_BINS)
	@echo "=== Running all integration tests ==="
	@passed=0; failed=0; total=0; \
	for t in $(TEST_BINS); do \
		total=$$((total + 1)); \
		echo "--- Running $$t ---"; \
		if ./$$t; then \
			passed=$$((passed + 1)); \
		else \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo "=== Results: $$passed/$$total passed, $$failed failed ==="; \
	if [ $$failed -ne 0 ]; then exit 1; fi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) coverage

# --- Individual test targets (convenience) ---
test-%: $(BUILD_DIR)/test_int_%
	./$(BUILD_DIR)/test_int_$*

# --- ASIL-filtered test targets ---

test-asild: $(TEST_ASILD)
	@echo "=== Running ASIL D integration tests ==="
	@for t in $^; do ./$$t || exit 1; done

test-asilc: $(TEST_ASILC)
	@echo "=== Running ASIL C integration tests ==="
	@for t in $^; do ./$$t || exit 1; done

test-safety: $(TEST_SAFETY)
	@echo "=== Running all safety integration tests (ASIL C-D) ==="
	@for t in $^; do ./$$t || exit 1; done

# =============================================================================
# Integration Test Build Rules
#
# Each test links a specific set of BSW modules together. Only the
# hardware layer (Can_Hw_*, Dio_*, Adc_*) is mocked in the test file.
# =============================================================================

# INT-003: E2E protect -> transmit -> receive -> check (ASIL D)
$(BUILD_DIR)/test_int_e2e_chain_asild: test_int_e2e_chain_asild.c $(BSW_E2E) $(BSW_COM_STACK) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-004: DEM error report -> DCM DTC readout (ASIL C)
$(BUILD_DIR)/test_int_dem_to_dcm_asilc: test_int_dem_to_dcm_asilc.c $(BSW_DIAG) $(BSW_DIR)/ecual/PduR/src/PduR.c $(BSW_DIR)/ecual/CanIf/src/CanIf.c $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-005: WdgM checkpoint -> deadline violation -> BswM safe state (ASIL D)
$(BUILD_DIR)/test_int_wdgm_supervision_asild: test_int_wdgm_supervision_asild.c $(BSW_SAFETY) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-006: BswM mode transitions propagate to all dependent modules (ASIL C)
$(BUILD_DIR)/test_int_bswm_mode_asilc: test_int_bswm_mode_asilc.c $(BSW_DIR)/services/BswM/src/BswM.c $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-007: Critical fault -> all actuators to safe state (ASIL D)
$(BUILD_DIR)/test_int_safe_state_asild: test_int_safe_state_asild.c $(BSW_SAFETY) $(BSW_RTE) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-008: ECU heartbeat timeout -> degraded mode (ASIL D)
$(BUILD_DIR)/test_int_heartbeat_loss_asild: test_int_heartbeat_loss_asild.c $(BSW_SAFETY) $(BSW_RTE) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-010: Current sensor -> threshold -> motor shutdown -> DEM (ASIL D)
$(BUILD_DIR)/test_int_overcurrent_chain_asild: test_int_overcurrent_chain_asild.c $(BSW_SAFETY) $(BSW_RTE) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-011: CAN message matrix verification (ASIL C)
$(BUILD_DIR)/test_int_can_matrix_asilc: test_int_can_matrix_asilc.c $(BSW_COM_STACK) $(BSW_E2E) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-012: Signal routing full stack (ASIL C)
$(BUILD_DIR)/test_int_signal_routing_asilc: test_int_signal_routing_asilc.c $(BSW_COM_STACK) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-013/014: E2E CRC corruption and sequence gap detection (ASIL D)
$(BUILD_DIR)/test_int_e2e_faults_asild: test_int_e2e_faults_asild.c $(BSW_E2E) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# INT-015: CAN bus-off recovery (ASIL D)
$(BUILD_DIR)/test_int_can_busoff_asild: test_int_can_busoff_asild.c $(BSW_COM_STACK) $(BSW_CORE) $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COV_CFLAGS) $(INCLUDES) $^ -o $@ $(COV_LDFLAGS)

# =============================================================================
# Coverage
# =============================================================================

coverage: COV_CFLAGS  = -fprofile-arcs -ftest-coverage
coverage: COV_LDFLAGS = -lgcov --coverage
coverage: clean $(TEST_BINS)
	@echo "=== Running integration tests with coverage ==="
	@passed=0; failed=0; total=0; \
	for t in $(TEST_BINS); do \
		total=$$((total + 1)); \
		echo "--- Running $$t ---"; \
		if ./$$t; then \
			passed=$$((passed + 1)); \
		else \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo "=== Results: $$passed/$$total passed, $$failed failed ==="; \
	if [ $$failed -ne 0 ]; then exit 1; fi
	@echo "=== Collecting integration test coverage ==="
	mkdir -p coverage
	lcov --capture --directory $(BUILD_DIR) --output-file coverage/int.info \
		--rc branch_coverage=1 --ignore-errors mismatch
	lcov --remove coverage/int.info '*/test/*' \
		--output-file coverage/int-filtered.info --rc branch_coverage=1 \
		--ignore-errors unused
	genhtml coverage/int-filtered.info --output-directory coverage/html \
		--branch-coverage --title "Integration Test Coverage"
	@echo "=== Integration Coverage Summary ==="
	lcov --summary coverage/int-filtered.info --rc branch_coverage=1
