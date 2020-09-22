--TEST--
leveldb - NULL comparator should not throw exception
--DESCRIPTION--
NULL is the default value of comparator open options
open with null shouldn't throw exception
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/null-comparator.test-db';

$db = new LevelDB($leveldb_path, array('comparator' => NULL));

?>
Should no exception
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/null-comparator.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECT--
Should no exception
==DONE==
