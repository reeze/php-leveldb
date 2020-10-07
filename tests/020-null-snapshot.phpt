--TEST--
leveldb - NULL snapshot should not throw exception
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = dirname(__FILE__) . '/null-snapshot.test-db';

$db = new LevelDB($leveldb_path);

var_dump($db->get("key", array('snapshot' => NULL)));

?>
Should no exception
==DONE==
--CLEAN--
<?php
$leveldb_path = dirname(__FILE__) . '/null-snapshot.test-db';
LevelDB::destroy($leveldb_path);
?>
--EXPECT--
bool(false)
Should no exception
==DONE==
