#!/bin/sh

set -eu

repo_root=$(pwd -P)
fetcher=$repo_root/bench/compare/fetch-competitors.sh
lock=$repo_root/bench/compare/competitors.lock
binary=./build/bench-compare/fhmap
permuted_binary=./build/bench-compare/fhmap_permuted_hits
fetch_test_dir=$(mktemp -d /tmp/fhmap-fetch-test.XXXXXX)
stdout_file=$fetch_test_dir/stdout
stderr_file=$fetch_test_dir/stderr
names='cc verstable khashl khash mlib stc stb_ds uthash tidwall pottery'

cleanup()
{
    case $fetch_test_dir in
        /tmp/fhmap-fetch-test.??????) rm -rf "$fetch_test_dir" ;;
        *) return 1 ;;
    esac
}
trap cleanup EXIT HUP INT TERM

expect_command_failure()
{
    : >"$stdout_file"
    : >"$stderr_file"
    if "$@" >"$stdout_file" 2>"$stderr_file"; then
        printf 'expected command to fail:' >&2
        printf ' %s' "$@" >&2
        printf '\n' >&2
        return 1
    fi
    test -s "$stderr_file"
}

offline_dir=$fetch_test_dir/offline
mkdir "$offline_dir" "$offline_dir/bin" "$offline_dir/cwd"
offline_git_calls=$offline_dir/git-calls
printf '%s\n' '#!/bin/sh' \
    "printf '%s\\n' \"\$*\" >>\"\$BENCH_GIT_CALLS\"" 'exit 97' \
    >"$offline_dir/bin/git"
chmod +x "$offline_dir/bin/git"

(
    cd "$offline_dir/cwd"
    BENCH_GIT_CALLS=$offline_git_calls \
    PATH=$offline_dir/bin:$PATH \
        "$fetcher" --check-lock
)
test ! -e "$offline_git_calls"
test -z "$(find "$offline_dir/cwd" -mindepth 1 -print -quit)"

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
expect_bad_fetch_arguments --dest -unsafe
expect_bad_fetch_arguments --dest "$fetch_test_dir/destination" extra
expect_bad_fetch_arguments --unknown

newline_dir=$fetch_test_dir/newline
mkdir "$newline_dir"
cp "$fetcher" "$newline_dir/fetch-competitors.sh"
lock_size=$(wc -c <"$lock")
dd if="$lock" of="$newline_dir/competitors.lock" bs=1 \
    count=$((lock_size - 1)) 2>/dev/null
expect_command_failure "$newline_dir/fetch-competitors.sh" --check-lock
: >"$newline_dir/competitors.lock"
expect_command_failure "$newline_dir/fetch-competitors.sh" --check-lock

fixture_dir=$fetch_test_dir/fixture
fixture_runner=$fixture_dir/runner
fixture_sources=$fixture_dir/sources
fixture_work=$fixture_dir/work
fixture_bin=$fixture_dir/bin
fixture_git_config=$fixture_dir/gitconfig
fixture_git_calls=$fixture_dir/git-calls
fixture_fetcher=$fixture_runner/fetch-competitors.sh
fixture_lock=$fixture_runner/competitors.lock
real_git=$(command -v git)
mkdir -p "$fixture_runner" "$fixture_sources" "$fixture_work" "$fixture_bin"
cp "$fetcher" "$fixture_fetcher"
: >"$fixture_lock"
wrong_cc_commit=

for name in $names
do
    work=$fixture_work/$name
    bare=$fixture_sources/$name.git
    "$real_git" init -q "$work"
    printf 'fixture license\n' >"$work/LICENSE"
    printf '%s fixture source\n' "$name" >"$work/source.h"
    "$real_git" -C "$work" add LICENSE source.h
    "$real_git" -C "$work" -c user.name=Fixture \
        -c user.email=fixture@example.com commit -q -m fixture
    commit=$("$real_git" -C "$work" rev-parse HEAD)
    if test "$name" = cc; then
        printf 'second fixture source\n' >>"$work/source.h"
        "$real_git" -C "$work" add source.h
        "$real_git" -C "$work" -c user.name=Fixture \
            -c user.email=fixture@example.com commit -q -m second
        wrong_cc_commit=$("$real_git" -C "$work" rev-parse HEAD)
    fi
    "$real_git" clone -q --bare "$work" "$bare"
    printf '%s\thttps://github.com/fhmap-fetch-test/%s.git\t%s\tLICENSE\tsource.h\n' \
        "$name" "$name" "$commit" >>"$fixture_lock"
