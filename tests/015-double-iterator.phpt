--TEST--
leveldb - Fixed bug segfault when double construct iterator
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb_iterator_double_construct.test-db';
$db = new LevelDB($leveldb_path);

$it1 = new LevelDBIterator($db);
$it2 = new LevelDBIterator($db);

$it1->destroy();
$it2->destroy();
?>
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb_iterator_double_construct.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
==DONE==
