#!/bin/sh

set -eu

repo_root=$(pwd -P)
fetcher=$repo_root/bench/compare/fetch-competitors.sh
lock=$repo_root/bench/compare/competitors.lock
lock_test_dir=$(mktemp -d)
git_calls=$lock_test_dir/git-calls
mkdir "$lock_test_dir/bin"
printf '%s\n' '#!/bin/sh' \
    "printf '%s\\n' \"\$*\" >>\"\$BENCH_GIT_CALLS\"" 'exit 97' \
    >"$lock_test_dir/bin/git"
chmod +x "$lock_test_dir/bin/git"

binary=./build/bench-compare/fhmap
permuted_binary=./build/bench-compare/fhmap_permuted_hits
stdout_file=$(mktemp)
stderr_file=$(mktemp)
cleanup()
{
    rm -f "$stdout_file" "$stderr_file" "$git_calls" \
        "$lock_test_dir/bin/git"
    rmdir "$lock_test_dir/bin" "$lock_test_dir" 2>/dev/null || :
}
trap cleanup EXIT HUP INT TERM

(
    cd "$lock_test_dir"
    BENCH_GIT_CALLS=$git_calls \
    PATH=$lock_test_dir/bin:$PATH \
        "$fetcher" --check-lock
)
test ! -e "$git_calls"
test "$(find "$lock_test_dir" -mindepth 1 -maxdepth 1 ! -name bin | wc -l)" -eq 0

awk -F '\t' '
    BEGIN {
        split("cc verstable khashl khash mlib stc stb_ds uthash tidwall pottery", names, " ")
        for (i = 1; i <= 10; ++i) expected[names[i]] = 1
    }
    /^$/ || /^#/ { next }
    {
        records++
        if (index($0, "\r") != 0 || NF != 5) exit 1
        for (i = 1; i <= 5; ++i) {
            if ($i == "" || $i ~ /^[[:space:]]/ || $i ~ /[[:space:]]$/) exit 1
        }
        if (!($1 in expected) || seen_name[$1]++) exit 1
        if ($2 !~ /^https:\/\/github[.]com\/[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+[.]git$/ ||
            seen_repository[$2]++) exit 1
        if (length($3) != 40 || $3 !~ /^[0-9a-f]+$/) exit 1
        if (!safe_path($4)) exit 1
        source_count = split($5, sources, ",")
        if (source_count == 0) exit 1
        for (i = 1; i <= source_count; ++i) {
            if (!safe_path(sources[i])) exit 1
        }
    }
    END {
        if (records != 10) exit 1
        for (name in expected) {
            if (seen_name[name] != 1) exit 1
        }
    }
    function safe_path(path, count, parts, i) {
        if (path == "" || path ~ /^\// || path !~ /^[A-Za-z0-9._\/-]+$/) return 0
        count = split(path, parts, "/")
        for (i = 1; i <= count; ++i) {
            if (parts[i] == "" || parts[i] == "." || parts[i] == "..") return 0
        }
        return 1
    }
' "$lock"

test "$(awk -F '\t' '!/^#/ && !/^[[:space:]]*$/ { print $1 }' "$lock" |
    sort -u | wc -l)" -eq 10

expect_bad_fetch_arguments()
{
    : >"$stdout_file"
    : >"$stderr_file"
    if "$fetcher" "$@" >"$stdout_file" 2>"$stderr_file"; then
        printf 'fetcher accepted invalid arguments:' >&2
        printf ' %s' "$@" >&2
        printf '\n' >&2
        return 1
    fi
    test ! -s "$stdout_file"
    test -s "$stderr_file"
}

expect_bad_fetch_arguments
expect_bad_fetch_arguments --check-lock extra
expect_bad_fetch_arguments --dest
expect_bad_fetch_arguments --dest ''
expect_bad_fetch_arguments --dest /
expect_bad_fetch_arguments --dest "$lock_test_dir/destination" extra
expect_bad_fetch_arguments --unknown

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
