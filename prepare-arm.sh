#!/bin/bash

export CC=clang
export CXX=clang++

meson setup build-arm --cross-file armv7-clang-crossfile.ini --buildtype=release --wipe