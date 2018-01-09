# PHP-LevelDB: The PHP Binding for LevelDB

Build Status: [![Build Status](https://secure.travis-ci.org/reeze/php-leveldb.png)](http://travis-ci.org/reeze/php-leveldb)

LevelDB is a fast key-value storage library written at Google that provides an ordered mapping from string keys to string values.

This extension is a binding for LevelDB

Please send Feature Request or Bug report  with [GitHub Issue](https://github.com/reeze/php-leveldb/issues).

## Requirements
- PHP >= 7
- LevelDB >= 1.7

You can install leveldb from your os distribution:

	$ sudo apt-get install libleveldb-dev


Or you could get leveldb from: <https://github.com/google/leveldb.git>

	$ git clone https://github.com/google/leveldb.git
	$ cd leveldb
	$ make

>**NOTE** LevelDB didn't have make install target in Makefile:
><http://code.google.com/p/leveldb/issues/detail?id=27>，
>If you want to install to a specific dir, you could:
>`make INSTALL_PATH=/Your/Path/`

## Installation

	$ git clone https://github.com/reeze/php-leveldb.git
	$ cd php-leveldb
	$ phpize
	$ ./configure --with-leveldb=/path/to/your/leveldb-1.*.*
	$ make
	$ make install

1. Install from PECL

	<http://pecl.php.net/package/leveldb>

## API Reference

API Reference could be found here: <http://reeze.cn/php-leveldb/doc/>

## Usage
Since PHP-LevelDB is a binding for LevelDB, most of the interfaces are the same as
LevelDB's: <https://github.com/google/leveldb/blob/master/doc/index.md>

### Open options
When opening a leveldb database you could specify options to override default values:

````php
<?php
/* default open options */
$options = array(
	'create_if_missing' => true,	// if the specified database didn't exist will create a new one
	'error_if_exists'	=> false,	// if the opened database exsits will throw exception
	'paranoid_checks'	=> false,
	'block_cache_size'	=> 8 * (2 << 20),
	'write_buffer_size' => 4<<20,
	'block_size'		=> 4096,
	'max_open_files'	=> 1000,
	'block_restart_interval' => 16,
	'compression'		=> LEVELDB_SNAPPY_COMPRESSION,
	'comparator'		=> NULL,   // any callable parameter which returns 0, -1, 1
);
/* default readoptions */
$readoptions = array(
	'verify_check_sum'	=> false,
	'fill_cache'		=> true,
	'snapshot'			=> null
);

/* default write options */
$writeoptions = array(
	'sync' => false
);

$db = new LevelDB("/path/to/db", $options, $readoptions, $writeoptions);
````

>**NOTE** The readoptions and writeoptions will take effect when operating on
>the db afterward, but you could override it by specify read/write options when
>accessing

### Using custom comparator
You could write your own comparator, the comparator can be anything callable in php the same as usort()'s compare function: <http://php.net/usort>, and the comparator
could be:

````php
<?php
int callback(string $a, string $b );
$db = new LevelDB("/path/to/db", array('comparator' => 'cmp'));
function cmp($a, $b)
{
	return strcmp($a, $b);
}
````

>**NOTE**
>If you create a database with custom comparator, you can only open it again
>with the same comparator.

### Basic operations: get(), put(), delete()
LevelDB is a key-value database, you could do those basic operations on it:

````php
<?php

$db = new LevelDB("/path/to/leveldb-test-db");

/*
 * Basic operate methods: set(), get(), delete()
 */
$db->put("Key", "Value");
$db->set("Key2", "Value2"); // set() is an alias of put()
$db->get("Key");
$db->delete("Key");
````

>**NOTE**
>Some key-value db use set instead of put to set value, so if like set(),
>you could use set() to save value.

### Write in a batch
If you want to do a sequence of updates and want to make it atomically,
then writebatch will be your friend.

>The WriteBatch holds a sequence of edits to be made to the database, 
>and these edits within the batch are applied in order.
>
>Apart from its atomicity benefits, WriteBatch may also be used to speed up
>bulk updates by placing lots of individual mutations into the same batch.

````php
<?php

$db = new LevelDB("/path/to/leveldb-test-db");

$batch = new LevelDBWriteBatch();
$batch->put("key2", "batch value");
$batch->put("key3", "batch value");
$batch->set("key4", "a bounch of values"); // set() is an alias of put()
$batch->delete("some key");

// Write once
$db->write($batch);
````

### Iterate through db

You can iterate through the whole database by iteration:

````php
<?php

$db = new LevelDB("/path/to/leveldb-test-db");
$it = new LevelDBIterator($db); // equals to： $it = $db->getIterator();

// Loop in iterator style
while($it->valid()) {
	var_dump($it->key() . " => " . $it->current() . "\n");
}

// Or loop in foreach
foreach($it as $key => $value) {
	echo "{$key} => {$value}\n";
}
````

If you want to iterate by reverse order, you could:

````php
<?php

$db = new LevelDB("/path/to/leveldb-test-db");
$it = new LevelDBIterator($db);

for($it->last(); $it->valid(); $it->prev()) {
	echo "{$key} => {$value}\n";
}

/*
 * And you could seek with: rewind(), next(), prev(), seek()
 */
````

>**NOTE** In LevelDB LevelDB::seek() will success even when the key doesn't exist.
>It will seek to the latest key:
>`db-with-key('a', 'b', 'd', 'e');  $db->seek('c');` iterator will point to `key 'd'`

### Snapshots
Snapshots provide consistent read-only views over the entire state of the key-value store.
`$read_options['snapshot']` may be non-NULL to indicate that a read should operate on a
particular version of the DB state. If `$read_options['snapshot']` is NULL, the read will
operate on an implicit snapshot of the current state.

Snapshots are created by the LevelDB::getSnapshot() method:

````php
<?php

$db = new LevelDB("you-db-path.db");
$db->put("key1", "value1");
$db->put("key2", "value2");

$snapshot = $db->getSnapshot();

$db->put("key3", "value3");

$read_options = array("snapshot" => $snapshot);
$db->get("key3", $read_options); // false but not "value3"
$db->get("key3"); // "value3" since not read from snapshot


$it = $db->getIterator($read_options);
foreach($it as $k => $v) {
	printf("$k => $v\n");
}
/*
Output:
key1 => value1
key2 => value2

key3 was not found because we are reading from snapshot
*/

?>
````

## Operations on database

### LevelDB::close()
Since leveldb can only accessed by a single proccess one time, you may want to
close it when you aren't using it anymore.

````php
<?php

$db = new LevelDB("/path/to/leveldb-test-db");
$it = new LevelDBIterator($db);
$db->close();
$it->next();				// noop you can't do that, exception thrown
$db->set("key", "value");	// you can't do this either
````

after database been closed, you can't do anything on it.

### LevelDB::destroy()
If you want to destroy a database, you could delete the whole database by hand:
`rm -rf /path/to/db` or you could use `LevelDB::destroy('/path/to/db')`.

Be careful with this.

>**NOTE**
>Before you destroy a database, please make sure it was closed. or
>an exception will thrown. (LevelDB >= 1.7.0)

### LevelDB::repair()
If you can't open a database, neither been locked or other error, if it's corrupted,
you could use `LevelDB::repair('/path/to/db')` to repair it. It will try to recover
as much data as possible.

## Known limitations

LevelDB was designed to be thread-safe but not process-safe, so if you
use php in multi-process mode (eg, php-fpm) you might not able to open
the single db concurrently.

> If you app is not designed to serve massive requests, you could try
> to catch fail to open exception and try to open it again.

## Thanks

Thanks Arpad <https://github.com/arraypad> for his original implementation: <http://github.com/arraypad/php-leveldb>
and his generous.

## Reference
More info could be found at:

- LevelDB project home: <https://github.com/google/leveldb>
- LevelDB document: <https://github.com/google/leveldb/blob/master/doc/index.md>
- A LevelDB internals analysis in Chinese <http://dirlt.com/LevelDB.html>

## License
PHP-LevelDB is licensed under the PHP License
