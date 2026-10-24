# fhmap design

## Goals

`fhmap` prioritizes lookup throughput, compact ownership, predictable
allocation behavior, and a small integration surface. It deliberately uses
fixed-size keys and values: that constraint lets the table keep all slot data
inline without allocating or chasing a node per entry.

## Allocation layout

One allocation contains three logical regions:

```text
[ control bytes + 16-byte mirror ][ aligned key slots ][ aligned value slots ]
```

The arrays are structure-of-arrays rather than an array of `{key, value}`
records. Failed lookups touch the small control array and occasional key slots,
without pulling values into cache. Iteration over full entries trades some
locality for this miss behavior.

Every capacity is a power of two and at least 16. Key and value strides are
rounded to the alignments in the configuration. The allocation is internally
overallocated by at most `max(key_align, value_align, 16) - 1` bytes so the
logical base can be aligned without requiring a separate allocation.

## Control bytes

There is one control byte per bucket:

| Byte | Meaning |
|---|---|
| `0x00..0x7f` | Occupied; low seven hash bits are the fingerprint |
| `0x80` | Never occupied / empty |
| `0xfe` | Deleted tombstone |

The first 16 control bytes are mirrored after the main array. A group load can
therefore cross the power-of-two boundary without a branch or split load.

On SSE2 targets, `_mm_cmpeq_epi8` and `_mm_movemask_epi8` test all 16 bytes in a
group. The scalar fallback produces the same 16-bit masks.

## Lookup

The high hash bits choose a starting bucket. A probe examines 16 consecutive
control bytes, then advances by 16 and wraps with a bit mask. For each group:

1. Compare all control bytes to the seven-bit fingerprint.
2. Run full key equality only for matching fingerprints.
3. If no candidate matches, stop when the group contains an empty byte.
   Tombstones do not stop a lookup.

The empty-byte test happens after candidate equality, so a successful lookup
does not compute an empty mask. This is safe because insertion always uses the
first available slot: an equal key cannot lie beyond an empty bucket along its
probe sequence. Fingerprint collisions elsewhere in the group still require
full key equality.

At normal load factors, a missing lookup commonly ends after one group and no
full key comparisons. Fingerprints have a 1/128 accidental match probability
before accounting for the number of occupied bytes examined.

Linear group probing is intentionally simple. It offers sequential control-byte
loads and avoids pointer chasing. A maximum load factor of 7/8 leaves enough
empty buckets to bound expected probe lengths under a well-distributed hash.

## Insertion and deletion

Insertion first performs a lookup so updating an existing key cannot trigger an
unnecessary growth. A new entry uses the earliest tombstone before the first
empty bucket in its probe sequence, or that empty bucket when no tombstone was
seen. Both available control-byte values have their high bit set. After the
lookup proves the key is absent, insertion uses one high-bit mask per group and
stops at its first set bit. It does not need separate empty and deleted masks
or a second scan past a reusable tombstone. Rehashing uses the same slot search.

Deletion converts an occupied control byte to a tombstone. Converting it
directly to empty could make a later key in the same probe chain unreachable.
The table tracks tombstones separately:

- insertions reuse them;
- a table with more than 25% tombstones and more tombstones than live entries is
  rebuilt before the next new insertion;
- when occupied buckets reach the growth limit, `fhmap` rebuilds at the current
  capacity if live entries still fit, otherwise it doubles;
- `fhmap_compact` gives the caller an explicit shrink-and-clean operation.

Rehashing is transactional with respect to allocation failure: the old table
remains intact until the replacement allocation is fully populated.

## Hashing

The default byte hash processes unaligned input safely with `memcpy`, mixes
16-byte chunks with a folded 64x64-bit multiplication, handles the final bytes
without reading past the key, and applies a 64-bit avalanche. GCC and Clang on
x86-64 lower the folded multiplication to the native wide-multiply sequence.

It is a fast non-cryptographic hash. A random seed is obtained from Linux
`getrandom`; a clock/address/process fallback exists for unusually early or
restricted environments. Custom callbacks can replace both hash and equality.

## Complexity

Expected complexities assume a suitable hash distribution:

| Operation | Expected | Worst case |
|---|---:|---:|
| Get / contains | O(1) | O(n) |
| Insert / update | Amortized O(1) | O(n) |
| Remove | O(1) | O(n) |
| Reserve / compact | O(n) | O(n) |
| Full iteration | O(capacity) | O(capacity) |

Worst-case O(n) lookup is inherent to this open-addressed design when many keys
collide. Random seeding prevents an attacker from cheaply reusing a collision
set prepared for a known seed, but the default hash is not a cryptographic
denial-of-service defense.

## Intentional non-features

- No implicit ownership for pointers, strings, or nested resources
- No variable-length inline keys
- No destructors
- No internal locking
- No stable iteration order
- No stable element addresses across structural mutations

These policies keep the hot path and object model small. Ownership, arenas,
locking, or serialization can be layered around the raw API according to the
application.
