--TEST--
leveldb - db close
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb_close.test-db';
$db = new LevelDB($leveldb_path);

$db->set("key", "value");

$it = new LevelDBIterator($db);

$db->close();
$db->close();

try {
	$db->set("new-key", "value");
} catch(Exception $e) {
	var_dump("should be exception");
}

try {
	$it->next();
} catch(Exception $e) {
	var_dump("should be exception");
}
?>
--EXPECTF--
string(19) "should be exception"
string(19) "should be exception"
