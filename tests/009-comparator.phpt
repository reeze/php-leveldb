--TEST--
leveldb - custom comparator
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/leveldb-comparator.test-db';

echo "*** could not use invalid custom comparator ***\n";
try {
	$db = new LevelDB($leveldb_path, array('comparator' => 'invaid_func'));
} catch(LevelDBException $e) {
	echo $e->getMessage() . "\n";
}

echo "*** valid custom comparator ***\n";
$db = new LevelDB($leveldb_path, array('comparator' => 'custom_comparator'));

$values = array(3, 1, 4, 6, 2, 5);
foreach($values as $v) {
	$db->set($v, $v);
}

$it = new LevelDBIterator($db);
foreach($it as $v) {
	echo "$v\n";
}
unset($it);
unset($db);

echo "*** custom comparator can only open with the same comparator again ***\n";
try {
	$db = new LevelDB($leveldb_path);
} catch(LevelDBException $e) {
	echo $e->getMessage() . "\n";
}


$db = new LevelDB($leveldb_path, array('comparator' => 'custom_comparator'));
var_dump($db->get(1) == 1);
unset($db);


echo "*** custom comparator which throw exception ***\n";
$db = new LevelDB($leveldb_path, array('comparator' => array('CustomFunc', 'willException')));
try {
	$db->set("Hi", "guys");
	var_dump($db->get("Hi"));
} catch(Exception $e) {
	echo $e->getMessage() . "\n";
}

// reverse DESC
function custom_comparator($a, $b) {
   if ($a == $b) {
        return 0;
    }
    return ($a > $b) ? -1 : 1;
}

class CustomFunc {
	public static function willException($a, $b) {
		throw new Exception("Oops!");
	}
}
?>
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/leveldb-comparator.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
*** could not use invalid custom comparator ***
Invalid open option: comparator, invaid_func() is not callable
*** valid custom comparator ***
6
5
4
3
2
1
*** custom comparator can only open with the same comparator again ***
Invalid argument: php_leveldb.custom_comparator%s leveldb.BytewiseComparator
bool(true)
*** custom comparator which throw exception ***
Oops!
==DONE==
