--TEST--
leveldb - LevelDB::getIterator()
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb_get_iterator.test-db';
$db = new LevelDB($leveldb_path);

foreach(range(1, 9) as $item) {
	$db->set($item, $item);
}

$it = $db->getIterator();

var_dump(get_class($it));

for ($it->rewind(); $it->valid(); $it->next()) {
	echo $it->key() . " => " . $it->current() . "\n";
}
?>
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb_get_iterator.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
string(15) "LevelDBIterator"
1 => 1
2 => 2
3 => 3
4 => 4
5 => 5
6 => 6
7 => 7
8 => 8
9 => 9
