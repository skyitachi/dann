## ADDED Requirements

### Requirement: Validate file integrity on load
The system SHALL validate file integrity when loading an index, including magic numbers, version, and checksums.

#### Scenario: Load valid index file
- **WHEN** loading an index with valid magic numbers and checksums
- **THEN** the system accepts the file and loads successfully

#### Scenario: Detect corrupted header
- **WHEN** loading an index file with invalid magic number
- **THEN** the system rejects the file with a corruption error
- **AND** returns `false` without modifying index state

#### Scenario: Detect corrupted data
- **WHEN** loading an index file where data checksum does not match
- **THEN** the system rejects the file with a checksum error
- **AND** returns `false` without modifying index state

### Requirement: Handle partial writes
The system SHALL detect and handle partial writes from interrupted save operations.

#### Scenario: Detect incomplete save
- **WHEN** a temporary file exists but no valid index file
- **THEN** the system ignores the temporary file
- **AND** returns an appropriate error on load attempt

### Requirement: Validate dimension consistency
The system SHALL validate that all loaded data has consistent dimensions with the index configuration.

#### Scenario: Dimension mismatch
- **WHEN** loading shard data with dimension different from index dimension
- **THEN** the system rejects the file with a dimension mismatch error
- **AND** returns `false` without modifying index state

### Requirement: Version compatibility check
The system SHALL check file version compatibility on load and reject incompatible versions.

#### Scenario: Load current version
- **WHEN** loading an index file with the current version number
- **THEN** the system loads the file successfully

#### Scenario: Reject incompatible version
- **WHEN** loading an index file with an unsupported version number
- **THEN** the system rejects the file with a version error
- **AND** returns `false` without modifying index state
