--TEST--
leveldb - snapshot
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb-snapshot.test-db';
@unlink($leveldb_path);

$db = new LevelDB($leveldb_path);
$db->put("key1", "value1");
$db->put("key2", "value2");

$snapshot = new LevelDBSnapshot($db);

$db->put("key3", "value3");

try {
	$it = new LevelDBIterator($db, array('snapshot' => ''));
} catch(LevelDBException $e) {
	var_dump($e->getMessage());
}

$it = $db->getIterator(array('snapshot' => $snapshot));
foreach($it as $k => $v) {
	echo "$k => $v\n";
}

var_dump($db->get("key3", array('snapshot' => $snapshot)));
var_dump($db->get("key3"));

$snapshot->release();
$snapshot->release();
try {
	$it = new LevelDBIterator($db, array('snapshot' => $snapshot));
} catch(LevelDBException $e) {
	var_dump($e->getMessage());
}
?>
==DONE==
--EXPECTF--
string(69) "Invalid snapshot parameter, it must be an instance of LevelDBSnapshot"
key1 => value1
key2 => value2
bool(false)
string(6) "value3"
string(48) "Invalid snapshot parameter, it has been released"
==DONE==
