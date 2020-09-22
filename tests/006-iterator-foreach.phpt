--TEST--
leveldb - iterate through db by foreach
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb_iterator-foreach.test-db';
$db = new LevelDB($leveldb_path);

/* Add test data, and the data will be be sorted */
$data = array(
	"First", "Second", "Third", 10, "", "Last"
);

foreach($data as $item) {
	$db->set($item, $item);
}

$it = new LevelDBIterator($db);

echo "*** Loop through in foreach style ***\n";
foreach ($it as $key => $value) {
	echo "{$key} => {$value}\n";
}

echo "*** Loop through in foreach with newly-created iterator ***\n";
foreach(new LevelDBIterator($db) as $key => $value){
	echo "{$key} => {$value}\n";
}
?>
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb_iterator-foreach.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
*** Loop through in foreach style ***
 => 
10 => 10
First => First
Last => Last
Second => Second
Third => Third
*** Loop through in foreach with newly-created iterator ***
 => 
10 => 10
First => First
Last => Last
Second => Second
Third => Third
