## 1. Posting File Format Implementation

- [x] 1.1 Define posting file magic numbers and constants in `ivf_shard.h`
- [x] 1.2 Add `Save` method to `IndexIVFShard` class with binary format
- [x] 1.3 Add `Load` method to `IndexIVFShard` class with validation
- [x] 1.4 Implement checksum calculation for posting data
- [x] 1.5 Add atomic write helper (write to temp, fsync, rename)

## 2. DistributedIndexIVF Persistence

- [x] 2.1 Add `save_index` method to `DistributedIndexIVF` header
- [x] 2.2 Implement `save_index` to persist centroids, metadata, and shard postings
- [x] 2.3 Add `load_index` method implementation to restore all components
- [x] 2.4 Add dirty flag tracking to `DistributedIndexIVF`
- [x] 2.5 Update `add_vectors` and `build_index` to set dirty flag

## 3. Metadata Persistence

- [x] 3.1 Define index metadata JSON structure (dimension, nlist, nprobe, shard_count)
- [x] 3.2 Add `SaveMetadata` helper method
- [x] 3.3 Add `LoadMetadata` helper method with validation
- [x] 3.4 Ensure metadata file uses atomic write pattern

## 4. Index Recovery and Validation

- [x] 4.1 Add magic number validation on load
- [x] 4.2 Add version compatibility check on load
- [x] 4.3 Add checksum validation for all loaded files
- [x] 4.4 Add dimension consistency validation across shards
- [x] 4.5 Return clear error messages for all validation failures

## 5. Auto-Persistence Manager

- [x] 5.1 Create `IndexPersistenceManager` class with background thread
- [x] 5.2 Add configurable auto-save interval (default 60 seconds)
- [x] 5.3 Implement auto-save loop with dirty flag check
- [x] 5.4 Add graceful shutdown hook to save pending changes
- [x] 5.5 Add start/stop methods for the persistence manager

## 6. Read-Write Lock Integration

- [x] 6.1 Add `std::shared_mutex` to `DistributedIndexIVF` for concurrent access
- [x] 6.2 Update search methods to acquire shared lock
- [x] 6.3 Update save methods to acquire shared lock (non-blocking)
- [x] 6.4 Update write methods to acquire exclusive lock
- [x] 6.5 Ensure auto-save uses shared lock to not block queries

## 7. Configuration Support

- [x] 7.1 Add persistence config options to `Config` class
- [x] 7.2 Add `persistence.enabled` config option
- [x] 7.3 Add `persistence.save_interval_seconds` config option
- [x] 7.4 Add `persistence.index_path` config option
- [x] 7.5 Update server startup to initialize persistence manager

## 8. Testing

- [x] 8.1 Write unit tests for `IndexIVFShard` Save/Load
- [x] 8.2 Write unit tests for `DistributedIndexIVF` save/load round-trip
- [x] 8.3 Write tests for corruption detection and error handling
- [x] 8.4 Write tests for auto-save functionality
- [x] 8.5 Write integration tests for graceful shutdown save
