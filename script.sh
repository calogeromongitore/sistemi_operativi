#!/bin/bash

while true; do
    ./$1 -f /tmp/socketfile.sk -W $2

    for i in 1 2 3 4 5 6 7 8 9 10; do
        ./$1 -f /tmp/socketfile.sk -W $2.$i -r $2 -t 0&
    done

    echo "wait"
    wait

    for i in 1 2 3 4 5 6 7 8 9 10; do
        ./$1 -f /tmp/socketfile.sk -c $2.$i -r $2 -t 0&
    done

    wait
    break
done

