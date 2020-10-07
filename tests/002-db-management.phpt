--TEST--
leveldb - db management
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$path = dirname(__FILE__) . '/leveldb-db-op.test-db';
$db = new LevelDB($path);
$db->set("key", "value");

unset($db);
var_dump(LevelDB::destroy($path));
var_dump(file_exists($path));
?>
--EXPECTF--
bool(true)
bool(false)
