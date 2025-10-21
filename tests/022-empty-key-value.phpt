--TEST--
leveldb - empty key and empty value handling
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-empty-key-value.test-db';
$db = new LevelDB($leveldb_path);

echo "* Test empty value\n";
var_dump($db->set('key1', ''));
var_dump($db->get('key1'));
var_dump($db->get('key1') === '');
var_dump($db->get('key1') === false);

echo "\n* Test empty key\n";
try {
    var_dump($db->set('', 'value'));
    var_dump($db->get(''));
    var_dump($db->get('') === 'value');
} catch (Exception $e) {
    echo "Exception: " . $e->getMessage() . "\n";
}

echo "\n* Test both empty\n";
try {
    var_dump($db->set('', ''));
    var_dump($db->get(''));
    var_dump($db->get('') === '');
    var_dump($db->get('') === false);
} catch (Exception $e) {
    echo "Exception: " . $e->getMessage() . "\n";
}

echo "\n* Test delete with empty key\n";
try {
    var_dump($db->delete(''));
} catch (Exception $e) {
    echo "Exception: " . $e->getMessage() . "\n";
}

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-empty-key-value.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Test empty value
bool(true)
string(0) ""
bool(true)
bool(false)

* Test empty key
bool(true)
string(5) "value"
bool(true)

* Test both empty
bool(true)
string(0) ""
bool(true)
bool(false)

* Test delete with empty key
bool(true)
