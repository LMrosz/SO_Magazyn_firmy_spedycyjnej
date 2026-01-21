#!/bin/bash

SESJA="magazyn"

tmux kill-session -t "$SESJA" 2>/dev/null

make clean && make || exit 1

tmux new-session -d -s "$SESJA"
tmux send-keys -t "$SESJA" "clear && ./magazyn && echo '' && echo '=== Symulacja zakonczona ===' && echo 'Nacisnij Enter aby zamknac...' && read && tmux kill-session -t $SESJA" C-m

tmux attach -t "$SESJA"
