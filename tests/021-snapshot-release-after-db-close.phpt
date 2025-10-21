--TEST--
leveldb - snapshot release after database close (potential crash)
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-snapshot-db-close.test-db';
$db = new LevelDB($leveldb_path);

// Add some data
$db->set('key1', 'value1');
$db->set('key2', 'value2');

// Create a snapshot
$snapshot = $db->getSnapshot();

// Close the database (internal handle becomes NULL)
// Note: close() is deprecated but sets db->db to NULL internally
unset($db);

// Try to release the snapshot after DB is closed
// This should either work gracefully or throw a proper exception
// Without proper checks, this could crash due to NULL pointer dereference
try {
    $snapshot->release();
    echo "Snapshot released successfully\n";
} catch (Exception $e) {
    echo "Exception caught: " . $e->getMessage() . "\n";
}

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-snapshot-db-close.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
%s
