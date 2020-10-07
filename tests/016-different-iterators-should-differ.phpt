--TEST--
leveldb - different iterators should not affect each other
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb_iterator_should_not_affect_eachother.test-db';
$db = new LevelDB($leveldb_path);

foreach(array(1, 2, 3, 4) as $k) {
	$db->put($k, $k);
}

$it1 = new LevelDBIterator($db);
$it2 = new LevelDBIterator($db);

foreach($it1 as $k => $v) {
	echo "$k => $v\n";
}

$it1->rewind();
var_dump($it1->next());
var_dump($it1->next());
var_dump($it1->current());
var_dump($it2->current());

$it1->destroy();
$it2->destroy();
?>
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb_iterator_should_not_affect_eachother.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
1 => 1
2 => 2
3 => 3
4 => 4
NULL
NULL
string(1) "3"
string(1) "1"
==DONE==
