# Bug Report and Test Cases for PHP-LevelDB

## Summary

This document describes potential bugs found during code review of the PHP-LevelDB extension and the test cases created to reproduce them.

## Bugs Identified

### 1. Snapshot Release After Database Close (CRITICAL)

**Location**: `leveldb.c:1555-1572` (`LevelDBSnapshot::release()`)

**Issue**: When `LevelDBSnapshot::release()` is called after the database has been closed, there's a potential NULL pointer dereference.

**Code**:
```c
if (intern->db == NULL || intern->snapshot == NULL) {
    return;
}

leveldb_object *db = intern->db;
leveldb_release_snapshot(db->db, intern->snapshot);  // db->db could be NULL!
```

**Problem**: The check verifies that `intern->db` is not NULL, but doesn't check if `intern->db->db` (the actual LevelDB handle) is NULL. If the database was closed after the snapshot was created, `db->db` will be NULL, causing `leveldb_release_snapshot` to receive a NULL pointer.

**Test Case**: `tests/021-snapshot-release-after-db-close.phpt`

**Severity**: HIGH - Potential crash/segmentation fault

---

### 2. Snapshot Used Across Different Database Connections

**Location**: `leveldb.c:542-562` (`php_leveldb_get_readoptions()`)

**Issue**: Snapshots created on one database connection can potentially be used with a different database connection without proper validation.

**Problem**: When a snapshot is passed in read options, the code checks if it's a valid `LevelDBSnapshot` object and if it hasn't been released, but doesn't verify that the snapshot belongs to the same database connection.

**Test Case**: `tests/024-snapshot-read-after-db-close.phpt`

**Severity**: MEDIUM - Could lead to undefined behavior or crashes

---

### 3. Iterator Memory Leak on Database Close

**Location**: `leveldb.c:1144-1148` (`php_leveldb_check_iter_db_not_closed()`)

**Issue**: When a database is closed while iterators exist, the iterator is set to NULL but not properly destroyed.

**Code**:
```c
if (intern->db->db == NULL) {
    intern->iterator = NULL;  // Set to NULL but not destroyed
    zend_throw_exception(...);
    return FAILURE;
}
```

**Problem**: Setting `intern->iterator = NULL` without calling `leveldb_iter_destroy()` first causes a memory leak.

**Test Case**: `tests/023-iterator-after-db-close.phpt`

**Severity**: MEDIUM - Memory leak

---

### 4. Empty Key/Value Edge Cases

**Location**: Various functions (`LevelDB::set()`, `LevelDB::get()`, etc.)

**Issue**: Edge cases with empty keys and empty values may not be properly tested.

**Problem**: While LevelDB supports empty keys and values, the extension may not handle all edge cases correctly, especially in iterators and comparisons.

**Test Case**: `tests/022-empty-key-value.phpt`

**Severity**: LOW - Edge case behavior

---

### 5. Binary Data with NULL Bytes

**Location**: All string handling functions

**Issue**: Keys and values containing NULL bytes (`\0`) may not be handled correctly due to C string assumptions.

**Problem**: If string lengths are not properly tracked and NULL-terminated string functions are used instead of length-based functions, binary data will be truncated.

**Test Case**: `tests/027-binary-data-handling.phpt`

**Severity**: MEDIUM - Data corruption for binary keys/values

---

### 6. Iterator on Empty Database

**Location**: `leveldb.c:1289-1324` (`LevelDBIterator::__construct()`)

**Issue**: Iterator behavior on an empty database may have edge cases.

**Code**:
```c
intern->iterator = leveldb_create_iterator(db_obj->db, readoptions);
leveldb_iter_seek_to_first(intern->iterator);  // On empty DB, this makes iterator invalid
```

**Problem**: On an empty database, seeking to first makes the iterator immediately invalid, which may cause unexpected behavior in operations like `key()` and `current()`.

**Test Case**: `tests/026-iterator-on-empty-db.phpt`

**Severity**: LOW - Edge case behavior

