--TEST--
leveldb - getProperty
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb-getProperty.test-db';

$db = new LevelDB($leveldb_path);
for($i=0; $i < 99999; ++$i) {
	$db->set("b{$i}", "value{$i}");
}

$stats = $db->getProperty('leveldb.stats');
var_dump($stats !== false);
var_dump(strlen($stats) > 1);
var_dump($db->getProperty('leveldb.num-files-at-level1'));
var_dump($db->getProperty('leveldb.num-files-at-level2'));
var_dump($db->getProperty('leveldb.num-files-at-level3'));

$sstables = $db->getProperty('leveldb.sstables');
var_dump($sstables !== false);
var_dump(strlen($sstables) > 1);

var_dump($db->getProperty('leveldb.anything'));
var_dump($db->getProperty('anythingelse'));
?>
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb-getProperty.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
bool(true)
bool(true)
string(1) "%d"
string(1) "%d"
string(1) "%d"
bool(true)
bool(true)
bool(false)
bool(false)
==DONE==
