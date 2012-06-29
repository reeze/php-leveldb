--TEST--
leveldb - compactRange
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb-compactRange.test-db';

$db = new LevelDB($leveldb_path);
for($i=0; $i < 99999; ++$i) {
	$db->set("b{$i}", "value{$i}");
}

var_dump($db->compactRange('a', 'Z'));
?>
==DONE==
--EXPECTF--
NULL
==DONE==
