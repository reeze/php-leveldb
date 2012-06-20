#!/bin/sh

wget http://leveldb.googlecode.com/files/leveldb-1.4.0.tar.gz
tar zxvf leveldb-1.4.0.tar.gz
cd leveldb-1.4.0 && make && mkdir lib && cp libleveldb* lib
cd ..
phpize && ./configure --with-leveldb=$PWD/leveldb-1.4.0 && make
