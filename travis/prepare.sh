#!/bin/sh

if [ ! -d leveldb-$LEVELDB_VERSION ] || [ ! -f leveldb-$LEVELDB_VERSION/libleveldb.so ]; then
	rm -rf leveldb-$LEVELDB_VERSION 2>/dev/null
	curl -fsSL https://github.com/google/leveldb/archive/v$LEVELDB_VERSION.tar.gz -o leveldb-$LEVELDB_VERSION.tar.gz
	tar zxvf leveldb-$LEVELDB_VERSION.tar.gz
	cd leveldb-$LEVELDB_VERSION && make -j4
	if [ "$SHLIB_DIR" != "" ]; then
		cp $SHLIB_DIR/libleveldb.* .
	fi
	cd ..
else
	echo "Using cached libleveldb build"
fi

phpize && ./configure --with-leveldb=$PWD/leveldb-$LEVELDB_VERSION && make -j4
