# NanoGPU v1 instruction set architecture

This defines the complete v1 ISA: register model, instruction encoding, and
every opcode. Read [`architecture.md`](architecture.md) first for the
reasoning behind these choices — this document is the precise reference,
that one is the "why."

## Register model

Each of the 8 lanes has its own private set of 8 registers (r0–r7). A
register name always means "this lane's copy of that register" — lanes
never share registers with each other.

| Register | Access | Meaning |
|---|---|---|
| `r0` | read-only, hardwired | Always reads as `0`. Writes are silently discarded. |
| `r1`–`r6` | read/write | General-purpose, per-thread. |
| `r7` | read-only, per-lane constant | This lane's thread index within the warp (`0`–`7`). Set by hardware, not software. |

**Why this matters for SIMT:** every lane executes the exact same
instruction every cycle. The *only* thing that can differ between lanes is
the data in their registers and memory. `r7` is what makes per-thread work
possible at all — e.g. a vector-add kernel uses `r7` to compute "my slice of
the array," so 8 lanes running identical code still process 8 different
elements.

## Instruction encoding

Every instruction is a fixed 16-bit word. Bit 0 is always the reserved
predicate bit; bits 15:12 are always the opcode. Everything in between
depends on the format — and the format is chosen per-opcode to spend the
12 middle bits on whatever that instruction actually needs most: a wide
immediate for building constants, a memory offset for indexed array
access, or a third register for ALU ops.

```
 15            12 11    9  8     6  5           1  0
┌────────────────┬────────┬────────┬─────────────┬───┐
│     opcode     │  rd    │  rs1   │   varies    │ P │   R format
└────────────────┴────────┴────────┴─────────────┴───┘
```

| Format | Layout (MSB→LSB) | Used by |
|---|---|---|
| **R** | `opcode[4] · rd[3] · rs1[3] · rs2[3] · reserved[2] · P[1]` | Register-register ALU ops |
| **C** | `opcode[4] · rd[3] · imm[8] · P[1]` | Constant-building ops (no `rs1`/`rs2` — see below) |
| **L** | `opcode[4] · rd[3] · rs1[3] · imm[5] · P[1]` | Loads (indexed by register + offset) |
| **S** | `opcode[4] · rs1[3] · rs2[3] · imm[5] · P[1]` | Stores (no destination register) |
| **Z** | `opcode[4] · rs1[3] · imm[8] · P[1]` | Branches (test one register against zero) |
| **N** | `opcode[4] · unused[11] · P[1]` | No-operand instructions (NOP, HALT, SYNC) |

`P` (bit 0), the **predicate bit**: reserved for Phase 6 (divergence
handling). **v1 hardware always treats every lane as active and ignores
this bit entirely.** It's encoded now so that adding real predication later
doesn't require re-encoding every instruction already written — see
`architecture.md` for the full rationale.

`imm` fields are two's-complement signed, except where a specific opcode
says otherwise (`IADDI`'s immediate is zero-extended, not sign-extended —
see its entry below).

## Opcode table

| Opcode | Mnemonic | Format | Operation | Notes |
|---|---|---|---|---|
| `0x0` | `NOP` | N | No operation | |
| `0x1` | `ADD` | R | `rd = rs1 + rs2` | |
| `0x2` | `SUB` | R | `rd = rs1 - rs2` | |
| `0x3` | `AND` | R | `rd = rs1 & rs2` | |
| `0x4` | `OR` | R | `rd = rs1 \| rs2` | |
| `0x5` | `XOR` | R | `rd = rs1 ^ rs2` | |
| `0x6` | `SLT` | R | `rd = (rs1 < rs2, signed) ? 1 : 0` | for building signed comparisons |
| `0x7` | `SLTU` | R | `rd = (rs1 < rs2, unsigned) ? 1 : 0` | for bounds checks like `tid < length` — lengths are never negative, so unsigned compare is the correct one to reach for |
| `0x8` | `IADDI` | C | `rd = rd + zext(imm8)` | accumulate an 8-bit immediate into `rd` |
| `0x9` | `LUI` | C | `rd = imm8 << 8` | loads immediate into the upper byte, low byte cleared |
| `0xA` | `LOAD` | L | `rd = MEM[rs1 + imm5]` | word-addressed, offset enables indexed array access |
| `0xB` | `STORE` | S | `MEM[rs1 + imm5] = rs2` | `rs1` = base address, `rs2` = data |
| `0xC` | `BZ` | Z | `if (rs1 == 0) PC += imm8` | see divergence warning below |
| `0xD` | `BNZ` | Z | `if (rs1 != 0) PC += imm8` | see divergence warning below |
| `0xE` | `HALT` | N | Stop core execution | Testbenches watch for this to detect kernel completion |
| `0xF` | `SYNC` | N | Reserved — barrier/sync point | **No-op in v1.** Becomes a real multi-warp barrier in Phase 6. Encoded now for the same forward-compatibility reason as the predicate bit. |

### Why no separate unconditional jump

