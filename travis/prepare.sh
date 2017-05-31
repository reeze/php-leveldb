#!/bin/sh

curl -fsSL https://github.com/google/leveldb/archive/v$LEVELDB_VERSION.tar.gz -o leveldb-$LEVELDB_VERSION.tar.gz
tar zxvf leveldb-$LEVELDB_VERSION.tar.gz
cd leveldb-$LEVELDB_VERSION && make
cd ..
phpize && ./configure --with-leveldb=$PWD/leveldb-$LEVELDB_VERSION && make
