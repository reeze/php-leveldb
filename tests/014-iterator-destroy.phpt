--TEST--
leveldb - iterator destroy
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb_iterator_destroy.test-db';
$db = new LevelDB($leveldb_path);

$db->set("key", "value");
$it = new LevelDBIterator($db);

$it->destroy();
try {
	$it->next();
} catch(LevelDBException $e) {
	echo $e->getMessage() . "\n";
}
?>
==DONE==
--EXPECTF--
Iterator has been destroyed
==DONE==
