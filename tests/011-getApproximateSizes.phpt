--TEST--
leveldb - getApproximateSizes
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb-getApproximateSizes.test-db';

$db = new LevelDB($leveldb_path);
for($i=0; $i < 99999; ++$i) {
	$db->set("b{$i}", "value");
}

$db->close();

$db = new LevelDB($leveldb_path);

var_dump($db->getApproximateSizes(array("A", "B", 3), array("b0", "Z")));
var_dump($db->getApproximateSizes(array(), array()));
var_dump($db->getApproximateSizes(array("a", "b"), array("c", "Z")));
?>
==DONE==
--EXPECTF--
Warning: LevelDB::getApproximateSizes(): The num of start keys and limit keys didn't match in %s on line %d
bool(false)
array(0) {
}
array(2) {
  [0]=>
  int(%d)
  [1]=>
  int(%d)
}
==DONE==
