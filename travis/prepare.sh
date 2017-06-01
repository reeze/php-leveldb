#!/bin/sh

curl -fsSL https://github.com/google/leveldb/archive/v$LEVELDB_VERSION.tar.gz -o leveldb-$LEVELDB_VERSION.tar.gz
tar zxvf leveldb-$LEVELDB_VERSION.tar.gz
cd leveldb-$LEVELDB_VERSION && make
if [ "$SHLIB_DIR" != "" ]; then
	cp $SHLIB_DIR/libleveldb.* .
fi
cd ..
phpize && ./configure --with-leveldb=$PWD/leveldb-$LEVELDB_VERSION && make
