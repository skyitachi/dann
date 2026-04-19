## Why

Currently, DANN lacks comprehensive index persistence. The `DistributedIndexIVF::load_index` returns `false` (not implemented), meaning all index data is lost on node restart. This makes the system unsuitable for production deployments requiring durability. When a node restarts, it must rebuild the entire index from scratch, which is time-consuming for large datasets and impacts service availability.

## What Changes

- Add complete persistence support for `DistributedIndexIVF` including centroids, postings, and metadata
- Implement `save_index` method for distributed index with atomic write guarantees
- Implement `load_index` method for distributed index with validation and recovery
- Add incremental persistence support for postings (inverted lists) in `IndexIVFShard`
- Introduce index snapshot mechanism with checksum validation
- Add background persistence thread for periodic auto-save
- Add recovery mechanism for corrupted index files

## Capabilities

### New Capabilities

- `index-persistence`: Core persistence capability for distributed IVF index, including save/load for centroids, postings, and metadata
- `index-recovery`: Recovery and validation mechanism for corrupted or incomplete index files
- `auto-persistence`: Background auto-save capability with configurable intervals

### Modified Capabilities

- None (this is a new feature, not modifying existing spec-level behavior)

## Impact

- **Core files**: `distributed_index_ivf.cpp`, `ivf_shard.cpp`, `main_index_storage.cpp`
- **New files**: `index_persistence_manager.cpp`, `index_snapshot.cpp`
- **Header files**: `distributed_index_ivf.h`, `ivf_shard.h`, `index_persistence_manager.h`
- **Configuration**: Add persistence-related config options (save_interval, snapshot_path, etc.)
- **Dependencies**: No new external dependencies
- **API**: Add `save_index()`, `load_index()`, `create_snapshot()`, `restore_snapshot()` methods
