--TEST--
leveldb - snapshot keeps database reference alive
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-snapshot-read-closed.test-db';
$db = new LevelDB($leveldb_path);

// Add some data
$db->set('key1', 'value1');
$db->set('key2', 'value2');

// Create a snapshot
$snapshot = $db->getSnapshot();

// This should work - reading with snapshot while DB is open
$result = $db->get('key1', array('snapshot' => $snapshot));
var_dump($result);

// Unset the database variable
// NOTE: The database remains open because the snapshot holds a reference
unset($db);

echo "Database variable unset, but snapshot keeps DB alive\n";

// Release the snapshot
$snapshot->release();

echo "Snapshot released\n";

// Unset the snapshot object
unset($snapshot);

echo "Snapshot object unset\n";

// Now try to reopen - this will FAIL due to the refcount bug
// The database is still locked because release() didn't decrement refcount
try {
    $db2 = new LevelDB($leveldb_path);
    echo "Successfully reopened (bug is fixed!)\n";
    var_dump($db2->get('key1'));
    unset($db2);
    LevelDB::destroy($leveldb_path);
} catch (LevelDBException $e) {
    echo "Failed to reopen: Lock still held (demonstrates the refcount bug)\n";
}

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-snapshot-read-closed.test-db';
@LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
string(6) "value1"
Database variable unset, but snapshot keeps DB alive
Snapshot released
Snapshot object unset
Failed to reopen: Lock still held (demonstrates the refcount bug)