done

"$real_git" config --file "$fixture_git_config" \
    "url.file://$fixture_sources/.insteadOf" \
    https://github.com/fhmap-fetch-test/

printf '%s\n' '#!/bin/sh' \
    "printf '%s\\n' \"\$*\" >>\"\$BENCH_GIT_CALLS\"" \
    "if test -n \"\${BENCH_FAIL_MATCH-}\"; then" \
    "    case \" \$* \" in" \
    "        *\"\$BENCH_FAIL_MATCH\"*) exit 89 ;;" \
    '    esac' \
    'fi' \
    "exec \"\$BENCH_REAL_GIT\" \"\$@\"" >"$fixture_bin/git"
chmod +x "$fixture_bin/git"

active_fetcher=$fixture_fetcher
fixture_fail_match=
run_fixture()
{
    BENCH_REAL_GIT=$real_git \
    BENCH_GIT_CALLS=$fixture_git_calls \
    BENCH_FAIL_MATCH=$fixture_fail_match \
    GIT_CONFIG_GLOBAL=$fixture_git_config \
    GIT_CONFIG_NOSYSTEM=1 \
    PATH=$fixture_bin:$PATH \
        "$active_fetcher" "$@"
}

expect_fixture_failure()
{
    : >"$stdout_file"
    : >"$stderr_file"
    if run_fixture "$@" >"$stdout_file" 2>"$stderr_file"; then
        printf 'fetcher accepted invalid checkout state:' >&2
        printf ' %s' "$@" >&2
        printf '\n' >&2
        return 1
    fi
    test -s "$stderr_file"
}

append_verify_calls()
{
    append_call_file=$1
    append_checkout=$2
    printf '%s\n' \
        "-C $append_checkout rev-parse --is-inside-work-tree" \
        "-C $append_checkout rev-parse --show-toplevel" \
        "-C $append_checkout rev-parse HEAD" \
        "-C $append_checkout symbolic-ref -q HEAD" \
        "-C $append_checkout config --local --get remote.origin.url" \
        "-C $append_checkout status --porcelain=v1 --untracked-files=all" \
        "-C $append_checkout ls-tree --name-only HEAD -- LICENSE" \
        "-C $append_checkout ls-tree --name-only HEAD -- source.h" \
        >>"$append_call_file"
}

write_runner_with_commit()
{
    runner=$1
    changed_name=$2
    changed_commit=$3
    mkdir "$runner"
    cp "$fetcher" "$runner/fetch-competitors.sh"
    awk -F '\t' -v OFS='\t' -v wanted="$changed_name" \
        -v commit="$changed_commit" \
        '{ if ($1 == wanted) $3 = commit; print }' \
        "$fixture_lock" >"$runner/competitors.lock"
}

checkout_state()
{
    checkout=$1
    "$real_git" -C "$checkout" rev-parse HEAD
    "$real_git" -C "$checkout" config --local --get remote.origin.url
    "$real_git" -C "$checkout" status --porcelain=v1 --untracked-files=all
    "$real_git" -C "$checkout" diff --binary | cksum
}

if test ! -e /cc && test ! -L /cc &&
        test ! -e /.fhmap-fetch-cc && test ! -L /.fhmap-fetch-cc; then
    : >"$fixture_git_calls"
    if BENCH_REAL_GIT=$real_git \
            BENCH_GIT_CALLS=$fixture_git_calls \
            BENCH_FAIL_MATCH=' clone ' \
            "$fixture_bin/git" clone --version >/dev/null 2>&1; then
        exit 1
    else
        intercept_status=$?
    fi
    test "$intercept_status" -eq 89
    test "$(sed -n '1p' "$fixture_git_calls")" = 'clone --version'

    : >"$fixture_git_calls"
    fixture_fail_match=' clone '
    expect_fixture_failure --dest /
    test ! -s "$fixture_git_calls"
    if test -d /proc/self/root; then
        expect_fixture_failure --dest /proc/self/root
        test ! -s "$fixture_git_calls"
    fi
    fixture_fail_match=
    test ! -e /cc
    test ! -L /cc
    test ! -e /.fhmap-fetch-cc
    test ! -L /.fhmap-fetch-cc
