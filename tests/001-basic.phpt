--TEST--
leveldb - basic: get(), set(), put(), delete()
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-basic.test-db';
$db = new LevelDB($leveldb_path);

var_dump($db->set('key', 'value'));
var_dump($db->get('key'));
var_dump($db->get('non-exists-key'));
var_dump($db->put('name', 'reeze'));
var_dump($db->get('name'));
var_dump($db->delete('name'));
var_dump($db->get('name'));
?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-basic.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
bool(true)
string(5) "value"
bool(false)
bool(true)
string(5) "reeze"
bool(true)
bool(false)
