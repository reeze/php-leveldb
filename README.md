# PHP-LevelDB: The PHP Binding for LevelDB
[![Build Status](https://secure.travis-ci.org/reeze/php-leveldb.png)](http://travis-ci.org/reeze/php-leveldb)

## Requirements
- PHP >= 5.2
- LevelDB 1.4 Install leveldb from: <http://code.google.com/p/leveldb/>

leveldb didn't have install in Makefile, you need to copy yourself.
eg:

	$ cd leveldb-1.4.0
	$ make
	$ cp -r include /usr/local
	$ cp -r libleveldb* /usr/local/lib

## Installation

1. Install from source

	$ git clone https://github.com/reeze/php-leveldb.git
	$ cd php-leveldb
	$ phpize
	* ./configure --enable-leveldb && make && make install

1. Install from PECL
	Didn't host in PECL Yet

## Usage

	<?php
	
	$db = new LevelDB("/path/to/leveldb-test-db");

	/*
     * Basic operate methods: set(), get(), delete()
	 */
	$db->set("Key", "Value");
	$db->get("Key");
	$db->delete("Key");

	/*
     * Or write in a batch
	 */
	$batch = new LevelDBWriteBatch();
	$batch->set("key2", "batch value");
	$batch->set("key3", "batch value");
	$batch->set("key4", "a bounch of values");
	$batch->delete("some key");

	// Write once
	$db->write($batch);

	/*
     * You could iterate through the whole db with Iterator
	$it = new LevelDBIterator($db);

	// Loop in iterator style
	while($it->valid()) {
		var_dump($it->key() . " => " . $it->current() . "\n");
	}

	// Or loop in foreach
	foreach($it as $key => $value) {
		echo "{$key} => {$value}\n";
	}

	/*
     * And you could seek with: rewind(), next(), prev(), seek() too!
     */

