# NanoGPU architecture & design decisions

This document is the living record of *how* NanoGPU works and *why* it's
built the way it is. It grows one phase at a time, in the same order the
project is built, so you can follow the reasoning instead of just the result.

If you're new to GPU architecture, read the [Glossary](#glossary) first —
every term used later is defined there in plain language before it's used.

---

## Glossary

Terms are introduced here before they're used anywhere else in this repo.

- **CPU-style ("MIMD-ish") execution**: each core runs its own independent
  instruction stream. Optimized for doing *one* complex task quickly.
- **SIMD (Single Instruction, Multiple Data)**: one instruction operates on a
  small fixed vector of data in one lane of hardware (e.g. a CPU's AVX unit
  adding 8 numbers in one instruction). The parallelism is inside a single
  instruction.
- **SIMT (Single Instruction, Multiple Threads)**: the GPU model. Many
  independent *threads* (each with its own registers and program counter
  state) execute the *same instruction stream* in lockstep, one instruction
  at a time, across many parallel lanes of hardware. It looks like SIMD from
  the hardware's point of view, but looks like ordinary independent threads
  from the programmer's point of view — that illusion is the whole trick of
  GPU programming models like CUDA/OpenCL.
- **Warp** (NVIDIA's term) / **wavefront** (AMD's term): a fixed-size group
  of threads that physically execute together, one instruction per cycle, in
  lockstep. NanoGPU uses "warp." This is a *hardware* grouping — the
  programmer usually just writes "one thread's" code, and the hardware
  batches threads into warps automatically.
- **Lane**: one physical execution unit (ALU + its slice of the register
  file) inside a warp's execution hardware. Warp size = number of lanes that
  execute together.
- **Divergence**: what happens when threads *within the same warp* disagree
  on which branch of an `if`/`while` to take. Because all threads in a warp
  must execute the same instruction each cycle, the hardware has to run
  *both* paths (masking off the threads that shouldn't be affected each
  time) rather than truly branching independently. This is why data-dependent
  branching is expensive on GPUs and why divergence handling is one of the
  more interesting hardware problems in this project (Phase 6).
- **Latency hiding**: a GPU's answer to memory being slow. Instead of
  building an CPU-style out-of-order engine to keep one thread busy while it
  waits on memory, a GPU core keeps *many warps* resident and simply
  switches to a different warp the instant the current one stalls (e.g. on a
  memory load). With enough warps in flight, the core is never idle. This
  needs multiple warps per core (Phase 6) to work at all — with only one
  warp (our v1), a memory stall just stalls the whole core.
- **Coalescing**: when multiple threads in a warp access nearby memory
  addresses in the same instruction, the memory system combines them into
  one wide transaction instead of N separate ones. Big real-world performance
  lever; not implemented until the memory hierarchy work in Phase 7.
- **Kernel**: the program a GPU thread runs — conceptually "the function each
  thread executes," launched across many threads at once.

---

## Phase 1 — architecture & ISA definition

### Scope decision: build minimal, design to scale

We are deliberately **not** building multi-warp latency hiding, divergence
handling, multi-core, or memory coalescing in v1. Real GPUs need all of
these to be *fast*, but a v1 that tries to build all of them at once is a v1
that never gets finished or verified properly.

Instead, v1's job is to prove the **core SIMT idea** works end to end,
correctly, with full test coverage — one core, one warp, lockstep lane
execution, real kernels running on it. Every later phase adds exactly one
new architectural concept on top of a working, tested foundation. 


### Decision: warp size = 8 lanes

