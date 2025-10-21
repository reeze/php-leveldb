--TEST--
leveldb - large keys and values
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-large.test-db';
$db = new LevelDB($leveldb_path);

echo "* Large value (1MB)\n";
$large_value = str_repeat('X', 1024 * 1024); // 1MB
$db->set('large_key', $large_value);
$retrieved = $db->get('large_key');
var_dump(strlen($retrieved));
var_dump(strlen($retrieved) === strlen($large_value));
var_dump($retrieved === $large_value);

echo "\n* Large key (10KB)\n";
$large_key = str_repeat('K', 10240); // 10KB
$db->set($large_key, 'small_value');
var_dump($db->get($large_key));
var_dump(strlen($db->get($large_key)));

echo "\n* Very large value (10MB)\n";
$very_large_value = str_repeat('Y', 10 * 1024 * 1024); // 10MB
$db->set('very_large_key', $very_large_value);
$retrieved2 = $db->get('very_large_key');
var_dump(strlen($retrieved2));
var_dump(strlen($retrieved2) === strlen($very_large_value));
var_dump($retrieved2 === $very_large_value);

echo "\n* Delete large value\n";
$db->delete('very_large_key');
var_dump($db->get('very_large_key'));

echo "Done\n";
?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-large.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Large value (1MB)
int(1048576)
bool(true)
bool(true)

* Large key (10KB)
string(11) "small_value"
int(11)

* Very large value (10MB)
int(10485760)
bool(true)
bool(true)

* Delete large value
bool(false)
Done
