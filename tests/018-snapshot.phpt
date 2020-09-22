--TEST--
leveldb - snapshot
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

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

echo "*** Release snapshot x1 ***\n";
$snapshot->release();
echo "*** Release snapshot x2 ***\n";
$snapshot->release();

echo "*** Try to use released snapshot ***\n";
try {
	$it = new LevelDBIterator($db, array('snapshot' => $snapshot));
} catch(LevelDBException $e) {
	var_dump($e->getMessage());
}
?>
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb-snapshot.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
string(69) "Invalid snapshot parameter, it must be an instance of LevelDBSnapshot"
key1 => value1
key2 => value2
bool(false)
string(6) "value3"
*** Release snapshot x1 ***
*** Release snapshot x2 ***
*** Try to use released snapshot ***
string(48) "Invalid snapshot parameter, it has been released"
==DONE==
