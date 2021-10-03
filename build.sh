#!/bin/bash

echo -e "build Geometry Generator"

if [ -d "./bin" ]
then
  echo -e "Directory ./bin exists."
  echo -e "Cleanup the previous build"
  if [ -f "./bin/gg.out" ]
  then
    echo -e "gg.out exists.. rename to old"
    if [ -f "./bin/gg.out.old" ]
    then
      echo -e "gg.old.out does exist. Delete it first"
      sudo rm "./bin/gg.out.old"
    fi
    echo -e rename gg.out to gg.out.old
    sudo mv ./bin/gg.out ./bin/gg.out.old
  fi
else
  echo -e "create the bin folder as it does not exist"
  sudo mkdir -p "./bin"
fi
ls -la ./bin
echo -e "build geometry_gen"
gcc ./src/geometry_gen.c -o ./bin/gg.out -I ./inc

ls -la ./bin

#build the other tools here.
