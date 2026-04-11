#!/bin/bash

export CC=clang
export CXX=clang++

meson setup build --buildtype=release --wipe