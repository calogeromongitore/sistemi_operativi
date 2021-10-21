#!/bin/bash

secs=30                           # Set interval (duration) in seconds.
endTime=$(( $(date +%s) + secs )) # Calculate end time.

j=0
while [ $(date +%s) -lt $endTime ]; do  # Loop until interval has elapsed.
    echo "loop $j - elapsed $SECONDS"
    j=$((j+1))

    ./$1 -f /tmp/socketfile.sk -W $2

    for i in 1 2 3 4 5 6 7 8 9 10; do
        ./$1 -f /tmp/socketfile.sk -W $2.$i -r $2 & 
    done
    wait

    for i in 1 2 3 4 5 6 7 8 9 10; do
        ./$1 -f /tmp/socketfile.sk -c $2.$i -r $2 -t 0&
    done
    wait
done

# while true; do
#     ./$1 -f /tmp/socketfile.sk -W $2

#     for i in 1 2 3 4 5 6 7 8 9 10; do
#         ./$1 -f /tmp/socketfile.sk -W $2.$i -r $2 & 
#     done
#     wait

#     for i in 1 2 3 4 5 6 7 8 9 10; do
#         ./$1 -f /tmp/socketfile.sk -c $2.$i -r $2 -t 0&
#     done
#     wait

#     break
# done

