#!/bin/bash
shopt -s extglob

rm -r !(.*|*.sh|*.pdf|*.md|src|Makefile|sop-backup)

./setup.sh