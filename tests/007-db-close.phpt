--TEST--
leveldb - db close
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb_close.test-db';
$db = new LevelDB($leveldb_path);

$db->close();


unset($it);
unset($db);

// ensure destructeur properly close the db */
LevelDB::destroy($leveldb_path);
var_dump(file_exists($leveldb_path));
?>
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb_close.test-db';
if (file_exists($leveldb_path)) LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
Deprecated: %s LevelDB::close() is deprecated in %s
bool(false)
