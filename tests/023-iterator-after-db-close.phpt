--TEST--
leveldb - iterator usage after database close
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-iter-db-close.test-db';
$db = new LevelDB($leveldb_path);

// Add some data
$db->set('key1', 'value1');
$db->set('key2', 'value2');
$db->set('key3', 'value3');

// Create an iterator
$iterator = new LevelDBIterator($db);

echo "* Iterator before closing DB:\n";
var_dump($iterator->valid());
var_dump($iterator->key());
var_dump($iterator->current());

// Close/destroy the database
unset($db);

echo "\n* Iterator after closing DB:\n";
// This should throw an exception instead of crashing
try {
    var_dump($iterator->valid());
} catch (Exception $e) {
    echo "Exception on valid(): " . $e->getMessage() . "\n";
}

try {
    var_dump($iterator->key());
} catch (Exception $e) {
    echo "Exception on key(): " . $e->getMessage() . "\n";
}

try {
    var_dump($iterator->current());
} catch (Exception $e) {
    echo "Exception on current(): " . $e->getMessage() . "\n";
}

try {
    $iterator->next();
} catch (Exception $e) {
    echo "Exception on next(): " . $e->getMessage() . "\n";
}

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-iter-db-close.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Iterator before closing DB:
bool(true)
string(4) "key1"
string(6) "value1"

* Iterator after closing DB:
Exception on valid(): Can not iterate on closed db
Exception on key(): Can not iterate on closed db
Exception on current(): Can not iterate on closed db
Exception on next(): Can not iterate on closed db