There's no dedicated `JMP` opcode, and that's deliberate rather than an
oversight: since `r0` is hardwired to `0` (see Register model), `BZ r0,
#offset` branches unconditionally — the condition `r0 == 0` is always true.
That gets an unconditional jump "for free" out of an opcode we needed
anyway, leaving all 16 opcode slots doing distinct, necessary work instead
of spending one on something derivable from another.

### Building full 16-bit constants

A single 16-bit instruction can't hold a full 16-bit immediate *and* an
opcode *and* a register field — there isn't room. So constants wider than
the small offsets used elsewhere are built in two instructions, a common
pattern in fixed-width RISC-style ISAs:

```
LUI   r1, #0x12     ; r1 = 0x1200
IADDI r1, #0x34      ; r1 = r1 + 0x34  →  r1 = 0x1234
```

`LUI` plants the high byte and clears the low byte; `IADDI` (zero-extended,
not sign-extended — this matters, since a sign-extending add here would
corrupt the high byte we just set) adds in the low byte.

## Why this looks like a CPU's ISA (and why that's correct)

If you skim the opcode table below, `ADD`, `SUB`, `LOAD`, `BZ` could belong
to almost any small RISC CPU. That's expected, not a sign something's
missing — **the instruction set is not what makes this a GPU. The execution
model around it is.**

A CPU fetches one instruction and executes it once, on one set of
registers. This core fetches one instruction and executes it **8 times
simultaneously** — once per lane, each on its own private register file,
each potentially touching different data via `r7` (thread ID, see below).
`ADD r1, r2, r3` is the exact same instruction word in both worlds; what
differs is that here, one fetch/decode triggers 8 physical ALUs computing 8
independent `r1 = r2 + r3` results in the same cycle, because every lane
has different values sitting in its own `r2`/`r3`/`r7`. Same-looking
instruction, radically different hardware consuming it underneath — that's
[SIMT](architecture.md#glossary), and it's *why* GPU kernels (CUDA/OpenCL)
read like ordinary scalar C rather than looking exotic.

**This also answers a natural follow-up: don't we need separate vector
instructions, the way x86 has AVX?** That's a real and different design
point — SIMD — worth being precise about the distinction:

- **SIMD**: one instruction *explicitly names* a vector op — e.g.
  `vadd v1, v2, v3` operates on all 8 elements of a vector register in one
  instruction. The vector-ness lives in the *encoding*.
- **SIMT (this ISA)**: every instruction is scalar from its own point of
  view — just `rd`, `rs1`, `rs2`, nothing vector-shaped about it. The
  parallelism comes entirely from *replicating the hardware* 8 times and
  feeding all 8 copies the same scalar instruction stream. The vector-ness
  lives in the *hardware*, invisible to the instruction encoding.

Deliberately choosing SIMT over SIMD is why this ISA has no vector opcodes
— it's not a v1 shortcut to add later, it's the architecture. It's also
why writing a NanoGPU kernel is closer to writing one thread's ordinary
logic than to hand-vectorizing code.

## ⚠️ v1 limitation: branches must be warp-uniform

This is the sharpest edge of building a SIMT core *before* divergence
handling exists, and it's worth understanding precisely rather than
discovering it as a mysterious bug.

Because v1 has no per-lane active-mask hardware, **every lane executes
whatever the fetch/decode stage fetches, unconditionally.** A `BZ`/`BNZ`
either redirects the *whole* warp's program counter, or it doesn't — there
is no mechanism for lane 3 to branch while lane 5 falls through.

This means: **v1 kernels are only correct if every lane's branch condition
agrees.** A loop where all 8 threads run the same number of iterations
(the overwhelmingly common case — e.g. "each thread processes exactly one
array element, then done") works perfectly. A kernel where the branch
depends on *per-thread* data (e.g. `if (my_value == 0)` where `my_value`
differs per lane) will produce wrong results in v1, silently, because the
hardware will branch based on whatever lane's comparison "wins" the shared
branch-decision logic — this is exactly the class of bug Phase 6 (real
divergence handling with a per-lane active mask) exists to solve correctly.

The first kernels we write and testbench (Phase 2 onward) are deliberately
chosen to be warp-uniform for this reason.

## Worked encoding example

Assembly: `ADD r1, r2, r3` — one specific 16-bit encoding, opcode `0x1`,
format R, `rd=r1(001)`, `rs1=r2(010)`, `rs2=r3(011)`, reserved bits `00`,
predicate `0`:

```
0001  001  010  011  00  0
opcode  rd  rs1  rs2 rsvd P
```

`0001 001 010 011 00 0` → `0x1298` in hex.

We'll generate encodings like this automatically once the assembler exists
(Phase 2) — this one is worked by hand so the encoding format above has a
concrete anchor.

## Next: functional simulator (Phase 2)

With the ISA fully pinned down, the next step is a C++ functional simulator
that executes this ISA in software: prove the instruction set and
programming model actually work *before* writing RTL against them.