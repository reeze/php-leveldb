--TEST--
leveldb - write batch with empty operations
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-batch-empty.test-db';
$db = new LevelDB($leveldb_path);

echo "* Empty batch write\n";
$batch = new LevelDBWriteBatch();
// Write an empty batch - should succeed but do nothing
var_dump($db->write($batch));

echo "\n* Batch with empty key\n";
$batch->clear();
$batch->set('', 'empty_key_value');
var_dump($db->write($batch));
var_dump($db->get(''));

echo "\n* Batch with empty value\n";
$batch->clear();
$batch->set('empty_value_key', '');
var_dump($db->write($batch));
var_dump($db->get('empty_value_key'));
var_dump($db->get('empty_value_key') === '');

echo "\n* Batch delete non-existent key\n";
$batch->clear();
$batch->delete('non_existent_key_12345');
var_dump($db->write($batch));
var_dump($db->get('non_existent_key_12345'));

echo "\n* Batch reuse after clear\n";
$batch->clear();
$batch->set('key1', 'value1');
var_dump($db->write($batch));
var_dump($db->get('key1'));

$batch->clear();
$batch->set('key2', 'value2');
var_dump($db->write($batch));
var_dump($db->get('key2'));
var_dump($db->get('key1')); // Should still exist

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-batch-empty.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Empty batch write
bool(true)

* Batch with empty key
bool(true)
string(15) "empty_key_value"

* Batch with empty value
bool(true)
string(0) ""
bool(true)

* Batch delete non-existent key
bool(true)
bool(false)

* Batch reuse after clear
bool(true)
string(6) "value1"
bool(true)
string(6) "value2"
string(6) "value1"
