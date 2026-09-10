#!/bin/sh

set -e

qemu-system-x86_64 -drive format=raw,file=build/os.img