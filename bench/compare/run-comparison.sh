#!/bin/sh

set -eu

script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
repo_root=$(CDPATH='' cd -- "$script_dir/../.." && pwd -P)
build_dir=$repo_root/build/bench-compare
lock_file=$script_dir/competitors.lock
summarizer=$script_dir/summarize.awk
maps='fhmap cc verstable khashl khash mlib stc stb_ds uthash tidwall pottery'
uint64_max=18446744073709551615
counts=${FHMAP_BENCH_COUNTS-32,4096,262144,1048576}
samples=${FHMAP_BENCH_SAMPLES-9}
seed=${FHMAP_BENCH_SEED-7640891576956012809}
pin_cpu=${FHMAP_BENCH_CPU-0}
bench_cc=${FHMAP_BENCH_CC:-${CC:-cc}}
bench_flags=${FHMAP_BENCH_CFLAGS:--std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O3 -DNDEBUG -march=native}

fail()
{
    printf 'run-comparison.sh: %s\n' "$1" >&2
    exit 1
}

is_canonical_uint64()
{
    LC_ALL=C awk -v value="$1" -v maximum="$uint64_max" '
        BEGIN {
            if (value !~ /^(0|[1-9][0-9]*)$/) exit 1
            if (length(value) < length(maximum)) exit 0
            if (length(value) > length(maximum)) exit 1
            exit (("x" value) > ("x" maximum))
        }
    ' </dev/null
}

is_canonical_positive_uint64()
{
    is_canonical_uint64 "$1" && test "$1" != 0
}

case $counts in
    ''|*[!0-9,]*|,*|*,|*,,*) fail 'invalid FHMAP_BENCH_COUNTS' ;;
esac
is_canonical_positive_uint64 "$samples" ||
    fail 'invalid FHMAP_BENCH_SAMPLES'
is_canonical_uint64 "$seed" || fail 'invalid FHMAP_BENCH_SEED'
is_canonical_uint64 "$pin_cpu" || fail 'invalid FHMAP_BENCH_CPU'
case $bench_cc in
    ''|*[!A-Za-z0-9_./+-]*) fail 'invalid benchmark compiler command' ;;
esac

remaining_counts=$counts
seen_counts=,
while :
do
    case $remaining_counts in
        *,*)
            count=${remaining_counts%%,*}
            remaining_counts=${remaining_counts#*,}
            ;;
        *)
            count=$remaining_counts
            remaining_counts=
            ;;
    esac
    is_canonical_positive_uint64 "$count" ||
        fail 'invalid FHMAP_BENCH_COUNTS'
    case $seen_counts in
        *,"$count",*) fail 'duplicate FHMAP_BENCH_COUNTS entry' ;;
    esac
    seen_counts=$seen_counts$count,
    test -n "$remaining_counts" || break
done

command -v "$bench_cc" >/dev/null 2>&1 || fail 'benchmark compiler not found'
test -r "$lock_file" || fail 'competitor lock file not found'
test -r "$summarizer" || fail 'benchmark summarizer not found'
"$script_dir/fetch-competitors.sh" --check-lock
for map in $maps
do
    test -x "$build_dir/$map" || fail "adapter binary not found: $map"
done

cpu_pin=none
if command -v taskset >/dev/null 2>&1 &&
        taskset -c "$pin_cpu" true >/dev/null 2>&1; then
    cpu_pin=$pin_cpu
elif test "${FHMAP_BENCH_CPU+x}" = x; then
    fail "requested CPU is unavailable: $pin_cpu"
fi

temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/fhmap-comparison.XXXXXX") ||
    fail 'could not create a temporary directory'
result_file=$temp_dir/results.tsv

cleanup()
{
    case $temp_dir in
        "${TMPDIR:-/tmp}"/fhmap-comparison.??????)
            rm -f "$result_file"
            rmdir "$temp_dir" 2>/dev/null || true
            ;;
    esac
}
trap cleanup EXIT HUP INT TERM

uname_value=$(uname -a) || fail 'could not read system information'
cpu_model=$(awk -F ':' '
    /^model name[[:space:]]*:/ {
        sub(/^[[:space:]]+/, "", $2)
        print $2
        exit
    }
' /proc/cpuinfo 2>/dev/null || true)
if test -z "$cpu_model" && command -v sysctl >/dev/null 2>&1; then
    cpu_model=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || true)
fi
test -n "$cpu_model" || cpu_model=unknown
compiler_output=$("$bench_cc" --version) ||
    fail 'could not read compiler version'
compiler_version=$(printf '%s\n' "$compiler_output" | sed -n '1p')
test -n "$compiler_version" || fail 'compiler version is empty'
branch=$(git -C "$repo_root" symbolic-ref --quiet --short HEAD ||
    printf 'detached')
commit=$(git -C "$repo_root" rev-parse --verify HEAD) ||
    fail 'could not read the current commit'

emit_metadata()
{
    metadata_value=$(printf '%s' "$2" | tr '\t\r\n' '   ')
    test -n "$metadata_value" || fail "empty metadata value: $1"
    printf 'meta\t%s\t%s\n' "$1" "$metadata_value"
}

emit_metadata format fhmap-benchmark-v1
emit_metadata uname "$uname_value"
emit_metadata cpu_model "$cpu_model"
emit_metadata compiler "$bench_cc $compiler_version"
emit_metadata compiler_flags "$bench_flags"
emit_metadata branch "$branch"
emit_metadata commit "$commit"
emit_metadata counts "$counts"
emit_metadata samples "$samples"
emit_metadata seed "$seed"
emit_metadata cpu_pin "$cpu_pin"

tab=$(printf '\t')
while IFS="$tab" read -r name _repository locked_commit _rest
do
    test -n "$name" || continue
    emit_metadata "lock_$name" "$locked_commit"
done <"$lock_file"

: >"$result_file"
remaining_counts=$counts
while :
do
    case $remaining_counts in
        *,*)
            count=${remaining_counts%%,*}
            remaining_counts=${remaining_counts#*,}
            ;;
        *)
            count=$remaining_counts
            remaining_counts=
            ;;
    esac
    for map in $maps
    do
        binary=$build_dir/$map
        if test "$cpu_pin" = none; then
            "$binary" --count "$count" --samples "$samples" --seed "$seed" \
                >>"$result_file"
        else
            taskset -c "$cpu_pin" "$binary" --count "$count" \
                --samples "$samples" --seed "$seed" >>"$result_file"
        fi
    done
    test -n "$remaining_counts" || break
done

cat "$result_file"
awk -f "$summarizer" "$result_file"
