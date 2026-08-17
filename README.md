# NanoGPU — a from-scratch educational GPGPU


NanoGPU is a small, fully open-source GPGPU built in Verilog, designed to be
read, understood, and rebuilt by anyone — not just run as a black box. It is
built in public, in phases, with every architectural decision explained in
[`docs/architecture.md`](docs/architecture.md) as it's made.

This project has two goals, and both shape how it's built:

1. **A complete, working, professionally verified core.** Every module has a
   testbench. Every testbench has edge cases. Every test run produces a
   waveform you can inspect. Nothing is "left as an exercise."
2. **A teaching resource.** If you're new to GPU architecture, you should be
   able to read this repo top to bottom and come out understanding *why* GPUs
   are built the way they are — not just copy RTL.

## Why build a GPU instead of another CPU?

A CPU core is optimized for **latency** — get one instruction stream through as fast as
possible, with branch prediction, out-of-order execution, deep caches.

A GPU is optimized for **throughput** — run the *same* instruction across
*hundreds or thousands* of data elements at once, and hide memory latency by
having so much parallel work available that the hardware is never idle
waiting on anything. That single reframing — "many threads, one instruction,
at a time" — is called **SIMT (Single Instruction, Multiple Threads)**, and
it's the foundation everything else in this repo is built on.

Full glossary and concept primer: [`docs/architecture.md`](docs/architecture.md).

## Project status

🚧 **Phase 3: RTL datapath (Verilog).** The C++ functional simulator is
complete and verified (Phase 2): machine state, decoder, and the SIMT
executor are all tested, and a real 8-wide vector-add kernel runs correctly
end to end — proof the ISA and execution model work before a single line
of Verilog exists. Now translating that proven design into synthesizable
RTL.

## Roadmap

| Phase | What | Status |
|---|---|---|
| 1 | Architecture & ISA definition | ✅ done |
| 2 | Functional simulator (C++ model of the ISA) | ✅ done |
| 3 | RTL: core datapath (fetch/decode/scheduler/lanes/memory) | 🚧 in progress |
| 4 | Unit testbenches (ALU, regfile, scheduler, memory) | ⬜ not started |
| 5 | Integration testbenches + real kernels (vector add, reduction) | ⬜ not started |
| 6 | v2: multiple warps, latency hiding, divergence handling | ⬜ not started |
| 7 | v3: multi-core, memory coalescing, FPGA synthesis | ⬜ not started |

Each phase gets its own section in `docs/architecture.md`, written *as we
build it* — decisions, rationale, and the alternatives we didn't pick.

## Repo structure

```
nanogpu/
├── README.md              you are here
├── docs/
│   ├── architecture.md    living design doc: concepts, decisions, rationale
│   ├── isa.md              full v1 instruction set reference
│   └── waveforms/         exported waveform screenshots referenced in docs
├── rtl/                   synthesizable Verilog source (Phase 3+)
├── tb/                    RTL testbenches (Phase 4+)
└── sim/                   C++ functional simulator (Phase 2)
    ├── include/           headers (state.h, decoder.h, ...)
    ├── src/                implementations (decoder.cpp, ...)
    └── test/               unit tests for the simulator itself
```

## Toolchain

**Simulator (Phase 2, current):**
- **C++17**, compiled with `g++` — no build system yet beyond direct
  `g++` invocations; a Makefile lands once there are enough files to
  justify one.

**RTL (Phase 3+):**
- **Verilog** (IEEE 1364), kept portable — no vendor-specific syntax where avoidable.
- **Icarus Verilog** (`iverilog`) for simulation — free, open source, one `apt install` away.
- **GTKWave** for waveform viewing.
- FPGA synthesis target is decided in Phase 7, once the design is verified in simulation.

### ⚠️ Windows setup gotcha: use MSYS2 UCRT64, not MINGW64

If you're on Windows via [MSYS2](https://www.msys2.org/), install and use
the **`MSYS2 UCRT64`** shell — not `MSYS2 MinGW x64` (MINGW64). MSYS2 ships
several parallel environments, each with its own `bin/` folder and its own
builds of every shared library. If you launch the wrong one, `g++` compiles
will fail with a symptom that's genuinely confusing to debug: `g++` reports
no error at all, just a bare nonzero exit code, because it's silently
handing off to a compiler stage (`cc1plus.exe`) that fails to load correct
DLLs at the OS level, before it ever gets a chance to print anything.

Symptom to watch for: `which g++` resolving to `/mingw64/bin/g++` instead
of `/ucrt64/bin/g++` (or `/c/msys64/ucrt64/bin/g++`). If you're already in
the wrong shell, `export PATH="/c/msys64/ucrt64/bin:$PATH"` fixes it for
that session — but switching to the correct shortcut is the real fix.

## Building & running

**Simulator tests** (from the repo root, in a correctly-configured shell — see above):

```bash
make test
```

This builds `decoder_test`, `exec_test`, and `vector_add_test` into `build/`
and runs all three, stopping on the first failure. `make` alone builds
without running; `make clean` removes `build/`.

To build/run a single test binary directly (e.g. while iterating on one
piece), the underlying `g++` invocations follow this pattern:

```bash
g++ -std=c++17 -Wall -Wextra -Isim/include sim/src/decoder.cpp sim/test/decoder_test.cpp -o decoder_test.exe
```

**RTL simulation:** *(coming in Phase 3, once there's RTL to build.)*

## License

TBD — will be an OSI-approved permissive license (likely MIT or Apache-2.0)
before the first RTL commit, so the open-source community can actually use
this.