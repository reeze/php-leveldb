--TEST--
leveldb - open base dir
--SKIPIF--
<?php include 'skipif.inc'; } ?>
--FILE--
<?php

ini_set("open_basedir", dirname(__FILE__));

$path = dirname(__FILE__) . '/../leveldb_openbasedir.test-db';
$db = new LevelDB($path);
LevelDB::destroy($path);
LevelDB::repair($path);
?>
--EXPECTF--
Warning: LevelDB::__construct(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d

Warning: LevelDB::destroy(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d

Warning: LevelDB::repair(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d
