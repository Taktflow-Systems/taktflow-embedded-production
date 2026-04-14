# =============================================================================
# BSW Unit Test Makefile -- runs on host (x86/x64)
#
# Usage:
#   make test           - Build and run all unit tests
#   make test-Can_asild - Build and run a specific test (e.g., test_Can_asild.c)
#   make test-asild     - Build and run all ASIL D tests
#   make test-qm        - Build and run all QM tests
#   make test-safety     - Build and run all safety tests (ASIL A-D)
#   make coverage       - Build with coverage, run tests, generate lcov report
#   make clean          - Remove build artifacts
#
# Convention:
#   Test file:   test/test_<Module>_<asil>.c  (e.g., test/test_Can_asild.c)
#   Source file:  <layer>/<Module>/src/<Module>.c  (e.g., mcal/Can/src/Can.c)
#   The Makefile auto-discovers source files across BSW layers.
#   ASIL suffix (_asild, _asilc, _asilb, _asila, _qm) is stripped to find source.
# =============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -g
CFLAGS += -DUNIT_TEST -DCANTP_ENABLED
CFLAGS += -Iinclude
CFLAGS += -Imcal/Can/include -Imcal/Spi/include -Imcal/Adc/include
CFLAGS += -Imcal/Pwm/include -Imcal/Dio/include -Imcal/Gpt/include -Imcal/Uart/include
CFLAGS += -Iecual/CanIf/include -Iecual/PduR/include -Iecual/IoHwAb/include
CFLAGS += -Iservices/Com/include -Iservices/Dcm/include -Iservices/Dem/include
CFLAGS += -Iservices/E2E/include -Iservices/WdgM/include -Iservices/BswM/include
CFLAGS += -Iservices/NvM/include -Iservices/SchM/include -Iservices/Det/include
CFLAGS += -Iservices/CanTp/include
CFLAGS += -Irte/include
CFLAGS += -Ios/include

# Source directories
MCAL_DIR = mcal
ECUAL_DIR = ecual
SERVICES_DIR = services
RTE_DIR = rte
TEST_DIR = test
UNITY_DIR = ../../firmware/lib/vendor/unity
CFLAGS += -I$(UNITY_DIR)

# Unity source
UNITY_SRC = $(UNITY_DIR)/unity.c

# DET source — all BSW modules call Det_ReportError(), so Det.c must be
# linked with every test.  The det_dep helper excludes it when the test
# module IS Det (find_source already returns Det.c in that case).
DET_SRC = $(SERVICES_DIR)/Det/src/Det.c

# SchM source — modules with SchM critical sections call SchM_Enter/Exit
# which resolve to real functions in UNIT_TEST builds.
SCHM_SRC = $(SERVICES_DIR)/SchM/src/SchM.c

# Find all test files
TEST_SRCS = $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(TEST_DIR)/build/%)

# Coverage flags (appended when building with coverage target)
COV_CFLAGS  =
COV_LDFLAGS =

# ASIL-filtered test sets
TEST_ASILD  = $(filter %_asild, $(TEST_BINS))
TEST_ASILC  = $(filter %_asilc, $(TEST_BINS))
TEST_ASILB  = $(filter %_asilb, $(TEST_BINS))
TEST_ASILA  = $(filter %_asila, $(TEST_BINS))
TEST_QM     = $(filter %_qm,    $(TEST_BINS))
TEST_SAFETY = $(TEST_ASILD) $(TEST_ASILC) $(TEST_ASILB) $(TEST_ASILA)

# Default: run all tests
.PHONY: test test-asild test-asilc test-asilb test-asila test-qm test-safety clean coverage coverage-report

test: $(TEST_BINS)
	@echo "=== Running all BSW unit tests ==="
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

# --- ASIL-filtered test targets ---

test-asild: $(TEST_ASILD)
	@echo "=== Running ASIL D BSW tests ==="
	@for t in $^; do ./$$t || exit 1; done

test-asilc: $(TEST_ASILC)
	@echo "=== Running ASIL C BSW tests ==="
	@for t in $^; do ./$$t || exit 1; done

test-asilb: $(TEST_ASILB)
	@echo "=== Running ASIL B BSW tests ==="
	@for t in $^; do ./$$t || exit 1; done

test-asila: $(TEST_ASILA)
	@echo "=== Running ASIL A BSW tests ==="
	@for t in $^; do ./$$t || exit 1; done

test-qm: $(TEST_QM)
	@echo "=== Running QM BSW tests ==="
	@for t in $^; do ./$$t || exit 1; done

test-safety: $(TEST_SAFETY)
	@echo "=== Running all safety BSW tests (ASIL A-D) ==="
	@for t in $^; do ./$$t || exit 1; done

# Build directory
$(TEST_DIR)/build:
	mkdir -p $(TEST_DIR)/build

