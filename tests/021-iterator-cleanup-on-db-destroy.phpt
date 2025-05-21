--TEST--
LevelDB: Iterator resource cleanup when DB object is destroyed via iterator's destruction
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$leveldb_path = __DIR__ . '/leveldb-iter-cleanup.test-db';
register_shutdown_function(function() use ($leveldb_path) {
    @LevelDB::destroy($leveldb_path);
});

echo "Creating DB and Iterator
";
$db = new LevelDB($leveldb_path, array("create_if_missing" => true));
$db->put("key1", "val1");
$db->put("key2", "val2");

$iterator = $db->getIterator();

echo "Unsetting DB variable
";
unset($db); 

echo "Iterator valid before unset: " . ($iterator->valid() ? "Yes" : "No") . "
";
$iterator->rewind();
if ($iterator->valid()) {
    echo "Key: " . $iterator->key() . ", Value: " . $iterator->current() . "
";
}

echo "Unsetting iterator
";
unset($iterator);

echo "Test completed. Check for memory errors with appropriate tools.
";

try {
    echo "Attempting to reopen and use DB
";
    $db2 = new LevelDB($leveldb_path); 
    $val = $db2->get("key1");
    var_dump($val);
    unset($db2); 
} catch (LevelDBException $e) {
    echo "Error reopening DB: " . $e->getMessage() . "
";
}

?>
--CLEAN--
<?php
// Destruction is handled by register_shutdown_function
?>
--EXPECTF--
Creating DB and Iterator
Unsetting DB variable
Iterator valid before unset: Yes
Key: key1, Value: val1
Unsetting iterator
Test completed. Check for memory errors with appropriate tools.
Attempting to reopen and use DB
string(4) "val1"
