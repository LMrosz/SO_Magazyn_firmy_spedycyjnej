#!/bin/bash

SESJA="magazyn"

tmux kill-session -t "$SESJA" 2>/dev/null
make clean && make || exit 1
tmux new-session -s "$SESJA" -d 'bash -c "./magazyn; echo \"\"; echo \"=== Symulacja zakonczona ===\"; echo \"Nacisnij Enter aby zamknac...\"; read"'
tmux attach -t "$SESJA"