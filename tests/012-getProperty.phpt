--TEST--
leveldb - getProperty
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb-getProperty.test-db';

$db = new LevelDB($leveldb_path);
for($i=0; $i < 99999; ++$i) {
	$db->set("b{$i}", "value{$i}");
}

var_dump($db->getProperty('leveldb.stats') !== false);
var_dump($db->getProperty('leveldb.num-files-at-level1') !== false);
var_dump($db->getProperty('leveldb.num-files-at-level2') !== false);
var_dump($db->getProperty('leveldb.num-files-at-level3') !== false);
var_dump($db->getProperty('leveldb.sstables') !== false);

var_dump($db->getProperty('leveldb.anything') === false);
var_dump($db->getProperty('anythingelse') === false);
?>
==DONE==
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
==DONE==
