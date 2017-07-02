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

var_dump($db->getProperty('leveldb.stats'));
var_dump($db->getProperty('leveldb.num-files-at-level1'));
var_dump($db->getProperty('leveldb.num-files-at-level2'));
var_dump($db->getProperty('leveldb.num-files-at-level3'));
var_dump($db->getProperty('leveldb.sstables'));

var_dump($db->getProperty('leveldb.anything'));
var_dump($db->getProperty('anythingelse'));
?>
==DONE==
--EXPECTF--
string(%d) "                               Compactions
Level  Files Size(MB) Time(sec) Read(MB) Write(MB)
--------------------------------------------------
  2        1        2         %d        0         2
"
string(1) "0"
string(1) "1"
string(1) "0"
string(160) "--- level 0 ---
--- level 1 ---
--- level 2 ---
 5:1901489['b0' @ 1 : 1 .. 'b9999' @ 10000 : 1]
--- level 3 ---
--- level 4 ---
--- level 5 ---
--- level 6 ---
"
bool(false)
bool(false)
==DONE==
