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

var_dump($db->getApproximateSizes(array(
	array("A", "B"),
	array("b0", "Z"),
)));
?>
==DONE==
--EXPECTF--
array(2) {
  [0]=>
  int(%d)
  [1]=>
  int(%d)
}
==DONE==
