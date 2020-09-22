--TEST--
leveldb - iterate through db
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb_iterator.test-db';
$db = new LevelDB($leveldb_path);

/* Add test data, and the data will be be sorted */
$data = array(
	"First", "Second", "Third", 10, "", "Last"
);

foreach($data as $item) {
	$db->set($item, $item);
}

$it = new LevelDBIterator($db);

echo "*** Loop through ***\n";
for ($it->rewind(); $it->valid(); $it->next()) {
	echo $it->key() . " => " . $it->current() . "\n";
}

echo "\n*** Reset to last ***\n";
var_dump($it->last());
var_dump($it->key() . " => " . $it->current());

echo "\n*** Last->next will be invalid ***\n";
var_dump($it->next());
var_dump($it->valid());
var_dump($it->key());

echo "\n*** Seek to give key ***\n";
$it->seek("Second");
var_dump($it->current());

echo "\n*** Seek to a non-exist key will point to nearest next key ***\n";
$it->seek("11");
var_dump($it->current());

echo "\n*** Bound checking ***\n";
$it->rewind();
$it->prev();
$it->prev();
var_dump($it->current());

$it->next();
$it->next();
$it->next();
$it->next();
$it->next();
$it->next();
$it->next();
$it->next();
$it->next();
$it->next();
var_dump($it->current());

var_dump($it->getError());
?>
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb_iterator.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
*** Loop through ***
 => 
10 => 10
First => First
Last => Last
Second => Second
Third => Third

*** Reset to last ***
NULL
string(14) "Third => Third"

*** Last->next will be invalid ***
NULL
bool(false)
bool(false)

*** Seek to give key ***
string(6) "Second"

*** Seek to a non-exist key will point to nearest next key ***
string(5) "First"

*** Bound checking ***
bool(false)
bool(false)
bool(false)