fi

fixture_destination=$fixture_dir/destination
expected_calls=$fixture_dir/expected-calls
: >"$fixture_git_calls"
: >"$expected_calls"
if ! run_fixture --dest "$fixture_destination" \
        >"$stdout_file" 2>"$stderr_file"; then
    sed 's/^/fixture fetch failed: /' "$stderr_file" >&2
    exit 1
fi
for name in $names
do
    repository=https://github.com/fhmap-fetch-test/$name.git
    commit=$(awk -F '\t' -v wanted="$name" '$1 == wanted { print $3 }' \
        "$fixture_lock")
    staging=$fixture_destination/.fhmap-fetch-$name
    checkout=$fixture_destination/$name
    printf '%s\n' \
        "clone --filter=blob:none --no-checkout $repository $staging" \
        "-C $staging fetch --filter=blob:none origin $commit" \
        "-C $staging checkout --detach $commit" >>"$expected_calls"
    append_verify_calls "$expected_calls" "$staging"
    append_verify_calls "$expected_calls" "$checkout"
    test -d "$checkout"
    test ! -e "$staging"
done
cmp "$expected_calls" "$fixture_git_calls"
stored_origin=$("$real_git" -C "$fixture_destination/cc" config --local \
    --get remote.origin.url)
rewritten_origin=$(GIT_CONFIG_GLOBAL=$fixture_git_config \
    GIT_CONFIG_NOSYSTEM=1 "$real_git" -C "$fixture_destination/cc" \
    remote get-url origin)
test "$stored_origin" = https://github.com/fhmap-fetch-test/cc.git
test "$rewritten_origin" != "$stored_origin"

: >"$fixture_git_calls"
: >"$expected_calls"
if ! run_fixture --dest "$fixture_destination" \
        >"$stdout_file" 2>"$stderr_file"; then
    sed 's/^/fixture verification failed: /' "$stderr_file" >&2
    exit 1
fi
for name in $names
do
    append_verify_calls "$expected_calls" "$fixture_destination/$name"
done
cmp "$expected_calls" "$fixture_git_calls"
test -z "$(grep -E '(^| )(clone|fetch|checkout)( |$)' \
    "$fixture_git_calls" || true)"

partial_destination=$fixture_dir/partial
: >"$fixture_git_calls"
fixture_fail_match=' fetch '
expect_fixture_failure --dest "$partial_destination"
fixture_fail_match=
test ! -e "$partial_destination/cc"
test -d "$partial_destination/.fhmap-fetch-cc"
partial_commit=$(awk -F '\t' '$1 == "cc" { print $3 }' "$fixture_lock")
printf '%s\n' \
    "clone --filter=blob:none --no-checkout https://github.com/fhmap-fetch-test/cc.git $partial_destination/.fhmap-fetch-cc" \
    "-C $partial_destination/.fhmap-fetch-cc fetch --filter=blob:none origin $partial_commit" \
    >"$expected_calls"
cmp "$expected_calls" "$fixture_git_calls"
grep 'inspect or remove.*before retrying' "$stderr_file" >/dev/null
: >"$fixture_git_calls"
expect_fixture_failure --dest "$partial_destination"
test ! -s "$fixture_git_calls"

wrong_head_destination=$fixture_dir/wrong-head
cp -R "$fixture_destination" "$wrong_head_destination"
"$real_git" -C "$wrong_head_destination/cc" checkout -q --detach \
    "$wrong_cc_commit"
before=$(checkout_state "$wrong_head_destination/cc")
expect_fixture_failure --dest "$wrong_head_destination"
after=$(checkout_state "$wrong_head_destination/cc")
test "$before" = "$after"

wrong_origin_destination=$fixture_dir/wrong-origin
cp -R "$fixture_destination" "$wrong_origin_destination"
"$real_git" -C "$wrong_origin_destination/cc" config remote.origin.url \
    https://github.com/fhmap-fetch-test/wrong.git
