--TEST--
leveldb - custom comparator
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

include "leveldb.inc";
cleanup_leveldb_on_shutdown();

$leveldb_path = dirname(__FILE__) . '/leveldb-comparator.test-db';

echo "*** could not use invalid custom comparator ***\n";
try {
	$db2 = new LevelDB($leveldb_path, array('comparator' => 'invaid_func'));
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

$db->close();

echo "*** custom comparator can only open with the same comparator again ***\n";
try {
	$db2 = new LevelDB($leveldb_path);
} catch(LevelDBException $e) {
	echo $e->getMessage() . "\n";
}

$db2 = new LevelDB($leveldb_path, array('comparator' => 'custom_comparator'));
var_dump($db2->get(1) == 1);

$db2->close();


echo "*** custom comparator which throw exception ***\n";
$db3 = new LevelDB($leveldb_path, array('comparator' => array('CustomFunc', 'willException')));
try {
	$db3->set("Hi", "guys");
	var_dump($db3->get("Hi"));
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
Invalid argument: php_leveldb.custom_comparatordoes not match existing comparator : leveldb.BytewiseComparator
bool(true)
*** custom comparator which throw exception ***
Oops!
==DONE==
