--TEST--
leveldb - open base dir
--INI--
open_basedir=.
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$cwd = getcwd();

$path = $cwd . '/../leveldb_openbasedir.test-db';
$db = new LevelDB($path);
LevelDB::destroy($path);
LevelDB::repair($path);
?>
--EXPECTF--
Warning: LevelDB::__construct(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d

Warning: LevelDB::destroy(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d

Warning: LevelDB::repair(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d
