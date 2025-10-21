--TEST--
leveldb - snapshot read after database close
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

// Close the database
unset($db);

// Reopen the database
$db = new LevelDB($leveldb_path);

// Try to use the old snapshot with the new database connection
// This should fail gracefully with an exception
try {
    $result = $db->get('key1', array('snapshot' => $snapshot));
    echo "Read succeeded (unexpected): ";
    var_dump($result);
} catch (Exception $e) {
    echo "Exception caught (expected): " . $e->getMessage() . "\n";
}

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-snapshot-read-closed.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
string(6) "value1"
%AException%s
