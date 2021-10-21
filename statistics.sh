#!/bin/bash

workers=""
regx='\s*\[(\#[0-9]+)\] Ready to work'
regx2='\s*\[(\#[0-9]+)\] Client [0-9]+'
declare -a nwrkes=()
while read line; do

    if [[ $line =~ $regx ]]; then
        workers="$workers ${BASH_REMATCH[1]}"
        nwrkes+=(0)
    elif [[ $line =~ $regx2 ]]; then 
        i=0
        echo "incr wrk ${BASH_REMATCH[1]}"
        for wrk in $workers; do
            if [ $wrk == ${BASH_REMATCH[1]} ]; then
                echo "wrk found @ $i"
                nwrkes[$i]=$((${nwrkes[$i]}+1))
                echo ${nwrkes[@]}
                break
            fi
            i=$((i+1))
        done
    fi


done < $1