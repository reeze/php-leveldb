#!/bin/sh

make test

# make test didn't return status code correctly
# use this to find whether the make test failed
cat tests/*.diff

if [ $? -eq 0 ]; then
	exit 1;
fi
