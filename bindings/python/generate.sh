#!/bin/sh

swig -o _preludedb.c -python -interface _preludedb -module _preludedb -noproxy ../libpreludedb.i