---

### 7. Write Batch Edge Cases

**Location**: `leveldb.c:1051-1128` (LevelDBWriteBatch methods)

**Issue**: Various edge cases with write batches:
- Empty batch writes
- Deleting non-existent keys
- Reusing batches after clear()
- Empty keys/values in batches

**Test Case**: `tests/025-writebatch-empty-operations.phpt`

**Severity**: LOW - Edge case behavior

---

### 8. Large Key/Value Handling

**Location**: All data access functions

**Issue**: Very large keys and values may expose memory or performance issues.

**Problem**: While LevelDB supports large values, the PHP extension may have issues with:
- Memory allocation for very large values
- Performance degradation
- String length handling (PHP string length limits)

**Test Case**: `tests/028-large-key-value.phpt`

**Severity**: LOW - Performance and limits testing

---

## Test Cases Created

All test cases follow the existing `.phpt` test format used by the project:

1. **021-snapshot-release-after-db-close.phpt** - Tests snapshot release after database closure
2. **022-empty-key-value.phpt** - Tests empty key and value handling
3. **023-iterator-after-db-close.phpt** - Tests iterator usage after database closure
4. **024-snapshot-read-after-db-close.phpt** - Tests snapshot read after database reconnection
5. **025-writebatch-empty-operations.phpt** - Tests write batch edge cases
6. **026-iterator-on-empty-db.phpt** - Tests iterator on empty database
7. **027-binary-data-handling.phpt** - Tests binary data with NULL bytes
8. **028-large-key-value.phpt** - Tests large keys and values (1MB, 10MB)

## Running the Tests

To run these tests, first build the extension:

```bash
phpize
./configure --with-leveldb
make
make test TESTS="tests/021-*.phpt tests/022-*.phpt tests/023-*.phpt tests/024-*.phpt tests/025-*.phpt tests/026-*.phpt tests/027-*.phpt tests/028-*.phpt"
```

Or run all tests:

```bash
make test
```

## Recommended Fixes

### Fix for Bug #1 (Snapshot Release After DB Close)

```c
PHP_METHOD(LevelDBSnapshot, release)
{
    leveldb_snapshot_object *intern;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    intern = LEVELDB_SNAPSHOT_OBJ_FROM_ZV(getThis());

    if (intern->db == NULL || intern->snapshot == NULL) {
        return;
    }

    leveldb_object *db = intern->db;

    // FIX: Check if the database handle is still valid
    if (db->db != NULL) {
        leveldb_release_snapshot(db->db, intern->snapshot);
    }
    // else: database was closed, snapshot is already implicitly released

    intern->snapshot = NULL;
    intern->db = NULL;
}
```

### Fix for Bug #3 (Iterator Memory Leak)

```c
static int php_leveldb_check_iter_db_not_closed(leveldb_iterator_object *intern) {
    if (intern->iterator == NULL) {
        zend_throw_exception(php_leveldb_ce_LevelDBException, "Iterator has been destroyed", PHP_LEVELDB_ERROR_ITERATOR_CLOSED);
        return FAILURE;
    }
    if (intern->db->db == NULL) {
        // FIX: Properly destroy the iterator before setting to NULL
        if (intern->iterator != NULL) {
            leveldb_iter_destroy(intern->iterator);
            intern->iterator = NULL;
        }
        zend_throw_exception(php_leveldb_ce_LevelDBException, "Can not iterate on closed db", PHP_LEVELDB_ERROR_DB_CLOSED);
        return FAILURE;
    }

    return SUCCESS;
}
```

## Priority

1. **HIGH**: Bug #1 (Snapshot release after DB close) - Potential crash
2. **MEDIUM**: Bug #2 (Snapshot cross-database use) - Undefined behavior
3. **MEDIUM**: Bug #3 (Iterator memory leak) - Memory leak
4. **MEDIUM**: Bug #5 (Binary data handling) - Data corruption
5. **LOW**: Other edge cases - Robustness improvements
