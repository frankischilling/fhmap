#!/bin/sh

set -eu

binary=./build/bench-compare/fhmap
permuted_binary=./build/bench-compare/fhmap_permuted_hits
stdout_file=$(mktemp)
stderr_file=$(mktemp)
trap 'rm -f "$stdout_file" "$stderr_file"' EXIT HUP INT TERM

output=$($binary --smoke --count 32 --samples 3)
rows=$(printf '%s\n' "$output" |
    awk -F '\t' '$1 == "result" { count++ } END { print count + 0 }')
test "$rows" -eq 14

printf '%s\n' "$output" | awk -F '\t' '
    BEGIN {
        split("insert_reserved insert_growing update lookup_hit lookup_miss erase iterate", workloads, " ")
    }
    {
        if (NF != 15 || $1 != "result" || $2 != "fhmap") exit 1
        if ($3 != "u64" && $3 != "cstr") exit 1
        known = 0
        for (i = 1; i <= 7; ++i) {
            if ($4 == workloads[i]) known = 1
        }
        if (!known || $5 != "32" || $6 != "3") exit 1
        if ($7 !~ /^[0-9]+([.][0-9]+)?$/ ||
            $8 !~ /^[0-9]+([.][0-9]+)?$/) exit 1
        if (($7 + 0) <= 0 || ($8 + 0) < 0) exit 1
        if ($9 != "ok" || $10 !~ /^[0-9]+$/) exit 1
        for (i = 11; i <= 15; ++i) {
            if ($i !~ /^[0-9]+$/) exit 1
        }
        seen[$3 SUBSEP $4]++
    }
    END {
        if (NR != 14) exit 1
        for (kind = 1; kind <= 2; ++kind) {
            key_kind = kind == 1 ? "u64" : "cstr"
            for (i = 1; i <= 7; ++i) {
                if (seen[key_kind SUBSEP workloads[i]] != 1) exit 1
            }
        }
    }
'

seed_zero_output=$($binary --seed 0 --count 32 --samples 3)
seed_zero_rows=$(printf '%s\n' "$seed_zero_output" |
    awk -F '\t' '$1 == "result" { count++ } END { print count + 0 }')
test "$seed_zero_rows" -eq 14
printf '%s\n' "$seed_zero_output" | awk -F '\t' '
    BEGIN {
        split("insert_reserved insert_growing update lookup_hit lookup_miss erase iterate", workloads, " ")
    }
    {
        if (NF != 15 || $1 != "result" || $2 != "fhmap") exit 1
        if ($3 != "u64" && $3 != "cstr") exit 1
        if ($5 != "32" || $6 != "3" || $9 != "ok") exit 1
        if ($7 !~ /^[0-9]+([.][0-9]+)?$/ ||
            $8 !~ /^[0-9]+([.][0-9]+)?$/ ||
            $10 !~ /^[0-9]+$/) exit 1
        if (($7 + 0) <= 0 || ($8 + 0) < 0) exit 1
        for (i = 11; i <= 15; ++i) {
            if ($i !~ /^[0-9]+$/) exit 1
        }
        known = 0
        for (i = 1; i <= 7; ++i) {
            if ($4 == workloads[i]) known = 1
        }
        if (!known) exit 1
        seen[$3 SUBSEP $4]++
    }
    END {
        if (NR != 14) exit 1
        for (kind = 1; kind <= 2; ++kind) {
            key_kind = kind == 1 ? "u64" : "cstr"
            for (i = 1; i <= 7; ++i) {
                if (seen[key_kind SUBSEP workloads[i]] != 1) exit 1
            }
        }
    }
'

tiny_output=$($binary --smoke --count 1 --samples 1)
tiny_rows=$(printf '%s\n' "$tiny_output" |
    awk -F '\t' '$1 == "result" { count++ } END { print count + 0 }')
test "$tiny_rows" -eq 14
printf '%s\n' "$tiny_output" | awk -F '\t' '
    BEGIN {
        split("insert_reserved insert_growing update lookup_hit lookup_miss erase iterate", workloads, " ")
    }
    {
        if (NF != 15 || $1 != "result" || $2 != "fhmap") exit 1
        if ($3 != "u64" && $3 != "cstr") exit 1
        if ($5 != "1" || $6 != "1" || $9 != "ok") exit 1
        if ($7 !~ /^[0-9]+([.][0-9]+)?$/ ||
            $8 !~ /^[0-9]+([.][0-9]+)?$/ ||
            $10 !~ /^[0-9]+$/) exit 1
        if (($7 + 0) <= 0 || ($8 + 0) < 0) exit 1
        for (i = 11; i <= 15; ++i) {
            if ($i !~ /^[0-9]+$/) exit 1
        }
        known = 0
        for (i = 1; i <= 7; ++i) {
            if ($4 == workloads[i]) known = 1
        }
        if (!known) exit 1
        seen[$3 SUBSEP $4]++
    }
    END {
        if (NR != 14) exit 1
        for (kind = 1; kind <= 2; ++kind) {
            key_kind = kind == 1 ? "u64" : "cstr"
            for (i = 1; i <= 7; ++i) {
                if (seen[key_kind SUBSEP workloads[i]] != 1) exit 1
            }
        }
    }
'

expect_bad_arguments()
{
    : >"$stdout_file"
    : >"$stderr_file"
    if "$binary" "$@" >"$stdout_file" 2>"$stderr_file"; then
        printf 'accepted invalid arguments:' >&2
        printf ' %s' "$@" >&2
        printf '\n' >&2
        return 1
    fi
    test ! -s "$stdout_file"
    test -s "$stderr_file"
}

expect_bad_arguments --count
expect_bad_arguments --count nope
expect_bad_arguments --count 0
expect_bad_arguments --count 18446744073709551616
expect_bad_arguments --samples
expect_bad_arguments --samples nope
expect_bad_arguments --samples 0
expect_bad_arguments --seed
expect_bad_arguments --seed nope
expect_bad_arguments --seed 18446744073709551616
expect_bad_arguments --unknown
expect_bad_arguments --count 16 --count 32
expect_bad_arguments --samples 2 --samples 3
expect_bad_arguments --seed 1 --seed 2
expect_bad_arguments --smoke --smoke

if "$permuted_binary" --smoke --count 32 --samples 1 \
        >"$stdout_file" 2>"$stderr_file"; then
    printf 'accepted permuted lookup-hit values\n' >&2
    exit 1
fi
grep 'lookup_hit: timed operation result mismatch' "$stderr_file" >/dev/null

printf 'benchmark comparison smoke tests passed\n'
