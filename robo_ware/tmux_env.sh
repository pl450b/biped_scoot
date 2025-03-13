#!/bin/bash

MY_IDF_PATH="~/esp/esp-idf"
SESSION="biped_dev"
SESSIONEXISTS=$(tmux list-sessions | grep $SESSION)

# Only create tmux session if it doesn't already exist
if [ "$SESSIONEXISTS" = "" ]
then
  tmux new-session -d -s $SESSION
  
  # Setup build environment
  tmux rename-window -t 0 'build'
  tmux send-keys -t 'build' 'source $MY_IDF_PATH/export.sh' C-m

  # Setup coding environment
  tmux new-window -t $SESSION:1 -n 'code'
  tmux send-keys -t 'code' 'cd ./main && nvim main.cpp' C-m
  tmux split-window -h
  tmux send-keys -t 'code' 'cd ./main && ls' C-m

  # Setup example viewing window
  tmux new-window -t $SESSION:2 -n 'examples'
  tmux send-keys -t 'examples' 'source $MY_IDF_PATH/export.sh' C-m
  tmux send-keys -t 'examples' 'cd $MY_IDF_PATH/examples/ && ls' C-m
fi 

# Attach Session, on the Main window
tmux attach-session -t $SESSION:1
