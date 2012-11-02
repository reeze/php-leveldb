--TEST--
leveldb - compression
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb-comparator.test-db';

$db = new LevelDB($leveldb_path, array('compression' => 33));
$db->close();

$db2 = new LevelDB($leveldb_path, array('compression' => LEVELDB_SNAPPY_COMPRESSION));
$db2->set("key", "value");

$db2->close();
?>
==DONE==
--EXPECTF--
Warning: LevelDB::__construct(): Invalid compression type in %s on line %s
==DONE==
