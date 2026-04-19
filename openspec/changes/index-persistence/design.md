## Context

DANN is a distributed vector database based on FAISS. Currently, the `DistributedIndexIVF` class manages:
- Global centroids (cluster centers)
- Shard postings (inverted lists in `IndexIVFShard`)
- Main index storage (centroid-to-node routing)

The `MainIndexStorage` class already provides Save/Load functionality for centroids and routing information. However, `DistributedIndexIVF::load_index()` returns `false` and postings are never persisted. On node restart, all data must be rebuilt from scratch.

**Key constraints:**
- Index data can be large (millions of vectors, GBs of memory)
- File I/O should be atomic to prevent corruption
- Loading should validate data integrity
- Auto-save should not block query operations

## Goals / Non-Goals

**Goals:**
- Implement complete persistence for `DistributedIndexIVF` (centroids + postings + metadata)
- Atomic write operations with temporary file + rename pattern
- Checksum validation on load to detect corruption
- Background auto-save thread with configurable interval
- Recovery mechanism for incomplete/corrupted files

**Non-Goals:**
- Distributed consensus for persistence (each node persists locally)
- Real-time WAL (write-ahead logging) for every operation
- Cross-version compatibility (current version only)
- Compression (future enhancement)

## Decisions

### 1. File Format Design

**Decision:** Use a multi-file persistence strategy with separate files for each component:
- `<index_name>.meta` - Index metadata (JSON format)
- `<index_name>.centroids` - Global centroids (binary, existing MainIndexStorage format)
- `<index_name>.shards/` - Directory containing per-shard postings
  - `shard_<id>.posting` - Postings for shard `<id>`

**Rationale:** 
- Separates concerns and enables incremental updates
- Allows parallel loading of shards
- Easier debugging and manual inspection
- Alternative considered: Single monolithic file. Rejected due to large file handling complexity and lack of incremental update support.

### 2. Atomic Write Pattern

**Decision:** Use write-to-temp + atomic rename pattern:
1. Write to `<file>.tmp`
2. fsync the temp file
3. Rename `<file>.tmp` → `<file>` (atomic on POSIX)

**Rationale:**
- Guarantees consistency even on crash during write
- Either old or new file exists, never partial data
- Alternative considered: Journaling/WAL. Rejected as overkill for current requirements.

### 3. Posting File Format

**Decision:** Binary format with header:
```
[HEADER]
  magic: uint32 (0x504F5354 = "POST")
  version: uint32 (1)
  shard_id: uint32
  dimension: uint32
  num_centroids: uint32
  num_vectors: uint64
  checksum: uint32

[DATA]
  For each centroid:
    centroid_id: int64
    num_vectors: uint32
    vector_ids: int64[num_vectors]
    vectors: float[num_vectors * dimension]

[FOOTER]
  magic: uint32 (0x54534F50 = "TSOP")
```

**Rationale:**
- Simple and efficient for sequential reads/writes
- Checksum enables corruption detection
- Footer magic validates complete file read

### 4. Auto-Save Strategy

**Decision:** Background thread with configurable interval (default: 60 seconds)
- Thread wakes up, checks if dirty flag is set
- If dirty, acquires read lock and saves
- Does not block query operations (uses shared lock)

**Rationale:**
- Balanced approach for durability vs performance
- Alternative considered: Save on every write. Rejected due to high I/O overhead.

### 5. Recovery Strategy

**Decision:** Multi-level recovery:
1. Check magic numbers and version
2. Validate checksums
3. Verify dimension consistency
4. If any shard file corrupted, mark index as needing rebuild

**Rationale:**
- Fails fast on obviously corrupt data
- Provides clear error messages for debugging

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Large index save takes long time | Use background thread; don't block queries |
| Crash during save leaves partial file | Temp file + atomic rename pattern |
| Disk full during save | Check return values; log error; retry later |
| Checksum collision (unlikely) | Use 32-bit Jenkins hash; collision probability ~1/4B |
| Auto-save conflicts with manual save | Use mutex to serialize save operations |
| Loading corrupted index | Validate all headers/checksums; return clear error |
