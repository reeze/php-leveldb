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
} catch(LevelDBException $e) {
	echo $e->getMessage() . "\n";
}

try {
	$it->next();
} catch(LevelDBException $e) {
	echo $e->getMessage() . "\n";
}

try{
	$it->rewind();
}catch(LevelDBException $e){
	echo $e->getMessage() . "\n";
}

try{
	foreach($it as $key => $value){
		echo "$key => $value\n";
	}
}catch(LevelDBException $e){
	echo $e->getMessage() . "\n";
}
?>
--EXPECTF--
Can not operate on closed db
Can not iterate on closed db
Iterator has been destroyed
Iterator has been destroyed
