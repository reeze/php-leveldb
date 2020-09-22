--TEST--
leveldb - compression
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb-compression.test-db';

$db = new LevelDB($leveldb_path, array('compression' => 33));
unset($db);

$db = new LevelDB($leveldb_path, array('compression' => LEVELDB_SNAPPY_COMPRESSION));
$db->set("key", "value");

?>
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb-compression.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
Warning: LevelDB::__construct(): Unsupported compression type in %s on line %s
==DONE==
