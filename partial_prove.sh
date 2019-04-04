#!/bin/bash

if [ $# -ne 6 ]
then
	exit 1
fi

params=$1
pws=$2
input=$3
partial=$4
name=$5

cd stuff
./pepper_partial_prover_$name $params $pws $input $partial
