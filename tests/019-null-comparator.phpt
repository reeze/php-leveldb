--TEST--
leveldb - NULL comparator should not throw exception
--DESCRIPTION--
NULL is the default value of comparator open options
open with null shouldn't throw exception
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/null-comparator.test-db';

$db = new LevelDB($leveldb_path, array('comparator' => NULL));

?>
Should no exception
==DONE==
--EXPECT--
Should no exception
==DONE==
