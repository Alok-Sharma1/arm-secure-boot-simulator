# ARM Bare-Metal Secure Boot Simulator

> **Resume bullet:**  
> *Developed a minimal boot-sequence simulator in C demonstrating the ARM
> Cortex-M boot flow, vector-table initialisation, SHA-256 firmware integrity
> checking, and HMAC-SHA256 keyed authentication — simulating an industry-grade
> Secure Boot chain-of-trust.*

---

## Table of Contents
1. [What This Project Is](#1-what-this-project-is)
2. [Theory You Will Learn](#2-theory-you-will-learn)
   - 2.1 [ARM Cortex-M Boot Flow](#21-arm-cortex-m-boot-flow)
   - 2.2 [Linker Scripts and LMA vs VMA](#22-linker-scripts-and-lma-vs-vma)
   - 2.3 [SHA-256 Cryptographic Hash Function](#23-sha-256-cryptographic-hash-function)
   - 2.4 [HMAC-SHA256 Message Authentication](#24-hmac-sha256-message-authentication)
   - 2.5 [Secure Boot Chain of Trust](#25-secure-boot-chain-of-trust)
   - 2.6 [Constant-Time Comparisons](#26-constant-time-comparisons)
3. [Project Structure](#3-project-structure)
4. [Quick Start](#4-quick-start)
5. [Build Targets](#5-build-targets)
6. [How the Simulator Works](#6-how-the-simulator-works)
7. [Key Concepts by File](#7-key-concepts-by-file)
8. [Expected Output](#8-expected-output)

---

## 1. What This Project Is

This is a **from-scratch C implementation** of the boot sequence found in every
ARM Cortex-M microcontroller (STM32, nRF52, LPC55, etc.), extended with a
**Secure Boot verification pipeline** that checks firmware integrity before
handing over control.

It runs on both:
- **Host (Linux/macOS)** — via a `-DSIMULATION` build that replaces hardware
  register writes with `printf`, so you can see the full output instantly.
- **Real ARM hardware** — via `arm-none-eabi-gcc` cross-compilation.

---

## 2. Theory You Will Learn

### 2.1 ARM Cortex-M Boot Flow

The Cortex-M CPU does **not** jump to `main()` directly. On every reset:

```
Power-on / Reset
      │
      ▼
CPU reads Flash[0x08000000 + 0]  → loads into MSP (stack pointer)
CPU reads Flash[0x08000000 + 4]  → jumps to Reset_Handler
      │
      ▼
Reset_Handler()          ← startup.c
  ① Copy .data  Flash → SRAM   (initialized globals)
  ② Zero  .bss  in SRAM        (uninitialized globals = 0 by C standard)
  ③ Call  SystemInit()         (optional clock/FPU setup)
  ④ Call  main()
```

The address table at the start of Flash is the **Interrupt Vector Table**
(`vector_table.c`).  Entry `[0]` is the initial stack pointer; entry `[1]`
is `Reset_Handler`; entries `[2..15]` are the 14 Cortex-M core exception
handlers.

### 2.2 Linker Scripts and LMA vs. VMA

Every variable has two addresses:

| Term | Meaning |
|------|---------|
| **LMA** (Load Memory Address)    | Where the initial value is *stored* in Flash at link time |
| **VMA** (Virtual Memory Address) | Where the CPU *reads/writes* it at runtime (SRAM)          |

The linker script (`linker/cortex_m.ld`) uses `AT > FLASH` to place the
`.data` section's initial values in Flash while its runtime address is in SRAM.
`startup.c` copies them with a `while (dst < &_edata) { *dst++ = *src++; }`
loop before `main()` runs.

### 2.3 SHA-256 Cryptographic Hash Function

SHA-256 (FIPS 180-4) maps any input to a deterministic 32-byte digest.

```
SHA-256("abc") = ba7816bf 8f01cfea 414140de 5dae2ec7
                  3b00361b bef0469f a5395fdc 39411468
```

Key properties for secure boot:
- **One-way**: cannot reverse the digest to recover the input.
- **Avalanche effect**: flipping 1 bit in the firmware produces a completely
  different 256-bit digest.
- **Collision resistance**: practically impossible to find two different inputs
  with the same digest.

The algorithm:
1. Pad message to a multiple of 512 bits.
2. Parse into 512-bit blocks.
3. For each block run 64 compression rounds using six bitwise functions
   (Ch, Maj, Σ₀, Σ₁, σ₀, σ₁) and 64 round constants derived from cube
   roots of primes.
4. Output = 8 × 32-bit words in big-endian order.

### 2.4 HMAC-SHA256 Message Authentication

HMAC adds a **secret key** to SHA-256:

```
HMAC(K, m) = SHA-256( (K' XOR opad) || SHA-256( (K' XOR ipad) || m ) )
```

Where `ipad = 0x36 × 64` and `opad = 0x5C × 64`.  
Only someone with the secret key `K` can produce a valid MAC.  
Verification: recompute the MAC and compare with constant-time comparison.

### 2.5 Secure Boot Chain of Trust

```
Manufacture time:
  1. Generate OEM keypair  (RSA-2048 or ECDSA-P256)
  2. Compute SHA-256(firmware) → burn into OTP fuses  (read-protected)
  3. Sign firmware: sig = RSA_sign(priv_key, hash)   → store in Flash header

Every boot:
  Stage 1 – Integrity:     SHA-256(firmware) == OTP hash?  else HALT
  Stage 2 – Authenticity:  HMAC_verify(device_key, fw) ?   else HALT
  Stage 3 (production):    RSA_verify(pub_key, hash, sig)?  else HALT
  ✓ → jump to application entry point
```

This simulator implements Stages 1 and 2 without requiring RSA.

### 2.6 Constant-Time Comparisons

`memcmp()` returns as soon as it finds the first difference — the number of
matching bytes is measurable by a side-channel attacker (timing attack).  
`secure_memcmp()` in `signature.c` always examines all bytes:

```c
uint8_t diff = 0;
for (i = 0; i < len; i++) { diff |= a[i] ^ b[i]; }  // no early exit
return (int)diff;
```

---

## 3. Project Structure

```
arm-secure-boot-simulator/
├── include/
│   └── common.h            # Shared macros (SUCCESS, ARRAY_SIZE, BIT…)
├── linker/
│   └── cortex_m.ld         # Linker script: Flash/SRAM layout, LMA/VMA
├── scripts/
│   └── run_sim.sh          # Convenience build+run script
├── src/
│   ├── main.c              # Boot sequence orchestrator
│   ├── boot/
│   │   ├── startup.c/h     # Reset_Handler: copy .data, zero .bss
│   │   └── vector_table.c/h# 16-entry ARM Cortex-M4 exception table
│   ├── crypto/
│   │   ├── sha256.c/h      # Full FIPS 180-4 SHA-256 implementation
│   │   └── signature.c/h   # HMAC-SHA256, verify_firmware_hash, secure_memcmp
│   ├── hal/
│   │   ├── memory_map.h    # Flash/SRAM/peripheral base addresses
│   │   ├── registers.h     # GPIO register macros
│   │   └── uart.c/h        # UART TX/RX (simulation: printf)
│   └── utils/
│       └── logger.c/h      # Log levels + hex dump over UART
├── tests/
│   ├── test_crypto.c       # NIST SHA-256 + RFC 4231 HMAC-SHA256 tests
│   └── test_boot.c         # Vector table + memory-map tests
├── Makefile
└── README.md
```

---

## 4. Quick Start

```bash
# Clone
git clone https://github.com/YOUR_USERNAME/arm-secure-boot-simulator.git
cd arm-secure-boot-simulator

# Run simulation on host (no ARM toolchain needed)
make sim

# Run unit tests
make test

# Cross-compile for real hardware (requires arm-none-eabi-gcc)
sudo apt install gcc-arm-none-eabi
make arm
```

---

## 5. Build Targets

| Target       | Description                                                    |
|--------------|----------------------------------------------------------------|
| `make sim`   | Build with `gcc -DSIMULATION` and run on the host             |
| `make arm`   | Cross-compile with `arm-none-eabi-gcc` → `build/secure_boot.elf` |
| `make test`  | Build and run `test_crypto` + `test_boot` on the host         |
| `make clean` | Remove the `build/` directory                                  |

---

## 6. How the Simulator Works

```
main()
  │
  ├─ logger_init()            UART init + boot banner
  ├─ dump_vector_table()      Print all 16 exception handler addresses
  │
  ├─ STAGE 1: SHA-256 Hash Verification
  │    secure_boot_get_firmware()     → 64-byte simulated firmware blob
  │    sha256_compute(firmware)       → 32-byte computed digest
  │    secure_boot_get_trusted_hash() → 32-byte OTP reference (pre-computed)
  │    secure_memcmp(computed, trusted)
  │      match   → STAGE 1 PASSED
  │      mismatch → HALT (boot_sig_fail)
  │
  ├─ STAGE 2: HMAC-SHA256 Authentication
  │    hmac_sha256(device_key, firmware) → 32-byte MAC
  │    verify_hmac_signature(...)        → constant-time compare
  │      match   → STAGE 2 PASSED
  │      mismatch → HALT
  │
  ├─ TAMPER DEMO
  │    Copy firmware, flip byte[10], re-run SHA-256 verify
  │    → Expect BOOT_SIG_FAIL (avalanche effect demonstration)
  │
  └─ boot_application()       or  enter_safe_state()
```

---

## 7. Key Concepts by File

| File | Concepts covered |
|------|------------------|
| `startup.c`      | Reset_Handler, .data copy (LMA→VMA), .bss zeroing, weak symbols |
| `vector_table.c` | ARM exception model, vector table layout, `__attribute__((section))` |
| `sha256.c`       | FIPS 180-4, compression function, message schedule, big-endian packing |
| `signature.c`    | HMAC-SHA256 (RFC 2104), constant-time comparison, OTP simulation |
| `uart.c`         | Memory-mapped I/O, `volatile`, hardware/simulation abstraction |
| `logger.c`       | Bare-metal printf alternative, log levels, hex dump without stdlib |
| `cortex_m.ld`    | ENTRY, MEMORY, SECTIONS, AT>FLASH, _sidata/_sdata/_edata/_sbss/_ebss |
| `Makefile`       | GCC cross-compilation, dual-target build, simulation flag |

---

## 8. Expected Output

```
========================================
  ARM Cortex-M4 Secure Boot Simulator
========================================
----------------------------------------
[INFO]  ARM Cortex-M4 Secure Boot Simulator v1.0
[INFO]  Flash : 0x08000000  (512 KB)
[INFO]  SRAM  : 0x20000000  (128 KB)
[INFO]  Stack : 0x20020000  (top of SRAM)
----------------------------------------
[INFO]  Vector Table (base: 0x08000000):
  +------+------------------+------------+
  | Idx  | Name             | Address    |
  ...all 16 entries...
----------------------------------------
[INFO]  STAGE 1: SHA-256 Integrity Verification
[INFO]    Computed : BA 78 16 BF 8F 01 CF EA ...
[INFO]    Trusted  : BA 78 16 BF 8F 01 CF EA ...
[INFO]  STAGE 1 PASSED -- firmware integrity confirmed.
----------------------------------------
[INFO]  STAGE 2: HMAC-SHA256 Authentication
[INFO]    HMAC     : <32 bytes>
[INFO]  STAGE 2 PASSED -- firmware authenticated.
----------------------------------------
[WARN]  TAMPER DEMO RESULT: Tampered firmware CORRECTLY REJECTED.
----------------------------------------
[INFO]  SECURE BOOT: VERIFICATION OK
[INFO]  >>> APPLICATION IS NOW RUNNING <<<
```
   ./scripts/run_sim.sh
   ```

## Usage
The simulator initializes the system, sets up the boot sequence, and performs a cryptographic signature verification step. You can modify the source code in the `src` directory to experiment with different boot configurations or cryptographic algorithms.

## Testing
To run the unit tests, use the following command:
```
make test
```
This will compile and execute the tests defined in the `tests` directory, providing feedback on the correctness of the implemented functions.

## Conclusion
This project enhances your understanding of embedded systems, secure boot processes, and low-level programming. It serves as a practical demonstration of how to implement a secure boot mechanism on ARM Cortex-M microcontrollers, making it a valuable addition to your resume.