--TEST--
LevelDB: LevelDBSnapshot::release() should release reference to DB object
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$leveldb_path = __DIR__ . '/leveldb-snapshot-release.test-db';

class MyDB extends LevelDB {
    public static $destroyed = false;
    public function __destruct() {
        echo "MyDB::__destruct called
";
        self::$destroyed = true;
    }
}

register_shutdown_function(function() use ($leveldb_path) {
    @LevelDB::destroy($leveldb_path);
});

echo "Creating MyDB instance
";
$db = new MyDB($leveldb_path, ["create_if_missing" => true]);
$db->put("key", "value");

echo "Getting snapshot
";
$snapshot = $db->getSnapshot();

echo "Releasing snapshot
";
$snapshot->release(); 

echo "Unsetting DB variable
";
unset($db); 

if (MyDB::$destroyed) {
    echo "MyDB instance was destroyed after unsetting \$db (snapshot released its ref).
";
} else {
    echo "MyDB instance was NOT destroyed after unsetting \$db (snapshot may still hold a ref).
";
}

echo "Unsetting snapshot variable
";
unset($snapshot); 

if (MyDB::$destroyed) {
    echo "MyDB instance was (or is now definitely) destroyed after unsetting \$snapshot.
";
} else {
    echo "Error: MyDB instance not destroyed even after snapshot unset.
";
}

MyDB::$destroyed = false;
?>
--CLEAN--
<?php
// Destruction is handled by register_shutdown_function
?>
--EXPECTF--
Creating MyDB instance
Getting snapshot
Releasing snapshot
Unsetting DB variable
MyDB::__destruct called
MyDB instance was destroyed after unsetting $db (snapshot released its ref).
Unsetting snapshot variable
MyDB instance was (or is now definitely) destroyed after unsetting $snapshot.
