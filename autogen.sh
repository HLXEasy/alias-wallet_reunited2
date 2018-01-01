#!/bin/bash -e
git submodule update --init

autoreconf --no-recursive --install

pushd tor
git checkout -f .
patch --no-backup-if-mismatch -f -p0 < ../tor-or-am.patch
patch --no-backup-if-mismatch -f -p0 < ../tor-am.patch
./autogen.sh
popd

pushd leveldb
git checkout -f .
patch --no-backup-if-mismatch -f -p1 < ../leveldb.patch
popd
