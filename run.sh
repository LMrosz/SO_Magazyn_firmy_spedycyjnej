#!/bin/bash

SESJA="magazyn"

tmux kill-session -t "$SESJA" 2>/dev/null
make clean && make || exit 1
tmux new-session -s "$SESJA" './magazyn'