--TEST--
leveldb - block_cache_size releases on reopen
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$path1 = __DIR__ . '/cache1.test-db';
$path2 = __DIR__ . '/cache2.test-db';

$db = new LevelDB($path1, array('block_cache_size' => 1024 * 1024));
$db->set('foo', 'bar');

$db->__construct($path2, array('block_cache_size' => 1024 * 1024));
var_dump($db->get('foo'));
?>
==DONE==
--CLEAN--
<?php
$path1 = __DIR__ . '/cache1.test-db';
$path2 = __DIR__ . '/cache2.test-db';
LevelDB::destroy($path1);
LevelDB::destroy($path2);
?>
--EXPECT--
bool(false)
==DONE==
