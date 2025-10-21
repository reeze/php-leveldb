--TEST--
leveldb - iterator on empty database
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-iter-empty.test-db';
$db = new LevelDB($leveldb_path);

echo "* Iterator on empty database\n";
$iterator = new LevelDBIterator($db);

var_dump($iterator->valid());
var_dump($iterator->key());
var_dump($iterator->current());

echo "\n* Iterator operations on empty database\n";
$iterator->rewind();
var_dump($iterator->valid());

$iterator->last();
var_dump($iterator->valid());

$iterator->next();
var_dump($iterator->valid());

$iterator->prev();
var_dump($iterator->valid());

$iterator->seek('any_key');
var_dump($iterator->valid());

echo "\n* Foreach on empty database\n";
$count = 0;
foreach ($iterator as $key => $value) {
    $count++;
}
echo "Count: $count\n";

echo "\n* Add data and test iterator\n";
$db->set('key1', 'value1');
$db->set('key2', 'value2');

$iterator2 = new LevelDBIterator($db);
var_dump($iterator2->valid());
var_dump($iterator2->key());
var_dump($iterator2->current());

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-iter-empty.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Iterator on empty database
bool(false)
bool(false)
bool(false)

* Iterator operations on empty database
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)

* Foreach on empty database
Count: 0

* Add data and test iterator
bool(true)
string(4) "key1"
string(6) "value1"
