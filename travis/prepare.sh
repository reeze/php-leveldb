#!/bin/sh

wget http://leveldb.googlecode.com/files/leveldb-$LEVELDB_VERSION.tar.gz
tar zxvf leveldb-$LEVELDB_VERSION.tar.gz
cd leveldb-$LEVELDB_VERSION && make
cd ..
phpize && ./configure --with-leveldb=$PWD/leveldb-$LEVELDB_VERSION && make
