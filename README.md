# NanoGPU — a from-scratch educational GPGPU

> Placeholder name — rename freely once it feels like "yours."

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

A CPU core is optimized for **latency** — get one instruction stream through as fast as possible, with branch prediction, out-of-order execution, deep caches.

A GPU is optimized for **throughput** — run the *same* instruction across
*hundreds or thousands* of data elements at once, and hide memory latency by
having so much parallel work available that the hardware is never idle
waiting on anything. That single reframing — "many threads, one instruction,
at a time" — is called **SIMT (Single Instruction, Multiple Threads)**, and
it's the foundation everything else in this repo is built on.

Full glossary and concept primer: [`docs/architecture.md`](docs/architecture.md).

## Project status

🚧 **Phase 1: Architecture definition.** No RTL yet — we're locking the
datapath and ISA before writing a single line of Verilog, the same way you'd
plan a building before pouring concrete.

## Roadmap

| Phase | What | Status |
|---|---|---|
| 1 | Architecture & ISA definition | 🚧 in progress |
| 2 | Functional simulator (Python/C model of the ISA) | ⬜ not started |
| 3 | RTL: core datapath (fetch/decode/scheduler/lanes/memory) | ⬜ not started |
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
├── rtl/                   synthesizable Verilog source
├── tb/                    testbenches (one per RTL module, plus integration)
└── sim/                   simulation scripts, Makefiles, compiled kernels
```

## Toolchain

- **Verilog** (IEEE 1364), kept portable — no vendor-specific syntax where avoidable.
- **Icarus Verilog** (`iverilog`) for simulation — free, open source, one `apt install` away.
- **GTKWave** for waveform viewing.
- FPGA synthesis target is decided in Phase 7, once the design is verified in simulation.

## Building & running

*(Coming in Phase 3, once there's RTL to build.)*

## License

TBD — will be an OSI-approved permissive license (likely MIT or Apache-2.0)
before the first RTL commit, so the open-source community can actually use
this.