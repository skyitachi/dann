## ADDED Requirements

### Requirement: Save distributed index to disk
The system SHALL provide a `save_index` method for `DistributedIndexIVF` that persists all index data including centroids, postings, and metadata to disk.

#### Scenario: Save index successfully
- **WHEN** `save_index("/path/to/index")` is called on a trained distributed index
- **THEN** the system writes index files to the specified path atomically
- **AND** the operation returns `true` on success

#### Scenario: Save untrained index
- **WHEN** `save_index` is called on an untrained index
- **THEN** the system returns `false` without writing any files

### Requirement: Load distributed index from disk
The system SHALL provide a `load_index` method for `DistributedIndexIVF` that restores index state from persisted files.

#### Scenario: Load index successfully
- **WHEN** `load_index("/path/to/index")` is called on valid index files
- **THEN** the system restores centroids, postings, and routing information
- **AND** the operation returns `true` on success

#### Scenario: Load non-existent index
- **WHEN** `load_index` is called with a path that does not exist
- **THEN** the system returns `false` without modifying index state

### Requirement: Persist shard postings
The system SHALL persist inverted list postings for each `IndexIVFShard` to separate files in a `shards/` subdirectory.

#### Scenario: Save shard postings
- **WHEN** saving a distributed index with multiple shards
- **THEN** each shard's postings are written to `shards/shard_<id>.posting`
- **AND** each file contains centroid IDs, vector IDs, and vector data

#### Scenario: Load shard postings
- **WHEN** loading a distributed index
- **THEN** all shard postings are restored from their respective files
- **AND** search operations work correctly after load

### Requirement: Atomic file writes
The system SHALL use atomic write operations to prevent data corruption during saves.

#### Scenario: Atomic save operation
- **WHEN** saving an index file
- **THEN** the system writes to a temporary file first
- **AND** renames the temp file to the target file atomically
- **AND** on crash during write, either the old or new file exists intact

### Requirement: Persist index metadata
The system SHALL persist index metadata in JSON format including dimension, index type, nlist, nprobe, and shard count.

#### Scenario: Save and load metadata
- **WHEN** saving and loading an index
- **THEN** metadata including dimension, nlist, nprobe are preserved
- **AND** the loaded index has the same configuration as the saved index
