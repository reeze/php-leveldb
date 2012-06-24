#!/bin/sh

wget http://leveldb.googlecode.com/files/leveldb-1.5.0.tar.gz
tar zxvf leveldb-1.5.0.tar.gz
cd leveldb-1.5.0 && make
cd ..
phpize && ./configure --with-leveldb=$PWD/leveldb-1.5.0 && make
