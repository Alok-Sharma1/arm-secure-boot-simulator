# =============================================================================
# Makefile -- ARM Bare-Metal Secure Boot Simulator
#
# Targets
# ───────
#   make sim     Build + run native simulation (host Linux/macOS, no ARM hw)
#   make arm     Cross-compile for ARM Cortex-M4 (requires arm-none-eabi-gcc)
#   make test    Build + run unit tests on host
#   make clean   Remove the build/ directory
#   make help    Show this summary
#
# Quick start (no ARM toolchain needed):
#   make sim
# =============================================================================

# ---- Directories -----------------------------------------------------------
SRC_DIR    := src
INC_DIR    := include
BUILD_DIR  := build
TEST_DIR   := tests
LINKER     := linker/cortex_m.ld

# ---- Source files ----------------------------------------------------------
APP_SRCS := \
    $(SRC_DIR)/main.c               \
    $(SRC_DIR)/boot/startup.c       \
    $(SRC_DIR)/boot/vector_table.c  \
    $(SRC_DIR)/crypto/sha256.c      \
    $(SRC_DIR)/crypto/signature.c   \
    $(SRC_DIR)/hal/uart.c           \
    $(SRC_DIR)/utils/logger.c

# Source files shared between tests (no main.c)
LIB_SRCS := \
    $(SRC_DIR)/boot/startup.c       \
    $(SRC_DIR)/boot/vector_table.c  \
    $(SRC_DIR)/crypto/sha256.c      \
    $(SRC_DIR)/crypto/signature.c   \
    $(SRC_DIR)/hal/uart.c           \
    $(SRC_DIR)/utils/logger.c

# ---- Include paths ---------------------------------------------------------
INCLUDES := \
    -I$(INC_DIR)           \
    -I$(SRC_DIR)/boot      \
    -I$(SRC_DIR)/crypto    \
    -I$(SRC_DIR)/hal       \
    -I$(SRC_DIR)/utils

# =============================================================================
# SIMULATION BUILD  --  runs on host x86/Linux with gcc
#
# -DSIMULATION  replaces HW register writes with printf/putchar
# =============================================================================
SIM_CC     := gcc
SIM_CFLAGS := -Wall -Wextra -std=c99 -g -DSIMULATION $(INCLUDES)
SIM_OUT    := $(BUILD_DIR)/secure_boot_sim

# =============================================================================
# ARM CROSS-COMPILE BUILD  --  produces ELF for Cortex-M4
#
# Install toolchain: sudo apt install gcc-arm-none-eabi
# Flash with:        openocd / st-flash / pyocd
# =============================================================================
ARM_CC     := arm-none-eabi-gcc
ARM_CFLAGS := \
    -mcpu=cortex-m4  \
    -mthumb          \
    -mfloat-abi=soft \
    -Wall -Wextra    \
    -std=c99         \
    -O2 -g           \
    -ffunction-sections \
    -fdata-sections  \
    $(INCLUDES)
ARM_LFLAGS := -T$(LINKER) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/output.map
ARM_OUT    := $(BUILD_DIR)/secure_boot.elf

# =============================================================================
# TEST BUILDS  --  two separate executables (one per test file)
# =============================================================================
TEST_CRYPTO_OUT := $(BUILD_DIR)/test_crypto
TEST_BOOT_OUT   := $(BUILD_DIR)/test_boot

# =============================================================================
.PHONY: all sim arm test clean help

all: sim

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ---------------------------------------------------------------------------
## sim  : Build and run the secure-boot simulation on host (no ARM hw needed)
sim: $(BUILD_DIR)
	@echo ">>> Building simulation binary..."
	$(SIM_CC) $(SIM_CFLAGS) -o $(SIM_OUT) $(APP_SRCS)
	@echo ""
	@echo ">>> Running ARM Cortex-M4 Secure Boot Simulator <<<"
	@echo ""
	./$(SIM_OUT)

# ---------------------------------------------------------------------------
## arm  : Cross-compile for Cortex-M4 (requires arm-none-eabi-gcc)
arm: $(BUILD_DIR)
	@echo ">>> Cross-compiling for ARM Cortex-M4..."
	$(ARM_CC) $(ARM_CFLAGS) $(ARM_LFLAGS) -o $(ARM_OUT) $(APP_SRCS)
	@echo "ELF built: $(ARM_OUT)"
	@arm-none-eabi-size $(ARM_OUT)
	@echo "Flash with: st-flash write $(ARM_OUT) 0x08000000"

# ---------------------------------------------------------------------------
## test : Build and run unit tests on host
test: $(BUILD_DIR)
	@echo ">>> Building crypto unit tests..."
	$(SIM_CC) $(SIM_CFLAGS) -o $(TEST_CRYPTO_OUT) \
	    $(TEST_DIR)/test_crypto.c \
	    $(SRC_DIR)/crypto/sha256.c \
	    $(SRC_DIR)/crypto/signature.c \
	    $(SRC_DIR)/hal/uart.c \
	    $(SRC_DIR)/utils/logger.c
	@echo ">>> Building boot unit tests..."
	$(SIM_CC) $(SIM_CFLAGS) -o $(TEST_BOOT_OUT) \
	    $(TEST_DIR)/test_boot.c \
	    $(SRC_DIR)/boot/startup.c \
	    $(SRC_DIR)/boot/vector_table.c \
	    $(SRC_DIR)/hal/uart.c
	@echo ""
	@echo "════════════════════════════════════"
	@echo " Running test_crypto"
	@echo "════════════════════════════════════"
	./$(TEST_CRYPTO_OUT)
	@echo ""
	@echo "════════════════════════════════════"
	@echo " Running test_boot"
	@echo "════════════════════════════════════"
	./$(TEST_BOOT_OUT)
	@echo ""
	@echo ">>> All tests complete."

# ---------------------------------------------------------------------------
## clean : Remove the build directory
clean:
	rm -rf $(BUILD_DIR)

# ---------------------------------------------------------------------------
## help  : Show available targets
help:
	@echo ""
	@echo "ARM Bare-Metal Secure Boot Simulator -- Build Targets"
	@echo "------------------------------------------------------"
	@grep -E '^## ' Makefile | sed 's/## /  make /'
	@echo ""