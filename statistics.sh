#!/bin/bash

workers=""
regx='\s*\[(\#[0-9]+)\] Ready to work'
regx2='\s*\[(\#[0-9]+)\] Client [0-9]+'
regx3='REQ_READ\s*-\s*([0-9]+) B'
regx4='REQ_WRITE\s*-\s*([0-9]+) B'
regx5='REQ_APPEND\s*-\s*([0-9]+) B'
regx6='CACHE OVLD!'
regx7='REQ_LOCK\s*-\s*([0-9]+) B'
regx8='REQ_UNLOCK\s*-\s*([0-9]+) B'
regx9='REQ_OPENLOCK\s*-\s*([0-9]+) B'
regx10='REQ_CLOSEFILE\s*-\s*([0-9]+) B'
regx11='\s*V_: ([0-9]+) ([0-9]+)'
regx12='Client [0-9]+ connected'
regx13='Client [0-9]+ disconnected'

declare -a nwrkes=()
nreads=0
sizereads=0
nwrite=0
sizewrites=0
novld=0
nlock=0
nunlock=0
nopenlock=0
nclose=0
nmaxfiles=0
nmaxsize=0
nconn=0
nconnmax=0

while read line; do

    if [[ $line =~ $regx ]]; then
        workers="$workers ${BASH_REMATCH[1]}"
        nwrkes+=(0)
    elif [[ $line =~ $regx2 ]]; then 
        i=0
        for wrk in $workers; do
            if [ $wrk == ${BASH_REMATCH[1]} ]; then
                nwrkes[$i]=$((${nwrkes[$i]}+1))
                break
            fi
            i=$((i+1))
        done
    elif [[ $line =~ $regx3 ]]; then 
        nreads=$((nreads+1))
        sizereads=$((sizereads+${BASH_REMATCH[1]}))
    elif [[ $line =~ $regx4 ]] || [[ $line =~ $regx5 ]]; then 
        nwrite=$((nwrite+1))
        sizewrites=$((sizewrites+${BASH_REMATCH[1]}))
    elif [[ $line =~ $regx6 ]]; then 
        novld=$((novld+1))
    elif [[ $line =~ $regx7 ]]; then 
        nlock=$((nlock+1))
    elif [[ $line =~ $regx8 ]]; then 
        nunlock=$((nunlock+1))
    elif [[ $line =~ $regx9 ]]; then 
        nopenlock=$((nopenlock+1))
    elif [[ $line =~ $regx10 ]]; then 
        nclose=$((nclose+1))
    elif [[ $line =~ $regx12 ]]; then 
        nconn=$((nconn+1))
        if [ $nconn -gt $nconnmax ]; then
            nconnmax=$nconn
        fi
    elif [[ $line =~ $regx13 ]]; then 
        nconn=$((nconn-1))
    fi

    if [[ $line =~ $regx11 ]]; then 
        nmaxfiles=${BASH_REMATCH[1]}
        nmaxsize=${BASH_REMATCH[2]}
    fi

done < $1

echo "Number of reads $nreads - Average size $((sizereads/nreads))"
echo "Number of writes $nwrite - Average size $((sizewrites/nwrite))"
echo "Number of cache replacement $novld"
echo "Number of locks $nlock"
echo "Number of unlocks $nunlock"
echo "Number of open-locks $nopenlock"
echo "Number of close $nclose"
echo "Number of max stored files: $nmaxfiles - Max total storage: $nmaxsize"
echo "Number of max parallel connection $nconnmax"
echo "Number of requests managed by each worker thread ${nwrkes[@]}"
