--TEST--
leveldb - binary data with null bytes
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-binary.test-db';
$db = new LevelDB($leveldb_path);

echo "* Binary key with null bytes\n";
$binary_key = "key\0with\0nulls";
$db->set($binary_key, 'value1');
var_dump($db->get($binary_key));
var_dump(strlen($db->get($binary_key)));

echo "\n* Binary value with null bytes\n";
$binary_value = "value\0with\0null\0bytes";
$db->set('binary_key', $binary_value);
$retrieved = $db->get('binary_key');
var_dump(strlen($retrieved));
var_dump(strlen($binary_value));
var_dump($retrieved === $binary_value);
echo "Hex: " . bin2hex($retrieved) . "\n";

echo "\n* Both key and value with null bytes\n";
$binary_key2 = "another\0key";
$binary_value2 = "another\0value";
$db->set($binary_key2, $binary_value2);
$retrieved2 = $db->get($binary_key2);
var_dump(strlen($retrieved2));
var_dump($retrieved2 === $binary_value2);

echo "\n* Iterator with binary keys\n";
$db->set("aaa\0key", "value_aaa");
$db->set("zzz\0key", "value_zzz");

$iter = new LevelDBIterator($db);
$count = 0;
foreach ($iter as $key => $value) {
    if (strpos($key, "\0") !== false) {
        echo "Found binary key: " . bin2hex($key) . "\n";
        $count++;
    }
}
echo "Binary keys found: $count\n";

echo "\n* Delete binary key\n";
$db->delete($binary_key);
var_dump($db->get($binary_key));

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-binary.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Binary key with null bytes
string(6) "value1"
int(6)

* Binary value with null bytes
int(21)
int(21)
bool(true)
Hex: 76616c75650077697468006e756c6c006279746573

* Both key and value with null bytes
int(13)
bool(true)

* Iterator with binary keys
Found binary key: %s
Found binary key: %s
Found binary key: %s
Found binary key: %s
Binary keys found: 4

* Delete binary key
bool(false)
