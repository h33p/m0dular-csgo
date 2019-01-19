#!/bin/sh

get_seeded_random()
{
    seed="$1"
    openssl enc -aes-256-ctr -pass pass:"$seed" -nosalt \
            </dev/zero 2>/dev/null
}

echo -n $(shuf -e $@)