**Why 8:** Large enough to actually demonstrate parallel SIMT execution (2–4
threads doesn't meaningfully show off the architecture). Small enough that
a waveform of all 8 lanes' registers is still readable by eye in GTKWave
during debugging — this matters a lot for a project meant to teach.
Power-of-2, so warp-related indexing/addressing stays simple, and scaling to
16 or 32 later (real GPUs typically use 32) is a parameter change, not a
redesign.

**Alternatives considered:** 4 lanes (too small to feel like a GPU), 32
lanes (matches real hardware, but far harder to debug by hand while still
learning the architecture — saved for a later phase once the design is
proven).

### Decision: 16-bit data width, 16-bit fixed instruction width

**Why:** Keeps registers and memory contents easy to read directly off a
waveform (a 16-bit hex value is far more scannable mid-debug than a 32-bit
one), fixed-width instructions
are simple to decode and simple to learn — no variable-length instruction
parsing to explain. The width is a Verilog parameter, not a hardcoded
constant, so widening to 32-bit later is a config change.

**Alternatives considered:** 32-bit (industry standard, but adds
complexity without adding *architectural* teaching value at this stage — the
SIMT concepts are identical at 16-bit or 32-bit).

### Decision: 8 registers per thread, addressed with 3 bits — but not all general-purpose

**Why 8:** Enough headroom for real kernels (thread-index calculation, a
loop counter, an accumulator, a couple of temporaries) without a register
file large enough to make the datapath diagrams and waveforms hard to
follow while you're still learning to read them. 3-bit addressing also
keeps the instruction encoding compact (see [`docs/isa.md`](isa.md)).

**Refinement (made while designing the ISA):** two of the eight are special,
following the RISC-V convention of a hardwired zero register:
- `r0` is hardwired to the constant `0` (writes are silently ignored). This
  turns out to simplify a surprising number of things — comparisons against
  zero, "discard this result," etc. — at zero hardware cost.
- `r7` is **read-only and unique per lane**: it holds that lane's thread
  index within the warp (0–7). This is the mechanism that lets all 8 lanes
  execute the *identical* instruction stream (true SIMT) while still
  operating on *different* data — e.g. `array[r7]` gives every lane its own
  array element. Without this, SIMT would have no way to differentiate
  threads at all.

That leaves `r1`–`r6` (6 registers) as true general-purpose, read-write,
per-thread storage. Full details in [`docs/isa.md`](isa.md).

### Decision: single core, single warp in v1 (no latency hiding yet)

**Why:** Latency hiding (see Glossary) only makes sense once there are
multiple warps to switch between. Building it before the single-warp
datapath is solid and tested would mean debugging two new hard problems at
once. v1 will stall on memory access — that's expected and will be called
out explicitly in the testbench documentation, not hidden.

### Decision: ISA reserves a predicate/mask field now, hardware ignores it until Phase 6

**Why:** Divergence handling (see Glossary) changes *how instructions are
interpreted*, not just the datapath — every instruction needs a way to say
"which lanes does this apply to." If we don't reserve that field in the
instruction encoding now, adding it later means changing the encoding of
every existing instruction and breaking every program already written
against it. Reserving the field costs us nothing today and saves a painful
rewrite later.

### Decision: single unified memory space, word-addressable, no cache

**Why:** Real GPUs split memory into global/shared/local tiers because
that's what makes them *fast*. But a memory hierarchy adds significant
verification surface (coherency, bank conflicts, coalescing) that isn't
needed to prove the SIMT execution model works. v1 gets one flat memory
space so the datapath and testbenches can focus on correctness first.

### Decision: toolchain = Verilog + Icarus Verilog + GTKWave

**Why:** Fully open source with zero licensing friction — anyone can `git
clone` this repo and run the full test suite with a single `apt install
iverilog gtkwave`, no vendor account, no license server. This matters both
for the "open source community" goal and the "beginners can follow along"
goal. FPGA synthesis (which does need to think about a specific target
device/toolchain) is deliberately deferred to Phase 7, after the design is
proven correct in simulation — synthesizing an unverified design just wastes
FPGA build cycles debugging things a testbench would have caught in seconds.

---

## v1 datapath overview

```
Instruction memory
        │
        ▼
┌───────────────────────────────────────────────────┐
│  SIMT core (v1) — 1 core, 1 warp, 8 lanes          │
│                                                     │
│  ┌───────────────┐   ┌───────────────┐  ┌────────┐ │
│  │ Fetch & decode │──▶│  Lane array   │─▶│ Memory │ │
│  │ Warp scheduler │   │ 8x ALU + regs │  │  I/F   │ │
│  └───────────────┘   └───────────────┘  └────────┘ │
└───────────────────────────────────────────────────┘
                                                │
                                                ▼
                                          Data memory
```

- **Fetch & decode / warp scheduler**: fetches one instruction per cycle and
  broadcasts it to all 8 lanes. In v1 there's only one warp, so "scheduling"
  is trivial — it becomes a real decision once Phase 6 adds multiple warps.
- **Lane array**: 8 lanes, each with its own 8-register file, executing the
  same decoded instruction on its own data every cycle.
- **Memory interface**: routes load/store requests from the lanes to data
  memory. No coalescing yet — each lane's memory access is handled
  independently.

## Next up

Phase 1 continues with the actual **ISA definition** — the opcode list,
encoding format, and the reserved predicate field — before we write the
functional simulator in Phase 2.