before=$(checkout_state "$wrong_origin_destination/cc")
expect_fixture_failure --dest "$wrong_origin_destination"
after=$(checkout_state "$wrong_origin_destination/cc")
test "$before" = "$after"

symlink_destination=$fixture_dir/symlink
mkdir "$symlink_destination"
ln -s "$fixture_destination/cc" "$symlink_destination/cc"
expect_fixture_failure --dest "$symlink_destination"
test -L "$symlink_destination/cc"
test ! -e "$symlink_destination/verstable"

nested_destination=$fixture_dir/nested
mkdir "$nested_destination"
"$real_git" init -q "$nested_destination"
mkdir "$nested_destination/cc"
printf 'nested license\n' >"$nested_destination/cc/LICENSE"
printf 'nested source\n' >"$nested_destination/cc/source.h"
"$real_git" -C "$nested_destination" add cc/LICENSE cc/source.h
"$real_git" -C "$nested_destination" -c user.name=Fixture \
    -c user.email=fixture@example.com commit -q -m nested
nested_commit=$("$real_git" -C "$nested_destination" rev-parse HEAD)
"$real_git" -C "$nested_destination" checkout -q --detach
"$real_git" -C "$nested_destination" remote add origin \
    https://github.com/fhmap-fetch-test/cc.git
nested_runner=$fixture_dir/nested-runner
write_runner_with_commit "$nested_runner" cc "$nested_commit"
active_fetcher=$nested_runner/fetch-competitors.sh
expect_fixture_failure --dest "$nested_destination"
test ! -e "$nested_destination/verstable"
active_fetcher=$fixture_fetcher

dirty_destination=$fixture_dir/dirty
cp -R "$fixture_destination" "$dirty_destination"
printf 'local edit\n' >>"$dirty_destination/cc/source.h"
before=$(checkout_state "$dirty_destination/cc")
expect_fixture_failure --dest "$dirty_destination"
after=$(checkout_state "$dirty_destination/cc")
test "$before" = "$after"

untracked_destination=$fixture_dir/untracked
cp -R "$fixture_destination" "$untracked_destination"
printf 'local file\n' >"$untracked_destination/cc/local.txt"
before=$(checkout_state "$untracked_destination/cc")
untracked_checksum=$(cksum <"$untracked_destination/cc/local.txt")
expect_fixture_failure --dest "$untracked_destination"
after=$(checkout_state "$untracked_destination/cc")
test "$before" = "$after"
test "$untracked_checksum" = "$(cksum <"$untracked_destination/cc/local.txt")"

missing_destination=$fixture_dir/missing
cp -R "$fixture_destination" "$missing_destination"
"$real_git" -C "$missing_destination/cc" rm -q source.h
"$real_git" -C "$missing_destination/cc" -c user.name=Fixture \
    -c user.email=fixture@example.com commit -q -m missing
missing_commit=$("$real_git" -C "$missing_destination/cc" rev-parse HEAD)
missing_runner=$fixture_dir/missing-runner
write_runner_with_commit "$missing_runner" cc "$missing_commit"
active_fetcher=$missing_runner/fetch-competitors.sh
before=$(checkout_state "$missing_destination/cc")
expect_fixture_failure --dest "$missing_destination"
after=$(checkout_state "$missing_destination/cc")
test "$before" = "$after"
test ! -e "$missing_destination/cc/source.h"

substitute_destination=$fixture_dir/substitute
cp -R "$missing_destination" "$substitute_destination"
printf '/source.h\n' >>"$substitute_destination/cc/.git/info/exclude"
printf 'untracked substitute\n' >"$substitute_destination/cc/source.h"
test -z "$("$real_git" -C "$substitute_destination/cc" status \
    --porcelain=v1 --untracked-files=all)"
before=$(checkout_state "$substitute_destination/cc")
substitute_checksum=$(cksum <"$substitute_destination/cc/source.h")
expect_fixture_failure --dest "$substitute_destination"
after=$(checkout_state "$substitute_destination/cc")
test "$before" = "$after"
test "$substitute_checksum" = \
    "$(cksum <"$substitute_destination/cc/source.h")"
active_fetcher=$fixture_fetcher

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
