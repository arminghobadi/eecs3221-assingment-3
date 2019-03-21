#!/bin/bash
set -e
clear
cc alarm_cond.c -D_POSIX_PTHREAD_SEMANTICS -lpthread
./a.out