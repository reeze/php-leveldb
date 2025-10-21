--TEST--
leveldb - iterator keeps database reference alive (by design)
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

echo "* Iterator before unsetting DB:\n";
var_dump($iterator->valid());
var_dump($iterator->key());
var_dump($iterator->current());

// Unset the database variable
// NOTE: The database remains open because the iterator holds a reference
unset($db);

echo "\n* Iterator after unsetting DB variable:\n";
echo "Iterator continues to work because it holds a DB reference\n";
var_dump($iterator->valid());
var_dump($iterator->key());
var_dump($iterator->current());

echo "\n* Iterator can still navigate:\n";
$iterator->next();
var_dump($iterator->valid());
var_dump($iterator->key());

echo "\n* Destroying iterator:\n";
$iterator->destroy();

echo "Iterator destroyed, trying to use it throws exception:\n";
try {
    var_dump($iterator->valid());
} catch (LevelDBException $e) {
    echo "Exception: " . $e->getMessage() . "\n";
}

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-iter-db-close.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Iterator before unsetting DB:
bool(true)
string(4) "key1"
string(6) "value1"

* Iterator after unsetting DB variable:
Iterator continues to work because it holds a DB reference
bool(true)
string(4) "key1"
string(6) "value1"

* Iterator can still navigate:
bool(true)
string(4) "key2"

* Destroying iterator:
Iterator destroyed, trying to use it throws exception:
Exception: Iterator has been destroyed
