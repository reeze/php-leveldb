--TEST--
leveldb - reopen cleans up old resources
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$path1 = __DIR__ . '/reopen1.test-db';
$path2 = __DIR__ . '/reopen2.test-db';

$db = new LevelDB($path1, array('comparator' => 'custom_comparator'));
$db->set('foo', 'bar');

// Reopen the same object with a new database
$db->__construct($path2);
var_dump($db->get('foo')); // should be false since new DB is empty

function custom_comparator($a, $b) {
    if ($a == $b) {
        return 0;
    }
    return ($a > $b) ? -1 : 1;
}
?>
==DONE==
--CLEAN--
<?php
$path1 = __DIR__ . '/reopen1.test-db';
$path2 = __DIR__ . '/reopen2.test-db';
LevelDB::destroy($path1);
LevelDB::destroy($path2);
?>
--EXPECT--
bool(false)
==DONE==
