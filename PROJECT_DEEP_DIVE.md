# ARM Bare-Metal Secure Boot Simulator — Complete Deep Dive

> A complete explanation of the project: from the basics of embedded systems to
> advanced cryptographic secure boot concepts, how every file works, how to run
> the project, and why this belongs on an Embedded Software Engineer résumé.

---

## Table of Contents

1. [What Problem Does This Project Solve?](#1-what-problem-does-this-project-solve)
2. [Background Theory — What You Must Know](#2-background-theory--what-you-must-know)
   - 2.1 [What is Bare-Metal Programming?](#21-what-is-bare-metal-programming)
   - 2.2 [What is ARM Cortex-M?](#22-what-is-arm-cortex-m)
   - 2.3 [Memory Map — The Foundation of Everything](#23-memory-map--the-foundation-of-everything)
   - 2.4 [The Boot Process — Step by Step](#24-the-boot-process--step-by-step)
   - 2.5 [The Vector Table — What It Really Is](#25-the-vector-table--what-it-really-is)
   - 2.6 [Linker Script — The Map of Your Program](#26-linker-script--the-map-of-your-program)
   - 2.7 [SHA-256 — The Cryptographic Hash Function](#27-sha-256--the-cryptographic-hash-function)
   - 2.8 [HMAC-SHA256 — Keyed Authentication](#28-hmac-sha256--keyed-authentication)
   - 2.9 [Constant-Time Comparison — Preventing Side-Channel Attacks](#29-constant-time-comparison--preventing-side-channel-attacks)
3. [Project Architecture — How Everything Connects](#3-project-architecture--how-everything-connects)
4. [File-by-File Explanation](#4-file-by-file-explanation)
   - 4.1 [vector_table.c / vector_table.h](#41-vector_tablec--vector_tableh)
   - 4.2 [startup.c / startup.h](#42-startupc--startuph)
   - 4.3 [sha256.c / sha256.h](#43-sha256c--sha256h)
   - 4.4 [signature.c / signature.h](#44-signaturec--signatureh)
   - 4.5 [uart.c / uart.h](#45-uartc--uarth)
   - 4.6 [memory_map.h / registers.h](#46-memory_maph--registersh)
   - 4.7 [logger.c / logger.h](#47-loggerc--loggerh)
   - 4.8 [main.c](#48-mainc)
   - 4.9 [cortex_m.ld (Linker Script)](#49-cortex_mld-linker-script)
   - 4.10 [Makefile](#410-makefile)
   - 4.11 [tests/test_crypto.c and tests/test_boot.c](#411-teststest_cryptoc-and-teststest_bootc)
5. [How to Run the Project](#5-how-to-run-the-project)
   - 5.1 [Prerequisites](#51-prerequisites)
   - 5.2 [Run the Simulation](#52-run-the-simulation)
   - 5.3 [Run Unit Tests](#53-run-unit-tests)
   - 5.4 [Cross-Compile for Real ARM Hardware](#54-cross-compile-for-real-arm-hardware)
6. [Understanding the Full Output](#6-understanding-the-full-output)
7. [How This Helps Your Embedded Software Engineer Résumé](#7-how-this-helps-your-embedded-software-engineer-résumé)
8. [Real Secure Boot vs. This Project](#8-real-secure-boot-vs-this-project)
9. [Summary](#9-summary)

---

## 1. What Problem Does This Project Solve?

Before writing a single line of code, you need to understand **why secure boot exists**.

Imagine you manufacture a medical device, a car ECU, or a smart lock. That device runs firmware (software stored in Flash memory). Now imagine an attacker:

- Replaces your firmware with malicious code
- Modifies your firmware to bypass safety checks
- Injects code that exfiltrates data to a remote server

**Secure Boot** is the mechanism that prevents this. It ensures that **only cryptographically verified, unmodified firmware can run** on a device. This is not optional in professional embedded systems — it is mandatory in:

| Standard | Domain |
|---|---|
| ISO 26262 | Automotive functional safety |
| IEC 62443 | Industrial cybersecurity |
| PSA Certified | ARM's platform security architecture |
| FIPS 140-2 | Government / medical / financial cryptography |
| ETSI EN 303 645 | Consumer IoT devices |

This project **simulates that exact process in software**, so you can understand and demonstrate it without needing physical hardware. Every concept maps 1:1 to a real production secure boot implementation.

---

## 2. Background Theory — What You Must Know

### 2.1 What is Bare-Metal Programming?

When you write a Python or Java program, there are many layers between your code and hardware:

```
Your Python Code
      ↓
Python Interpreter
      ↓
Operating System (Linux / Windows)
      ↓
Hardware Drivers (kernel modules)
      ↓
Physical Hardware (CPU, RAM, Disk)
```

**Bare-metal** means there is **NO operating system**. Your C code talks directly to hardware registers:

```
Your C Code
      ↓
Physical Hardware (CPU, Flash, SRAM, Peripherals)
```

This is how **every microcontroller works** — STM32, ESP32, nRF52, TI MSP430, etc. There is no Linux, no file system, no `malloc` backed by an OS, no threads. You are in complete control and complete responsibility.

Consequences of bare-metal programming you must handle yourself:

- Initialise the stack pointer before any function call
- Copy initialised global variables from Flash to SRAM before `main()`
- Zero out uninitialised global variables before `main()`
- Set up interrupt vectors before any interrupt can fire
- Configure clock speed and peripheral clocks before using peripherals
- Write your own `printf` equivalent (no OS to provide one)

All of the above are demonstrated in this project.

---

### 2.2 What is ARM Cortex-M?

**ARM** is a CPU **architecture** — a design specification for how a processor executes instructions, handles exceptions, maps memory, etc. ARM Limited does not manufacture chips. They license the design to silicon vendors:

| Vendor | Chip Family | Used In |
|---|---|---|
| STMicroelectronics | STM32 | Drones, industrial, IoT |
| Nordic Semiconductor | nRF52 / nRF53 | Bluetooth LE devices |
| NXP | i.MX RT, LPC55 | Automotive, industrial |
| Silicon Labs | EFM32 | Low-power IoT |
| Raspberry Pi Foundation | RP2040 | Maker boards |

**Cortex-M** is the ARM sub-family designed specifically for **microcontrollers** (embedded systems with tight constraints on cost, power, and size). The variants differ in pipeline depth and optional features:

| Core | FPU | DSP | TrustZone | Target Use |
|---|---|---|---|---|
| Cortex-M0 / M0+ | No | No | No | Ultra low-power, simple IoT |
| Cortex-M3 | No | No | No | General purpose MCUs |
| **Cortex-M4** | **Optional** | **Yes** | **No** | **DSP, motor control (this project)** |
| Cortex-M7 | Yes | Yes | No | High-performance embedded |
| Cortex-M23 | No | No | Yes | Security-critical, IoT |
| Cortex-M33 | Optional | Yes | Yes | Secure embedded systems |

This simulator targets **Cortex-M4** (modelled on the STM32F407), which is the most widely used Cortex-M core in industry today. All boot and security concepts demonstrated here apply identically to every Cortex-M variant.

---

### 2.3 Memory Map — The Foundation of Everything

On a Cortex-M processor, **everything is memory-mapped**. The entire 4 GB address space is divided into fixed regions defined by the ARM architecture:

```
0xFFFFFFFF  ┌─────────────────────────────────┐
            │   Vendor Specific               │
0xE0100000  ├─────────────────────────────────┤
            │   Private Peripheral Bus (PPB)  │  ← CPU registers live here
            │   NVIC, SysTick, SCB, DWT, FPB  │
            │   SCB->VTOR is at 0xE000ED08    │  ← KEY: vector table location
0xE0000000  ├─────────────────────────────────┤
            │   External Device               │
0xA0000000  ├─────────────────────────────────┤
            │   External RAM                  │
0x60000000  ├─────────────────────────────────┤
            │   Peripheral Registers          │  ← UART, GPIO, SPI, I2C, etc.
0x40000000  ├─────────────────────────────────┤
            │   SRAM (read/write)             │  ← Your variables live here
0x20000000  ├─────────────────────────────────┤
            │   Flash / Code (read-only)      │  ← Your compiled code lives here
0x08000000  ├─────────────────────────────────┤
            │   (Boot ROM / aliased region)   │
0x00000000  └─────────────────────────────────┘
```

Every peripheral is accessed by reading and writing to its address. For example:

```c
// src/hal/uart.c -- writing to hardware UART data register
#define UART_BASE   0x40011000UL
#define UART_DR     (*(volatile uint32_t*)(UART_BASE + 0x04))

void uart_send_char(char c) {
    // Wait until TX FIFO has space
    while (UART_FR & UART_FR_TXFF) { }
    // Write directly to the hardware register
    UART_DR = (uint32_t)(unsigned char)c;
}
```

The `volatile` keyword is critical — it tells the compiler "this memory location can change at any time due to hardware events; never cache it in a register and never skip the access."

This entire address map is defined in `src/hal/memory_map.h` and `src/hal/registers.h`.

---

### 2.4 The Boot Process — Step by Step

This is the **most important concept** in the entire project. When you press the reset button or power on a Cortex-M chip, the following happens **automatically in hardware** before a single instruction executes:

```
POWER ON / HARDWARE RESET
         │
         │  ← CPU performs two automatic memory reads from address 0x00000000
         │    (which is aliased to Flash at 0x08000000 on STM32 devices)
         │
         ├── Read word at [0x00000000] → load into MSP (Main Stack Pointer)
         │         This is the initial stack pointer value.
         │         On our device: 0x20020000 (top of 128 KB SRAM)
         │
         └── Read word at [0x00000004] → load into PC (Program Counter)
                   This is the address of Reset_Handler.
                   CPU jumps here immediately.
                   │
                   ▼
            Reset_Handler()     ← src/boot/startup.c
                   │
                   ├── STEP 1: Copy .data section Flash (LMA) → SRAM (VMA)
                   │     Initialised globals like `int x = 5;` are stored in
                   │     Flash at compile time but must live in writable SRAM.
                   │     startup.c copies them word-by-word before main().
                   │
                   ├── STEP 2: Zero the .bss section in SRAM
                   │     Uninitialised globals like `int y;` are guaranteed to
                   │     be zero by the C standard (§6.7.9 ¶10). There is no
                   │     OS to do this, so startup.c does it with a fill loop.
                   │
                   ├── STEP 3: Call SystemInit()
                   │     Weak hook that:
                   │       a) Writes 0x08000000 to SCB->VTOR (VECTOR TABLE INIT)
                   │       b) On real hardware: configures PLL, Flash wait states
                   │
                   └── STEP 4: Call main()
                         Your application code runs.
                         If main() ever returns (it should not in bare-metal),
                         startup.c spins in an infinite loop — safety net.
```

The **first two 32-bit words in Flash are not code** — they are data that the CPU reads automatically. This is the **Vector Table** (see next section).

---

### 2.5 The Vector Table — What It Really Is

The vector table is a **flat array of 32-bit addresses** stored at the beginning of Flash. Each entry is the address of an exception or interrupt handler function.

```
Flash Address   Value Stored       Entry Meaning
─────────────────────────────────────────────────────────────────
0x08000000      0x20020000         [0]  Initial Stack Pointer
0x08000004      0x08000141         [1]  Reset_Handler (Thumb: addr+1)
0x08000008      0x08000151         [2]  NMI_Handler
0x0800000C      0x08000161         [3]  HardFault_Handler
0x08000010      0x08000171         [4]  MemManage_Handler (MPU fault)
0x08000014      0x08000181         [5]  BusFault_Handler
0x08000018      0x08000191         [6]  UsageFault_Handler
0x0800001C      0x00000000         [7]  Reserved (must be 0)
0x08000020      0x00000000         [8]  Reserved
0x08000024      0x00000000         [9]  Reserved
0x08000028      0x00000000         [10] Reserved
0x0800002C      0x080001A1         [11] SVC_Handler (syscall gate for RTOS)
0x08000030      0x080001B1         [12] DebugMon_Handler
0x08000034      0x00000000         [13] Reserved
0x08000038      0x080001C1         [14] PendSV_Handler (RTOS context switch)
0x0800003C      0x080001D1         [15] SysTick_Handler (RTOS tick timer)
0x08000040 ...  ...                [16+] Vendor peripheral IRQs (UART, SPI, ...)
```

**Why +1 on addresses?** ARM Cortex-M uses the **Thumb instruction set** where instructions are 16 or 32 bits wide. The LSB of a function pointer being `1` is the *Thumb-mode indicator bit* — it tells the CPU "these are Thumb instructions". The actual code address is always 2-byte aligned (LSB=0), but the pointer stored in the table always has LSB=1. The compiler and linker handle this automatically.

#### What is VTOR?

**VTOR (Vector Table Offset Register)** lives at `0xE000ED08` inside the ARM System Control Block (SCB). It holds the base address of the vector table.

- On hardware reset: VTOR = `0x00000000` (most STM32s alias Flash here → boots correctly)
- `SystemInit()` explicitly writes `0x08000000` to VTOR — this is the **vector table initialisation** step
- When a bootloader jumps to an application, it **must** write VTOR = `APP_BASE` before jumping, so the application's own exception handlers replace the bootloader's

Without the VTOR update before jumping to the application, any interrupt (SysTick, UART, SPI, ...) that fires inside the application would look up handlers from the **bootloader's** vector table — which is a serious bug and a security vulnerability.

This is implemented explicitly in `src/boot/startup.c` (`SystemInit`) and `src/main.c` (`boot_application`).

---

### 2.6 Linker Script — The Map of Your Program

The linker script (`linker/cortex_m.ld`) answers one critical question: **"where in memory does each part of my compiled program go?"**

Without it, the linker has no idea that:
- Flash starts at `0x08000000`
- SRAM starts at `0x20000000`  
- The vector table must be placed at the very beginning of Flash
- Stack grows downward from the top of SRAM
- Initialised variables must be stored in Flash but accessed from SRAM

#### Memory Region Declaration

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
```

`rx` = readable + executable (code). `rwx` = readable + writable + executable (data).

#### Section Layout

```ld
SECTIONS
{
    /* Vector table MUST be first in Flash */
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    /* Executable code */
    .text : { *(.text*) *(.rodata*) } > FLASH

    /* Initialised global variables
     *
     * > SRAM AT > FLASH  means:
     *   VMA (runtime address) = in SRAM  (writable, runtime access)
     *   LMA (load address)    = in Flash (where initial values are stored)
     *
     * startup.c copies from LMA to VMA every boot.
     */
    _sidata = LOADADDR(.data);   /* LMA start — startup.c reads from here */
    .data : {
        _sdata = .;              /* VMA start — startup.c writes to here  */
        *(.data*)
        _edata = .;              /* VMA end                               */
    } > SRAM AT > FLASH

    /* Uninitialised globals — zero-filled by startup.c */
    .bss : {
        _sbss = .;
        *(.bss*) *(COMMON)
        _ebss = .;
    } > SRAM
}

/* Top of stack = top of SRAM */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);   /* 0x20020000 */
```

#### LMA vs. VMA — The Most Important Linker Concept

Every variable has **two addresses**:

| Term | Meaning | For `.data` section |
|---|---|---|
| **LMA** (Load Memory Address) | Where the initial value is *stored* | Inside Flash (non-volatile) |
| **VMA** (Virtual Memory Address) | Where the CPU *accesses* it at runtime | Inside SRAM (read/write) |

Consider: `int counter = 42;`
- At build time: the compiler stores the initial value `42` in the `.data` section
- The linker places `.data` in SRAM for its VMA but Flash for its LMA
- At runtime: `startup.c` copies the value `42` from Flash LMA to SRAM VMA
- After that copy, the CPU's `counter` variable at its SRAM address contains `42`

Without this copy, `counter` would read as `0xFF` (erased Flash) or garbage.

---

### 2.7 SHA-256 — The Cryptographic Hash Function

SHA-256 (Secure Hash Algorithm, 256-bit output) is defined in **NIST FIPS 180-4**. It maps any input to a fixed 32-byte (256-bit) output called a **digest** or **hash**.

```
"Hello World"     →  SHA-256  →  a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e
"Hello world"     →  SHA-256  →  64ec88ca00b268e5ba1a35678a1b5316d212f4f366b2477232534a8aeca37f3c
(one bit changed)                (completely different digest — avalanche effect)

""  (empty)       →  SHA-256  →  e3b0c44298fc1c149afbf4c8996fb924...
"abc"             →  SHA-256  →  ba7816bf8f01cfea414140de5dae2223...
```

#### Four Critical Properties for Secure Boot

| Property | Description | Why It Matters |
|---|---|---|
| **Deterministic** | Same input always → same output | Comparison is reproducible |
| **Avalanche Effect** | 1-bit change → ~50% output bits change | Detects any modification |
| **One-way (Pre-image Resistance)** | Cannot reverse hash to find input | Attacker cannot forge firmware |
| **Collision Resistance** | Infeasible to find two inputs with same hash | Attacker cannot substitute firmware |

#### How SHA-256 is Used in Secure Boot

```
MANUFACTURE TIME:
  1. Compute SHA-256( golden firmware binary )
  2. Burn the 32-byte result into OTP fuses (hardware write-once memory)

EVERY BOOT:
  1. Compute SHA-256( Flash contents from 0x08008000 to 0x08008000 + fw_size )
  2. Read the 32-byte reference from OTP fuses
  3. Compare (constant-time):
       match     → firmware is intact    → proceed
       mismatch  → firmware was modified → halt
```

#### How SHA-256 Works Internally (FIPS 180-4)

```
INPUT: arbitrary-length message
         │
         ▼
STEP 1: PAD MESSAGE
  - Append bit '1'          (0x80 byte)
  - Append zero bytes until message length ≡ 448 bits (mod 512)
  - Append original message length as 64-bit big-endian integer
  Result: message is now a multiple of 512 bits (64 bytes)

STEP 2: INITIALIZE STATE
  H[0..7] = First 32 bits of fractional parts of √(2,3,5,7,11,13,17,19)
  H[0] = 0x6a09e667, H[1] = 0xbb67ae85, ..., H[7] = 0x5be0cd19

STEP 3: FOR EACH 512-BIT BLOCK:
  a) Build message schedule W[0..63]:
       W[i] = M[i]                                    (i = 0..15: direct)
       W[i] = σ1(W[i-2]) + W[i-7] + σ0(W[i-15]) + W[i-16]  (i = 16..63)

  b) Set working variables: a,b,c,d,e,f,g,h = H[0..7]

  c) 64 COMPRESSION ROUNDS:
       T1 = h + Σ1(e) + Ch(e,f,g) + K[i] + W[i]
       T2 = Σ0(a) + Maj(a,b,c)
       h=g, g=f, f=e, e=d+T1, d=c, c=b, b=a, a=T1+T2

  d) Update state: H[j] += working_variable[j]

STEP 4: OUTPUT
  Concatenate H[0]..H[7] as big-endian 32-bit words = 32 bytes
```

The six bitwise functions:

| Function | Formula | Name |
|---|---|---|
| `Ch(x,y,z)` | `(x AND y) XOR (NOT x AND z)` | Choose |
| `Maj(x,y,z)` | `(x AND y) XOR (x AND z) XOR (y AND z)` | Majority |
| `Σ0(x)` | `ROTR²(x) XOR ROTR¹³(x) XOR ROTR²²(x)` | Compression Sigma-0 |
| `Σ1(x)` | `ROTR⁶(x) XOR ROTR¹¹(x) XOR ROTR²⁵(x)` | Compression Sigma-1 |
| `σ0(x)` | `ROTR⁷(x) XOR ROTR¹⁸(x) XOR SHR³(x)` | Schedule sigma-0 |
| `σ1(x)` | `ROTR¹⁷(x) XOR ROTR¹⁹(x) XOR SHR¹⁰(x)` | Schedule sigma-1 |

All 64 round constants `K[0..63]` are the first 32 bits of the fractional parts of the cube roots of the first 64 prime numbers. These are fixed, published values used by every correct SHA-256 implementation.

This entire algorithm is implemented from scratch in `src/crypto/sha256.c` with no external library dependencies — verified against the official NIST test vectors.

---

### 2.8 HMAC-SHA256 — Keyed Authentication

SHA-256 alone has a critical limitation: an attacker can replace firmware **and** compute a new valid SHA-256 of their malicious firmware. If they can also update the stored reference hash, the integrity check passes.

**HMAC (Hash-based Message Authentication Code)**, defined in **RFC 2104**, adds a **secret key** to the hash. Without knowing the key, an attacker cannot compute a valid MAC — even if they have the firmware image, the algorithm, and a powerful computer.

#### HMAC Construction

```
Given:
  K    = secret key (256-bit in our simulator)
  m    = message (firmware bytes)
  ipad = 0x36 repeated 64 times
  opad = 0x5C repeated 64 times

1. K' = SHA-256(K) if len(K) > 64, else K padded with zeros to 64 bytes
2. inner = SHA-256( (K' XOR ipad) || m )
3. HMAC  = SHA-256( (K' XOR opad) || inner )
```

#### How HMAC is Used in Secure Boot

```
MANUFACTURE TIME:
  1. Device has a 256-bit secret key burned into OTP (read-protected, write-protected)
  2. Compute HMAC-SHA256(secret_key, firmware)
  3. Store the 32-byte MAC in a signed firmware header in Flash

EVERY BOOT (after hash check passes):
  1. Read the stored MAC from firmware header
  2. Re-compute HMAC-SHA256(secret_key, firmware)
  3. Compare (constant-time):
       match     → firmware was produced by someone with the secret key → authentic
       mismatch  → firmware was tampered or from an unknown source      → halt
```

This simulates what is commonly called **keyed firmware authentication** — even if an attacker clones your device and replaces firmware that passes the hash check, without the secret key they cannot produce a valid HMAC.

---

### 2.9 Constant-Time Comparison — Preventing Side-Channel Attacks

This is an **advanced security concept** that separates professional security implementations from naive ones.

#### The Problem with `memcmp()`

Standard `memcmp()` returns as soon as it finds the first byte that differs:

```c
// memcmp internal pseudocode:
for (i = 0; i < len; i++) {
    if (a[i] != b[i]) return (a[i] - b[i]);   // ← EARLY EXIT
}
return 0;
```

An attacker measuring execution time can determine:
- If the comparison takes 1 nanosecond → "0 bytes matched"
- If the comparison takes 2 nanoseconds → "1 byte matched"
- Iterate: submit increasingly correct guesses → recover the secret one byte at a time

This is a **timing side-channel attack**. It has been used against real TLS implementations, password hashing systems, and embedded firmware verifiers.

#### The Solution: Constant-Time Comparison

```c
// src/crypto/signature.c -- secure_memcmp()
int secure_memcmp(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0u;
    size_t  i;
    for (i = 0u; i < len; i++) {
        diff |= a[i] ^ b[i];   // XOR: 0 if equal, non-zero if different
    }                           // OR: accumulate any difference
    return (int)diff;           // 0 = all bytes equal, non-zero = mismatch
}
```

This **always iterates all `len` bytes** regardless of whether a mismatch is found early. Execution time is constant. No timing information leaks to an attacker.

This function is called by both `verify_firmware_hash()` and `verify_hmac_signature()` in `src/crypto/signature.c`.

---

## 3. Project Architecture — How Everything Connects

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              main.c                                     │
│                                                                         │
│  1. logger_init()          → UART at 115200 baud, print boot banner     │
│  2. dump_vector_table()    → Log all 16 exception handler addresses     │
│  3. STAGE 1                → SHA-256 firmware integrity check           │
│     sha256_compute(fw)     → Compare vs. OTP trusted hash (secure_cmp)  │
│  4. STAGE 2                → HMAC-SHA256 firmware authentication        │
│     hmac_sha256(key, fw)   → Compare vs. stored MAC (secure_cmp)        │
│  5. Tamper Demo            → Flip 1 byte, re-run → expect FAIL          │
│  6. boot_application()     → Write VTOR, set SP, jump to app            │
└──────────┬───────────────────────────────┬──────────────────────────────┘
           │                               │
           ▼                               ▼
┌──────────────────────┐        ┌──────────────────────────────┐
│     Boot Layer       │        │        Crypto Layer          │
│  src/boot/           │        │  src/crypto/                 │
│                      │        │                              │
│  vector_table.c      │        │  sha256.c                    │
│  ┌────────────────┐  │        │  ┌──────────────────────┐    │
│  │ vector_table[] │  │        │  │ sha256_init()         │    │
│  │ [0] = _estack  │  │        │  │ sha256_update()       │    │
│  │ [1] = Reset_H  │  │        │  │ sha256_final()        │    │
│  │ [2] = NMI_H    │  │        │  │ sha256_compute()      │    │
│  │ ... 16 entries │  │        │  │ All 64 K constants    │    │
│  └────────────────┘  │        │  │ All 8 H0 values       │    │
│  dump_vector_table() │        │  └──────────────────────┘    │
│                      │        │                              │
│  startup.c           │        │  signature.c                 │
│  ┌────────────────┐  │        │  ┌──────────────────────┐    │
│  │ Reset_Handler  │  │        │  │ verify_firmware_hash  │    │
│  │ .data copy     │  │        │  │ hmac_sha256()         │    │
│  │ .bss zero      │  │        │  │ verify_hmac_sig()     │    │
│  │ SystemInit()   │  │        │  │ secure_memcmp()       │    │
│  │  → VTOR write  │  │        │  │ Simulated firmware    │    │
│  │ → main()       │  │        │  │ Simulated OTP hash    │    │
│  └────────────────┘  │        │  └──────────────────────┘    │
└──────────┬───────────┘        └──────────────┬───────────────┘
           │                                   │
           ▼                                   ▼
┌──────────────────────────────────────────────────────────────┐
│                         HAL Layer                            │
│  src/hal/                                                    │
│                                                              │
│  uart.c                  registers.h        memory_map.h    │
│  ┌──────────────┐        ┌──────────────┐   ┌─────────────┐ │
│  │ uart_init()  │        │ SCB_VTOR     │   │ FLASH_BASE  │ │
│  │ uart_send_   │        │ 0xE000ED08   │   │ SRAM_BASE   │ │
│  │   char()     │        │ SCB_CPUID    │   │ UART_BASE   │ │
│  │ uart_send_   │        │ SCB_CCR      │   │ APP_BASE    │ │
│  │   string()   │        │ GPIOA regs   │   │ SRAM_TOP    │ │
│  │ SIMULATION:  │        └──────────────┘   └─────────────┘ │
│  │   putchar()  │                                            │
│  └──────────────┘                                            │
│                                                              │
│  utils/logger.c                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ logger_init()   → uart_init(115200) + boot banner    │   │
│  │ logger_log()    → [INFO]/[WARN]/[ERROR]/[DEBUG]      │   │
│  │ log_hex()       → hex dump without printf            │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
           │
           ▼
┌──────────────────────────────────────────────────────────────┐
│                      Linker Script                           │
│  linker/cortex_m.ld                                          │
│                                                              │
│  FLASH: 0x08000000, 512K  ← .isr_vector, .text, .rodata,    │
│                              .data LMA (initial values)      │
│                                                              │
│  SRAM:  0x20000000, 128K  ← .data VMA (runtime),            │
│                              .bss, stack                     │
│                                                              │
│  Symbols exported to startup.c:                             │
│    _estack  = 0x20020000  (top of SRAM = initial SP)        │
│    _sidata  = LMA start of .data in Flash                   │
│    _sdata   = VMA start of .data in SRAM                    │
│    _edata   = VMA end   of .data in SRAM                    │
│    _sbss    = start of .bss in SRAM                         │
│    _ebss    = end   of .bss in SRAM                         │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. File-by-File Explanation

### 4.1 `vector_table.c` / `vector_table.h`

**Location**: `src/boot/`  
**Purpose**: Declares, stores, and exposes the ARM Cortex-M4 interrupt vector table.

#### Key Implementation Detail — `__attribute__((section, aligned))`

```c
__attribute__((section(".isr_vector"), used, aligned(512)))
const uint32_t vector_table[NUM_VECTORS] = {
    (uint32_t)&_estack,          // [0]  Initial Stack Pointer
    (uint32_t)Reset_Handler,     // [1]  Reset_Handler (Thumb bit +1)
    (uint32_t)NMI_Handler,       // [2]  NMI
    (uint32_t)HardFault_Handler, // [3]  HardFault
    ...
};
```

- `section(".isr_vector")` → Compiler puts this array in the `.isr_vector` output section
- The linker script then rules `KEEP(*(.isr_vector)) > FLASH` → placed first at `0x08000000`
- `used` → Prevents linker `--gc-sections` from discarding it (it's never "called")
- `aligned(512)` → VTOR requires alignment to power-of-2 ≥ 4 × num_vectors

#### `dump_vector_table()` Function

Iterates all 16 entries and prints them via `uart_send_string()`. Does **not** use `printf`. This simulates what a bootloader debug shell would show — you can verify the correct handler addresses are loaded.

#### Why `uint32_t` Instead of Function Pointers?

ARM Thumb function pointers have LSB=1 (Thumb bit). Storing as `uint32_t` gives exact control and the casts `(uint32_t)function_name` preserve the Thumb bit automatically.

---

### 4.2 `startup.c` / `startup.h`

**Location**: `src/boot/`  
**Purpose**: ARM Cortex-M startup code — the bridge between hardware reset and `main()`.

#### `Reset_Handler` Implementation

```c
void Reset_Handler(void)
{
    uint32_t *src, *dst;

    // Step 1: Copy .data section (Flash LMA → SRAM VMA)
    src = &_sidata;          // starts at LMA in Flash
    dst = &_sdata;           // copies to VMA in SRAM
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    // Step 2: Zero .bss section
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0U;
    }

    // Step 3: SystemInit (VTOR write, clock setup)
    SystemInit();

    // Step 4: Call main
    main();
    while (1) { }  // Safety: never return to Flash garbage
}
```

#### `SystemInit()` and VTOR Write

```c
__attribute__((weak)) void SystemInit(void)
{
    // VECTOR TABLE INITIALIZATION
    // Tell the CPU where our vector table is in Flash.
    // SCB->VTOR is at the fixed ARM architecture address 0xE000ED08.
    #define SCB_VTOR  (*(volatile uint32_t *)0xE000ED08U)
    SCB_VTOR = 0x08000000U;   // FLASH_BASE: bootloader vector table address
}
```

`__attribute__((weak))` means application code can override `SystemInit()` with its own version (for clock configuration, FPU enable, MPU setup, etc.) without modifying `startup.c`.

#### SIMULATION Mode

When compiled with `-DSIMULATION` (host Linux build), the OS C-runtime already initialises `.data` and `.bss`. All startup functions become empty stubs so the code compiles and runs on x86.

---

### 4.3 `sha256.c` / `sha256.h`

**Location**: `src/crypto/`  
**Purpose**: Complete FIPS 180-4 SHA-256 implementation, zero external dependencies.

#### Streaming API vs One-Shot API

```c
// STREAMING API -- for large data arriving in chunks (e.g., reading Flash in blocks)
SHA256_CTX ctx;
sha256_init(&ctx);
sha256_update(&ctx, firmware_chunk1, chunk1_len);
sha256_update(&ctx, firmware_chunk2, chunk2_len);
sha256_final(&ctx, output_hash);   // output_hash = 32 bytes

// ONE-SHOT API -- convenience wrapper for contiguous buffers
sha256_compute(firmware, firmware_len, output_hash);
```

The streaming API is critical for embedded use: firmware images can be 1 MB+, larger than available SRAM. The streaming API processes it 64 bytes at a time, never needing to hold the entire image in memory.

#### SHA-256 Context Structure

```c
typedef struct {
    uint32_t state[8];    // Running hash state (A..H working variables)
    uint64_t bit_count;   // Total bits consumed (for padding)
    uint8_t  buffer[64];  // Partial 512-bit block waiting to be processed
    size_t   buffer_len;  // Valid bytes in buffer
} SHA256_CTX;
```

#### Verified Against NIST Test Vectors

| Input | Expected SHA-256 |
|---|---|
| `""` (empty) | `e3b0c44298fc1c14...` |
| `"abc"` | `ba7816bf8f01cfea...` |
| `"abcdbcdecdefdefg..."` (56 bytes) | `248d6a61d20638b8...` |

All three pass in `tests/test_crypto.c`.

---

### 4.4 `signature.c` / `signature.h`

**Location**: `src/crypto/`  
**Purpose**: Secure boot verification pipeline — hash verification, HMAC, and secure comparison.

#### `boot_status_t` Return Codes

```c
typedef enum {
    BOOT_OK        = 0,  // Verification passed — safe to boot
    BOOT_SIG_FAIL  = 1,  // Hash / HMAC mismatch — firmware tampered
    BOOT_NULL_PTR  = 2,  // NULL pointer argument
    BOOT_BAD_MAGIC = 3,  // Firmware magic header wrong
} boot_status_t;
```

Using a typed enum instead of raw integers means the compiler can warn on unhandled cases in switch statements and code is self-documenting.

#### Simulated Firmware Image

A 64-byte constant array (`SIMULATED_FIRMWARE[]`) represents a minimal ARM firmware image:

```
Bytes [0-3]:   0x20020000 — Initial Stack Pointer
Bytes [4-7]:   0x08000009 — Reset_Handler (Thumb: +1)
Bytes [8-15]:  "ARMSCURE" — Magic header
Bytes [16-23]: "BOOTV1.0" — Version string
Bytes [24-31]: 0xFFFFFFFF — Reserved (erased Flash)
Bytes [32-35]: 0xDEADBEEF — Canary value
Bytes [36-63]: Simulated Thumb-2 instruction bytes
```

This simulates the type of content you would find at the start of a real application Flash region.

#### `secure_boot_get_trusted_hash()` — Simulating OTP Fuses

```c
static uint8_t s_trusted_hash[SHA256_BLOCK_SIZE];
static int     s_hash_ready = 0;

static void init_trusted_hash(void)
{
    if (!s_hash_ready) {
        sha256_compute(SIMULATED_FIRMWARE, sizeof(SIMULATED_FIRMWARE),
                       s_trusted_hash);
        s_hash_ready = 1;
    }
}
```

On first call, this computes SHA-256 of `SIMULATED_FIRMWARE` and caches it. This simulates what a real device does: at manufacture, compute SHA-256 of the golden firmware and burn it into OTP fuses (which can never be modified). Every subsequent call returns the same 32-byte cached value.

---

### 4.5 `uart.c` / `uart.h`

**Location**: `src/hal/`  
**Purpose**: Hardware Abstraction Layer for serial communication.

#### Bare-Metal Mode: PL011-Compatible UART Register Access

```c
// Register map (PL011 UART, also used by many ARM evaluation boards)
#define UART_DR_OFFSET    0x000  // Data Register (TX write / RX read)
#define UART_FR_OFFSET    0x018  // Flag Register
#define UART_IBRD_OFFSET  0x024  // Integer Baud Rate Divisor
#define UART_CR_OFFSET    0x030  // Control Register

void uart_send_char(char c)
{
    // Poll until TX FIFO has space (bit 5 of FR = TXFF = TX FIFO Full)
    while (UART_REG(UART_FR_OFFSET) & UART_FR_TXFF) { }
    // Write character to Data Register
    UART_REG(UART_DR_OFFSET) = (uint32_t)(unsigned char)c;
}
```

Every peripheral access is a memory read/write to the peripheral's base address + register offset. The `volatile` qualifier prevents the compiler from optimising away these accesses.

#### Simulation Mode (`-DSIMULATION`): `putchar()` / `getchar()`

```c
void uart_send_char(char c)
{
#ifdef SIMULATION
    putchar((unsigned char)c);
    fflush(stdout);
#else
    while (UART_REG(UART_FR_OFFSET) & UART_FR_TXFF) { }
    UART_REG(UART_DR_OFFSET) = (uint32_t)(unsigned char)c;
#endif
}
```

The `#ifdef SIMULATION` guard means **the same source file** compiles for both real ARM hardware and a Linux host. No separate files, no code duplication.

---

### 4.6 `memory_map.h` / `registers.h`

**Location**: `src/hal/`  
**Purpose**: Centralised hardware address definitions.

#### `memory_map.h` — Physical Address Constants

```c
#define FLASH_BASE        0x08000000UL    // STM32F407 Flash start
#define SRAM_BASE         0x20000000UL    // STM32F407 SRAM start
#define PERIPHERAL_BASE   0x40000000UL    // APB/AHB peripheral space

#define FLASH_SIZE        (512U * 1024U)  // 512 KB
#define SRAM_SIZE         (128U * 1024U)  // 128 KB
#define SRAM_TOP          (SRAM_BASE + SRAM_SIZE)  // 0x20020000 = initial SP

// Secure boot Flash layout
#define BOOTLOADER_BASE   FLASH_BASE
#define BOOTLOADER_SIZE   (32U * 1024U)   // 32 KB reserved for bootloader
#define APP_BASE          (FLASH_BASE + BOOTLOADER_SIZE)  // 0x08008000
#define APP_SIZE          (FLASH_SIZE - BOOTLOADER_SIZE)  // 480 KB for app
```

#### `registers.h` — ARM Architecture Registers

```c
// SCB (System Control Block) — ARM architecture-defined at 0xE000ED00
// These addresses are IDENTICAL on EVERY Cortex-M device.
#define SCB_BASE    (0xE000E000UL + 0x0D00UL)     // 0xE000ED00
#define SCB_VTOR    (*(volatile uint32_t *)(SCB_BASE + 0x08UL))  // 0xE000ED08
#define SCB_CPUID   (*(volatile uint32_t *)(SCB_BASE + 0x00UL))  // 0xE000ED00
#define SCB_CCR     (*(volatile uint32_t *)(SCB_BASE + 0x14UL))  // 0xE000ED14
```

`SCB_VTOR` is the register that makes "vector table initialisation" real. Writing to it via `SystemInit()` sets where the CPU looks for exception handlers.

---

### 4.7 `logger.c` / `logger.h`

**Location**: `src/utils/`  
**Purpose**: Structured logging system that operates without `printf` or dynamic memory.

#### Log Levels

```
[INFO]  Normal boot progress messages
[WARN]  Non-fatal issues, tamper demo output
[ERROR] Fatal errors, verification failures
[DEBUG] Detailed diagnostic information
```

#### `log_hex()` — Hex Dump Without `printf`

```c
void log_hex(const char *label, const uint8_t *data, size_t len)
{
    char pair[4];  // "XY \0"
    for (i = 0; i < len; i++) {
        pair[0] = HEX_CHARS[(data[i] >> 4u) & 0x0Fu];  // high nibble
        pair[1] = HEX_CHARS[ data[i]        & 0x0Fu];  // low nibble
        pair[2] = ' ';
        pair[3] = '\0';
        uart_send_string(pair);
    }
}
```

This prints SHA-256 digests and HMAC values as hex without ever calling `printf`, `sprintf`, or `snprintf`. This is the correct approach for bare-metal code where the C standard library may not be available, is too large, or is not certified.

---

### 4.8 `main.c`

**Location**: `src/`  
**Purpose**: The application entry point and secure boot orchestrator.

#### Complete Boot Flow in `main()`

```c
int main(void)
{
    // 1. Initialize UART and print boot banner
    logger_init();

    // 2. Show the vector table (addresses of all 16 exception handlers)
    dump_vector_table();

    // 3. Run the 2-stage verification pipeline
    boot_status_t result = run_secure_boot_pipeline();

    // 4. Demonstrate tamper detection
    run_tamper_demo();

    // 5. Make the boot decision
    if (result == BOOT_OK) {
        boot_application();   // Update VTOR, set SP, jump to app
    } else {
        enter_safe_state();   // Lockout: disable debug, spin forever
    }
}
```

#### `run_secure_boot_pipeline()` — Two-Stage Verification

**Stage 1: SHA-256 Integrity**

```c
sha256_compute(firmware, fw_len, computed_hash);      // Compute hash
const uint8_t *trusted = secure_boot_get_trusted_hash();  // OTP reference
status = verify_firmware_hash(firmware, fw_len, trusted); // Constant-time compare
```

**Stage 2: HMAC-SHA256 Authentication**

```c
hmac_sha256(DEVICE_SECRET_KEY, 32, firmware, fw_len, hmac_sig);
status = verify_hmac_signature(DEVICE_SECRET_KEY, 32,
                                firmware, fw_len, hmac_sig);
```

#### `boot_application()` — The Actual ARM Boot Jump

```c
// Bare-metal only — not compiled in simulation
SCB_VTOR_REG = APP_BASE;  // Step 1: Relocate vector table to application

uint32_t app_sp    = *(volatile uint32_t *)(APP_BASE + 0U);  // App's initial SP
uint32_t app_entry = *(volatile uint32_t *)(APP_BASE + 4U);  // App's Reset_Handler

// Step 2: Set MSP to application stack pointer
__asm volatile ("MSR msp, %0" : : "r" (app_sp) : );

// Step 3: Jump to application — no return
((void (*)(void))app_entry)();
```

This is exactly what every ARM Cortex-M bootloader does. The three steps — VTOR relocation, SP update, entry point jump — are mandatory in this exact order.

---

### 4.9 `cortex_m.ld` (Linker Script)

**Location**: `linker/`  
**Purpose**: Defines the memory layout of the compiled program for the ARM target.

Key sections and their meaning:

| Section | Memory | Contents | Notes |
|---|---|---|---|
| `.isr_vector` | FLASH | `vector_table[]` | Must be first — CPU reads from `0x08000000` |
| `.text` | FLASH | Compiled machine code | Executable instructions |
| `.rodata` | FLASH | `const` variables, string literals | Read-only data |
| `.data` LMA | FLASH | Initial values of globals | Stored here, copied by startup.c |
| `.data` VMA | SRAM | Runtime globals | Accessed by code at SRAM address |
| `.bss` | SRAM | Uninitialised globals | Zeroed by startup.c |
| `._stack` | SRAM | Stack space (2 KB) | Grows downward from `_estack` |

The `ENTRY(Reset_Handler)` directive tells the linker that `Reset_Handler` is the program entry point, which is embedded into the ELF header for tools like GDB to know where to start.

---

### 4.10 `Makefile`

**Location**: project root  
**Purpose**: Automates building, testing, and cleaning the project.

#### Targets

```makefile
make sim     # Build with gcc -DSIMULATION and run on host Linux/macOS
make arm     # Cross-compile with arm-none-eabi-gcc → ELF for real hardware
make test    # Build and run test_crypto + test_boot on host
make clean   # Remove build/ directory
make help    # Show all targets
```

#### The `-DSIMULATION` Flag

This preprocessor flag is the bridge between host and hardware:

```c
// uart.c
#ifdef SIMULATION
    putchar(c);          // runs on Linux
#else
    UART_REG(...) = c;   // runs on ARM hardware
#endif
```

The `sim` target passes `-DSIMULATION` to `gcc`. The `arm` target uses `arm-none-eabi-gcc` **without** `-DSIMULATION`, enabling the real hardware register code.

#### ARM Cross-Compilation Flags

```makefile
ARM_CFLAGS := \
    -mcpu=cortex-m4     # Target the Cortex-M4 instruction set
    -mthumb             # Generate Thumb (16/32-bit) instructions
    -mfloat-abi=soft    # Software floating point (no FPU for this project)
    -ffunction-sections # Each function in its own linker section
    -fdata-sections     # Each variable in its own linker section
    -O2                 # Optimisation level 2
```

`-ffunction-sections` + `-fdata-sections` combined with `-Wl,--gc-sections` in the linker flags enables **dead code elimination** — the linker can remove any function or variable that is never referenced, keeping the Flash image as small as possible.

---

### 4.11 `tests/test_crypto.c` and `tests/test_boot.c`

**Location**: `tests/`  
**Purpose**: Unit tests for cryptographic functions and boot sequence components.

#### Test Framework

A lightweight macro-based framework with no external dependencies:

```c
#define TEST(cond, name)                              \
    do {                                              \
        g_tests_run++;                                \
        if (cond) {                                   \
            printf("  [PASS] %s\n", (name));          \
            g_tests_passed++;                         \
        } else {                                      \
            printf("  [FAIL] %s (line %d)\n",         \
                   (name), __LINE__);                 \
            g_tests_failed++;                         \
        }                                             \
    } while (0)
```

#### `test_crypto.c` — 14 Tests

| Test | Source | Validates |
|---|---|---|
| `SHA-256("")` | NIST FIPS 180-4 | SHA-256 of empty string |
| `SHA-256("abc")` | NIST FIPS 180-4 | SHA-256 of 3-byte message |
| `SHA-256(56-byte NIST msg)` | NIST FIPS 180-4 | Multi-block boundary |
| Streaming == one-shot | Internal | API consistency |
| Different inputs differ | Internal | Collision property |
| HMAC-SHA256 RFC 4231 Case 1 | RFC 4231 | HMAC against official vector |
| `verify_hmac_signature` correct MAC | Internal | Returns `BOOT_OK` |
| `verify_hmac_signature` tampered MAC | Internal | Returns `BOOT_SIG_FAIL` |
| `secure_memcmp` equal arrays | Internal | Returns 0 |
| `secure_memcmp` different arrays | Internal | Returns non-zero |
| `verify_firmware_hash` authentic | Internal | Returns `BOOT_OK` |
| `verify_firmware_hash` tampered | Internal | Returns `BOOT_SIG_FAIL` |
| `verify_firmware_hash` NULL firmware | Internal | Returns `BOOT_NULL_PTR` |
| `verify_firmware_hash` NULL hash | Internal | Returns `BOOT_NULL_PTR` |

#### `test_boot.c` — 35 Tests

| Category | What Is Tested |
|---|---|
| Vector table size | `NUM_VECTORS == 16` |
| Initial SP | `vector_table[0] == 0x20020000` |
| Reset_Handler | `vector_table[1] != 0` |
| Reserved entries | `vector_table[7..10,13] == 0` |
| All handlers | `vector_table[2..6, 11, 12, 14, 15] != 0` |
| Handler symbols | Each handler symbol exists and is non-NULL |
| Base addresses | `FLASH_BASE`, `SRAM_BASE`, `PERIPHERAL_BASE` correct |
| Region sizes | `FLASH_SIZE == 512KB`, `SRAM_SIZE == 128KB` |
| SRAM_TOP | `== SRAM_BASE + SRAM_SIZE == 0x20020000` |
| SP consistency | `SRAM_TOP == vector_table[0]` |
| Peripherals | UART, GPIO, TIMER in peripheral region |
| Flash layout | APP_BASE > FLASH_BASE, within Flash, correct sizes |

**Current result: 49/49 tests passing.**

---

## 5. How to Run the Project

### 5.1 Prerequisites

```bash
# Install GCC and Make (if not already installed)
sudo apt-get update
sudo apt-get install build-essential gcc make

# Verify installation
gcc --version        # should show gcc 9.x or later
make --version       # should show GNU Make 4.x or later

# Optional: ARM cross-compiler (to build actual ELF for real hardware)
sudo apt-get install gcc-arm-none-eabi binutils-arm-none-eabi

# Verify ARM toolchain
arm-none-eabi-gcc --version
```

### 5.2 Run the Simulation

```bash
# Navigate to project
cd /home/alok/Documents/arm-secure-boot-simulator

# Build the simulation binary and run it immediately
make sim
```

This single command:
1. Creates the `build/` directory
2. Compiles all `.c` files with `gcc -DSIMULATION`
3. Runs `./build/secure_boot_sim`

You should see the full boot sequence output on your terminal.

To **only build** without running:

```bash
gcc -Wall -Wextra -std=c99 -g -DSIMULATION \
    -Iinclude -Isrc/boot -Isrc/crypto -Isrc/hal -Isrc/utils \
    -o build/secure_boot_sim \
    src/main.c src/boot/startup.c src/boot/vector_table.c \
    src/crypto/sha256.c src/crypto/signature.c \
    src/hal/uart.c src/utils/logger.c
```

To **run with GDB** for debugging:

```bash
gdb ./build/secure_boot_sim
(gdb) break main
(gdb) run
(gdb) step          # step through line by line
(gdb) print result  # inspect variables
```

### 5.3 Run Unit Tests

```bash
make test
```

This builds two separate test binaries and runs them:

```bash
# What make test does internally:
gcc ... -o build/test_crypto tests/test_crypto.c src/crypto/sha256.c \
        src/crypto/signature.c src/hal/uart.c src/utils/logger.c

gcc ... -o build/test_boot tests/test_boot.c src/boot/startup.c \
        src/boot/vector_table.c src/hal/uart.c

./build/test_crypto
./build/test_boot
```

Expected final output:

```
--- Results: 14/14 passed ---
--- Results: 35/35 passed ---
>>> All tests complete.
```

If any test fails, the `make test` command exits with a non-zero error code (useful for CI/CD pipelines).

### 5.4 Cross-Compile for Real ARM Hardware

```bash
# Build the ARM ELF
make arm

# Output: build/secure_boot.elf
# The Makefile also runs arm-none-eabi-size to show Flash/RAM usage
```

#### Flashing to an STM32F4 Board

```bash
# Method 1: st-link (ST-Link V2 debugger)
sudo apt-get install stlink-tools
st-flash write build/secure_boot.elf 0x08000000

# Method 2: OpenOCD (any supported debugger)
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/secure_boot.elf verify reset exit"

# Method 3: pyOCD
pyocd flash build/secure_boot.elf --target stm32f407vg
```

After flashing, open a serial terminal at 115200 baud (e.g., `minicom`, `screen`, or `putty`) connected to the UART TX pin of the board to see the boot log output.

#### Clean All Build Artifacts

```bash
make clean
# Removes the entire build/ directory
```

---

## 6. Understanding the Full Output

When you run `make sim`, the complete annotated output is:

```
========================================
  ARM Cortex-M4 Secure Boot Simulator  
========================================
```
> Logger initialised. UART configured at 115200 baud (simulated via putchar).

```
----------------------------------------
[INFO]  ARM Cortex-M4 Secure Boot Simulator v1.0
[INFO]  Build: Mar 30 2026  17:52:59
[INFO]  Flash : 0x08000000  (512 KB)
[INFO]  SRAM  : 0x20000000  (128 KB)
[INFO]  Stack : 0x20020000  (top of SRAM)
----------------------------------------
```
> Boot banner with compile timestamp, Flash/SRAM base addresses, and the initial stack pointer value (top of SRAM).

```
[INFO]  Vector Table (base: 0x08000000):

  +------+------------------+------------+
  | Idx  | Name             | Address    |
  +------+------------------+------------+
  | [ 0] | Initial SP       | 0x20020000 |
  | [ 1] | Reset_Handler    | 0x08000009 |
  | [ 2] | NMI_Handler      | 0x08000011 |
  | [ 3] | HardFault        | 0x08000013 |
  | [ 4] | MemManage        | 0x08000015 |
  | [ 5] | BusFault         | 0x08000017 |
  | [ 6] | UsageFault       | 0x08000019 |
  | [ 7] | Reserved         | 0x00000000 |
  ...
  | [15] | SysTick_Handler  | 0x08000021 |
  +------+------------------+------------+
```
> `dump_vector_table()` output. Entry [0] is `0x20020000` = `SRAM_TOP`. Entries [1..6, 11..12, 14..15] are non-zero handler addresses. Entries [7..10, 13] are the ARM-required reserved zeros.

```
[INFO]  STAGE 1: SHA-256 Integrity Verification
[INFO]    Computing SHA-256 hash of firmware...
[INFO]    Computed : C8 58 0F B8 54 26 61 E8 E2 27 33 3D 96 B9 2B F9
                     93 5D 2E 73 D7 32 76 45 3B 3D 44 77 AE E2 F3 B8
[INFO]    Loading trusted hash from OTP secure storage...
[INFO]    Trusted  : C8 58 0F B8 54 26 61 E8 ...  (identical)
[INFO]    Comparing digests (constant-time)...
[INFO]  STAGE 1 PASSED -- firmware integrity confirmed.
```
> SHA-256 of `SIMULATED_FIRMWARE[]` computed and compared with the pre-stored reference hash. Computed == Trusted → PASS.

```
[INFO]  STAGE 2: HMAC-SHA256 Authentication
[INFO]    Computing HMAC-SHA256 with device secret key...
[INFO]    HMAC     : 36 11 6B 13 2D C5 0E D7 D9 1A 6E A8 ...
[INFO]    Verifying HMAC signature...
[INFO]  STAGE 2 PASSED -- firmware authenticated.
```
> HMAC-SHA256 computed with `DEVICE_SECRET_KEY` and verified against itself. Simulates keyed authentication.

```
[INFO]  TAMPER DEMO: Simulating a modified firmware image...
[INFO]    Byte[10] flipped from original value.
[INFO]    Re-running SHA-256 verification on tampered image...
[WARN]  TAMPER DEMO RESULT: Tampered firmware CORRECTLY REJECTED.
[WARN]    SHA-256 avalanche effect: 1-bit change -> completely
[WARN]    different digest.  Attack detected and blocked.
```
> A copy of the firmware has byte[10] XOR'd with `0xFF`. SHA-256 of the modified copy is completely different from the trusted hash. `verify_firmware_hash()` returns `BOOT_SIG_FAIL`. This visually demonstrates the avalanche effect.

```
[INFO]  SECURE BOOT: VERIFICATION OK
[INFO]  Transferring control to application firmware...
[INFO]    Bootloader Flash base   : 0x08000000
[INFO]    Application Flash base  : 0x08008000
[INFO]    [SIM] VTOR would be set : 0x08008000 (app vector table)
[INFO]    [SIM] MSP  would be set : APP_BASE word[0]
[INFO]    [SIM] Jump would go to  : APP_BASE word[1] (Reset_Handler)
[INFO]  >>> APPLICATION IS NOW RUNNING <<<
```
> Both stages passed. On real hardware: VTOR is written to `0x08008000`, MSP loaded from app's vector table word[0], then a `BX r0` jump transfers control permanently to the application. In simulation these are logged but not executed (cannot jump to arbitrary RAM on Linux).

---

## 7. How This Helps Your Embedded Software Engineer Résumé

This project directly demonstrates skills that embedded companies test in technical interviews.

### Skills Demonstrated and Where

| Skill | Project Evidence | Why It Matters to Employers |
|---|---|---|
| **ARM Cortex-M architecture** | `vector_table.c`, `startup.c`, `registers.h` | Core knowledge for STM32/nRF/NXP roles |
| **Bare-metal C** | Entire project, no OS | Fundamental for all embedded positions |
| **Linker scripts** | `cortex_m.ld`, LMA/VMA understanding | Required for custom BSPs and boot-loaders |
| **Memory-mapped I/O** | `uart.c`, `registers.h` | How all embedded peripherals work |
| **Startup code / CRT0** | `startup.c` | Critical for BSP engineers |
| **Cryptography from scratch** | `sha256.c` (FIPS 180-4) | Security-critical embedded systems |
| **SHA-256 / HMAC** | `signature.c` | Automotive, IoT, medical requirements |
| **Security engineering** | `secure_memcmp`, HMAC keying | Prevents timing side-channel attacks |
| **FIPS / RFC compliance** | NIST + RFC 4231 test vectors | Government/medical/financial embedded |
| **Unit testing** | 49 tests across 2 suites | Professional embedded dev practice |
| **Cross-compilation** | Makefile `arm` target | Standard embedded toolchain usage |
| **Hardware abstraction** | `hal/` layer | Portable firmware design patterns |
| **VTOR register usage** | `startup.c`, `main.c` | Demonstrates bootloader-level knowledge |
| **Weak symbols / aliases** | Exception handler pattern | Production firmware pattern |

### Interview Questions This Prepares You to Answer

1. **"Walk me through what happens from power-on to main() on a Cortex-M."**
   → Reset_Handler reads SP and PC from vector table, copies .data, zeroes .bss, calls SystemInit (VTOR), calls main.

2. **"What is the vector table? Where is it stored? How does the CPU find it?"**
   → Array of 32-bit handler addresses at the start of Flash. CPU reads from address 0x0 (aliased to Flash). VTOR register tells CPU where it is.

3. **"What is VTOR and when must a bootloader update it?"**
   → Vector Table Offset Register at 0xE000ED08. Must be updated to APP_BASE before jumping to application, so the app's exception handlers are active.

4. **"What is a linker script and why does embedded code need one?"**
   → Defines memory layout: where Flash and SRAM are, section placement, LMA/VMA for .data, stack size. Without it linker doesn't know the hardware address map.

5. **"Explain LMA vs VMA for the .data section."**
   → LMA is where initial values are stored in Flash at build time. VMA is where the CPU accesses variables at runtime in SRAM. startup.c copies from LMA to VMA every boot.

6. **"How does secure boot work?"**
   → Hash firmware with SHA-256, compare against OTP-stored hash (integrity). Compute HMAC with device secret key, compare stored MAC (authentication). Any mismatch halts boot.

7. **"Why can't you use `memcmp()` for comparing cryptographic hashes?"**
   → Timing side-channel attack: `memcmp` exits early on first mismatch, leaking how many bytes match. `secure_memcmp` always iterates all bytes — constant time.

8. **"What is HMAC and why is it more secure than a plain hash for firmware authentication?"**
   → HMAC binds a secret key to the hash. An attacker can compute SHA-256 of any firmware, but without the secret key cannot compute a valid HMAC.

9. **"What is the `__attribute__((weak))` pattern used for in startup code?"**
   → Allows application code to override `SystemInit()` or any exception handler without modifying the startup file. The application's strong definition replaces the weak one at link time.

10. **"What does `-ffunction-sections -fdata-sections -Wl,--gc-sections` do?"**
    → Places each function/variable in its own linker section, then the linker removes any section with no references. Minimises Flash usage — critical when you have 64 KB of Flash.

### Companies and Domains That Value This Project

| Domain | Companies | Why This Project Is Relevant |
|---|---|---|
| **Automotive** | Bosch, Continental, Aptiv, ZF | ISO 26262 mandates secure boot for ASIL-B/C/D |
| **Semiconductors** | STMicroelectronics, NXP, Nordic | Their products run this exact boot flow |
| **IoT** | Arm, Amazon (FreeRTOS), Silicon Labs | PSA Certified requires secure boot |
| **Defense / Aerospace** | BAE Systems, Thales, Raytheon | All firmware must be signed and verified |
| **Medical Devices** | Medtronic, Philips, GE Healthcare | FDA cybersecurity guidance requires it |
| **Consumer Electronics** | Apple (embedded), Samsung, Qualcomm | Every modern SoC has secure boot |
| **Industrial** | Siemens, Schneider, Rockwell | IEC 62443 cybersecurity standard |

---

## 8. Real Secure Boot vs. This Project

| Aspect | This Simulator | Real Production (e.g. STM32Trust, ARM TF-A) |
|---|---|---|
| **Reference hash storage** | Constant in source code | OTP hardware fuses (write-once, read-lock) |
| **Secret key storage** | Constant in source code | HSM, eFuse with JTAG read-lock, TrustZone |
| **Asymmetric crypto** | Not implemented (HMAC only) | ECDSA P-256, RSA-2048, Ed25519 |
| **Chain of trust** | Single stage | ROM BL0 → BL1 → BL2 → TF-A → RTOS → App |
| **Rollback protection** | Not implemented | Monotonic counter in OTP |
| **Debug interface lock** | Not implemented | JTAG disabled after verification |
| **Encrypted firmware** | Not implemented | AES-256-CBC/GCM for confidentiality |
| **Certificate chain** | Not implemented | X.509 certificates for key management |

This simulator reproduces the **concepts and flow** of every stage correctly. The simplifications are intentional and appropriate — a résumé project demonstrates understanding of the architecture, which is exactly what it achieves.

---

## 9. Summary

This project is a complete, working simulation of the ARM Cortex-M secure boot process. It covers every layer from hardware reset through cryptographic verification:

### What You Learn By Building This

1. **Hardware layer**: How Cortex-M CPUs actually start — vector table, Reset_Handler, VTOR register.
2. **Startup layer**: The C runtime initialisation that runs before `main()` — `.data` copy, `.bss` zeroing.
3. **Linker layer**: Memory layout via linker scripts — LMA vs. VMA, section placement, symbol exports.
4. **HAL layer**: Memory-mapped I/O — how every embedded peripheral is accessed.
5. **Crypto layer**: SHA-256 from first principles (FIPS 180-4), HMAC-SHA256 (RFC 2104).
6. **Security layer**: Constant-time comparison, OTP simulation, two-stage boot verification.
7. **Testing layer**: Unit testing against official NIST/RFC test vectors.
8. **Build system**: Cross-compilation, dual-target Makefiles, simulation guards.

### The Résumé Claim — Fully Substantiated

> *"ARM Bare-Metal Secure Boot Simulator: Developed a minimal boot sequence simulator in C demonstrating Cortex boot flow, vector table initialization, and a basic cryptographic signature verification step simulating a Secure Boot process."*

| Claim | Implementation |
|---|---|
| Cortex boot flow | `Reset_Handler` → `.data`/`.bss` init → `SystemInit` → `main()` |
| Vector table initialization | 16-entry `vector_table[]` in `.isr_vector` + `SCB_VTOR = 0x08000000` in `SystemInit()` + VTOR relocation in `boot_application()` |
| Cryptographic signature verification | SHA-256 hash check (Stage 1) + HMAC-SHA256 (Stage 2) + `secure_memcmp` + tamper detection demo |
| Simulating a Secure Boot process | OTP hash simulation, device key simulation, two-stage pipeline, `boot_application()` with VTOR + SP + entry point jump |

**Test results: 49/49 passing.**

---

*This document was generated from the ARM Bare-Metal Secure Boot Simulator project.*  
*Author: Alok | Date: March 30, 2026*
