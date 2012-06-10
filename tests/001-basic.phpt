--TEST--
leveldb - basic: get(), set(), put(), delete()
--SKIPIF--
<?php include 'skipif.inc'; } ?>
--FILE--
<?php

$path = dirname(__FILE__) . '/leveldb.test';
$db = new LevelDb($path);

var_dump($db->set('key', 'value'));
var_dump($db->get('key'));
var_dump($db->get('non-exists-key'));
var_dump($db->put('name', 'reeze'));
var_dump($db->get('name'));
var_dump($db->delete('name'));
var_dump($db->get('name'));
?>
--EXPECTF--
bool(true)
string(5) "value"
bool(false)
bool(true)
string(5) "reeze"
bool(true)
bool(false)
