## ADDED Requirements

### Requirement: Background auto-save thread
The system SHALL provide a background thread that periodically saves the index if changes have been made.

#### Scenario: Auto-save triggers after interval
- **WHEN** the index has been modified and the auto-save interval has elapsed
- **THEN** the background thread saves the index to the configured path
- **AND** the save does not block query operations

#### Scenario: Auto-save skips when no changes
- **WHEN** the auto-save interval elapses but no changes have been made
- **THEN** the background thread skips the save operation

### Requirement: Configurable save interval
The system SHALL allow configuration of the auto-save interval.

#### Scenario: Custom save interval
- **WHEN** auto-save is configured with a custom interval (e.g., 30 seconds)
- **THEN** the background thread saves at the specified interval

#### Scenario: Disable auto-save
- **WHEN** auto-save interval is set to 0
- **THEN** the background thread does not perform automatic saves

### Requirement: Graceful shutdown saves index
The system SHALL save the index on graceful shutdown if changes are pending.

#### Scenario: Save on shutdown
- **WHEN** the system receives a shutdown signal and has unsaved changes
- **THEN** the system saves the index before exiting

#### Scenario: No save on clean shutdown
- **WHEN** the system receives a shutdown signal and no changes are pending
- **THEN** the system exits without performing a save

### Requirement: Non-blocking persistence
The system SHALL allow query operations to proceed during save operations using appropriate locking.

#### Scenario: Query during save
- **WHEN** a save operation is in progress
- **THEN** query operations can still execute using a read lock
- **AND** write operations wait for the save to complete

### Requirement: Persistence state tracking
The system SHALL track whether the index has unsaved changes.

#### Scenario: Dirty flag after modification
- **WHEN** vectors are added or removed from the index
- **THEN** the dirty flag is set to indicate unsaved changes

#### Scenario: Dirty flag cleared after save
- **WHEN** a save operation completes successfully
- **THEN** the dirty flag is cleared
