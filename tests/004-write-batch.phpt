--TEST--
leveldb - write batch
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb_batch.test-db';
$db = new LevelDB($leveldb_path);

echo "* batch write with setting\n";
$batch = new LevelDBWriteBatch();
$batch->set('batch_foo', 'batch_bar');
$batch->put('batch_foo2', 'batch_bar2');
$batch->delete('batch_foo');

var_dump($db->write($batch));

var_dump($db->get('batch_foo2'));
var_dump($db->get('batch_foo'));

echo "* batch write with delete\n";
$batch->clear();
$batch->delete('batch_foo2');
$batch->set('batch_foo', 'batch again');

var_dump($db->write($batch));

var_dump($db->get('batch_foo2'));
var_dump($db->get('batch_foo'));
?>
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb_batch.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* batch write with setting
bool(true)
string(10) "batch_bar2"
bool(false)
* batch write with delete
bool(true)
bool(false)
string(11) "batch again"
