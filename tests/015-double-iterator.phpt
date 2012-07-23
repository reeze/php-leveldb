--TEST--
leveldb - Fixed bug iterator double construct
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb_iterator_double_construct.test-db';
$db = new LevelDB($leveldb_path);

$it = new LevelDBIterator($db);
$it = new LevelDBIterator($db);

$it->destroy();
?>
==DONE==
--EXPECTF--
