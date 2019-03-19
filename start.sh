#!/bin/bash
set -e
cc alarm_cond.c -D_POSIX_PTHREAD_SEMANTICS -lpthread
./a.out
