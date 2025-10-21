--TEST--
leveldb - snapshot release() doesn't decrement database refcount (BUG)
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php

$leveldb_path = __DIR__ . '/leveldb-snapshot-refcount-bug.test-db';
$db = new LevelDB($leveldb_path);
$db->set('key1', 'value1');

echo "* Creating snapshot\n";
$snapshot = $db->getSnapshot();

echo "* Unsetting database (snapshot holds reference)\n";
unset($db);

echo "* Manually releasing snapshot\n";
$snapshot->release();

echo "* Unsetting snapshot object\n";
unset($snapshot);

echo "* Trying to reopen database\n";
// BUG: This will fail because the database reference wasn't properly released
// The release() method sets intern->db = NULL but doesn't call zval_ptr_dtor(&intern->zdb)
// to decrement the refcount!
try {
    $db2 = new LevelDB($leveldb_path);
    echo "Successfully reopened (bug is fixed)\n";
    var_dump($db2->get('key1'));
} catch (LevelDBException $e) {
    echo "FAILED: " . $e->getMessage() . "\n";
    echo "This demonstrates the refcount bug in LevelDBSnapshot::release()\n";
}

?>
--CLEAN--
<?php
$leveldb_path = __DIR__ . '/leveldb-snapshot-refcount-bug.test-db';
@LevelDB::destroy($leveldb_path);
?>
--EXPECTF--
* Creating snapshot
* Unsetting database (snapshot holds reference)
* Manually releasing snapshot
* Unsetting snapshot object
* Trying to reopen database
FAILED: IO error: lock %s/LOCK: already held by process
This demonstrates the refcount bug in LevelDBSnapshot::release()
