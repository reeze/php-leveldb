#!/bin/sh

LDB_VERSION=1.7.0

wget http://leveldb.googlecode.com/files/leveldb-$LDB_VERSION.tar.gz
tar zxvf leveldb-$LDB_VERSION.tar.gz
cd leveldb-$LDB_VERSION && make
cd ..
phpize && ./configure --with-leveldb=$PWD/leveldb-$LDB_VERSION && make