# Pattern rule: compile test + matching source + unity + Det + SchM
# Each test_<Module>_<asil>.c links with <Module>.c from the appropriate BSW layer.
# strip_asil removes the ASIL suffix before calling find_source.
# det_dep/schm_dep conditionally include Det.c/SchM.c (excluded when module IS that module).
$(TEST_DIR)/build/test_%: $(TEST_DIR)/test_%.c $(UNITY_SRC) | $(TEST_DIR)/build
	$(CC) $(CFLAGS) $(COV_CFLAGS) -I$(TEST_DIR)/mocks \
		$< $(call find_source,$(call strip_asil,$*)) $(call extra_srcs,$*) $(call det_dep,$*) $(call schm_dep,$*) $(UNITY_SRC) -o $@ $(COV_LDFLAGS)

# Helper: strip ASIL suffix from module name (e.g., Can_asild -> Can)
strip_asil = $(patsubst %_asild,%,$(patsubst %_asilc,%,$(patsubst %_asilb,%,$(patsubst %_asila,%,$(patsubst %_qm,%,$(1))))))

# Helper: find the source file for a module across BSW layers
# First try exact match (Module/src/Module.c), then sub-module match (*/src/Module.c)
# so that e.g. Det_Callout_Sil resolves to Det/src/Det_Callout_Sil.c
define find_source
$(or $(wildcard $(MCAL_DIR)/$(1)/src/$(1).c),$(wildcard $(ECUAL_DIR)/$(1)/src/$(1).c),$(wildcard $(SERVICES_DIR)/$(1)/src/$(1).c),$(wildcard $(RTE_DIR)/$(1)/src/$(1).c),$(wildcard $(RTE_DIR)/src/$(1).c),$(wildcard $(MCAL_DIR)/*/src/$(1).c),$(wildcard $(ECUAL_DIR)/*/src/$(1).c),$(wildcard $(SERVICES_DIR)/*/src/$(1).c),$(wildcard $(RTE_DIR)/*/src/$(1).c))
endef

extra_srcs = $(if $(filter Dcm,$(call strip_asil,$(1))),$(SERVICES_DIR)/Dcm/src/Dcm_ReadDtcInfo.c $(SERVICES_DIR)/Dcm/src/Dcm_ClearDtc.c $(SERVICES_DIR)/Dcm/src/Dcm_RoutineControl.c,)

# Helper: return DET_SRC unless the module IS Det (avoids double-linking)
det_dep = $(if $(filter Det,$(call strip_asil,$(1))),,$(DET_SRC))

# Helper: return SCHM_SRC unless the module IS SchM (avoids double-linking)
schm_dep = $(if $(filter SchM,$(call strip_asil,$(1))),,$(SCHM_SRC))

clean:
	rm -rf $(TEST_DIR)/build
	rm -rf coverage

# Individual test targets (convenience)
test-%: $(TEST_DIR)/build/test_%
	./$(TEST_DIR)/build/test_$*

# =============================================================================
# Coverage targets — gcov + lcov
#
# Builds all tests with --coverage, runs them, collects coverage data with
# lcov, and generates an HTML report. Measures statement, branch, and
# function coverage per IEC 61508 / ISO 26262 requirements.
#
# Prerequisites: gcc, gcov, lcov, genhtml
# =============================================================================

# Coverage build: rebuild tests with --coverage, run, collect
coverage: COV_CFLAGS  = -fprofile-arcs -ftest-coverage
coverage: COV_LDFLAGS = -lgcov --coverage
coverage: clean $(TEST_BINS)
	@echo "=== Running BSW tests with coverage ==="
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
	@echo "=== Collecting coverage data ==="
	mkdir -p coverage
	lcov --capture --directory $(TEST_DIR)/build --output-file coverage/bsw.info \
		--rc branch_coverage=1 --ignore-errors mismatch --ignore-errors unused
	lcov --remove coverage/bsw.info '*/test/*' '/usr/*' \
		--output-file coverage/bsw-filtered.info --rc branch_coverage=1 --ignore-errors unused
	@echo "=== Generating HTML report ==="
	genhtml coverage/bsw-filtered.info --output-directory coverage/html \
		--branch-coverage --title "BSW Unit Test Coverage"
	@echo "=== Coverage Summary ==="
	lcov --summary coverage/bsw-filtered.info --rc branch_coverage=1
	@echo "=== HTML report: coverage/html/index.html ==="

# Text-only summary (for CI without genhtml)
coverage-report: COV_CFLAGS  = -fprofile-arcs -ftest-coverage
coverage-report: COV_LDFLAGS = -lgcov --coverage
coverage-report: clean $(TEST_BINS)
	@for t in $(TEST_BINS); do ./$$t || true; done
	mkdir -p coverage
	lcov --capture --directory $(TEST_DIR)/build --output-file coverage/bsw.info \
		--rc branch_coverage=1 --ignore-errors mismatch --ignore-errors unused
	lcov --remove coverage/bsw.info '*/test/*' '/usr/*' \
		--output-file coverage/bsw-filtered.info --rc branch_coverage=1 --ignore-errors unused
	lcov --summary coverage/bsw-filtered.info --rc branch_coverage=1
