#!/bin/sh

make install
#make test

export TEST_PHP_EXECUTABLE=`which php`

php run-tests.php -d extension=leveldb.so -m tests/*.phpt

cat tests/*.mem

# make test didn't return status code correctly
# use this to find whether the make test failed
cat tests/*.diff

if [ $? -eq 0 ]; then
	exit 1;
fi
