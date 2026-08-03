#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

script_dir=$(CDPATH='' cd "$(dirname "$0")" && pwd)
lock_file=$script_dir/competitors.lock
tab=$(printf '\t')

usage()
{
    printf 'usage: %s --check-lock | --dest DIR\n' "$0" >&2
    exit 2
}

fail()
{
    printf 'fetch-competitors: %s\n' "$1" >&2
    exit 1
}

validate_lock()
{
    test -f "$lock_file" || fail "lock file not found: $lock_file"

    awk -F '\t' '
        BEGIN {
            split("cc verstable khashl khash mlib stc stb_ds uthash tidwall pottery", names, " ")
            for (i = 1; i <= 10; ++i) expected[names[i]] = 1
        }
        {
            if (index($0, "\r") != 0) invalid("carriage return")
        }
        /^$/ || /^#/ { next }
        {
            records++
            if (NF != 5) invalid("expected five tab-separated fields")
            for (i = 1; i <= 5; ++i) {
                if ($i == "") invalid("empty field")
                if ($i ~ /^[[:space:]]/ || $i ~ /[[:space:]]$/)
                    invalid("leading or trailing whitespace")
            }
            if (!($1 in expected)) invalid("unknown competitor " $1)
            if (seen_name[$1]++) invalid("duplicate competitor " $1)
            if ($2 !~ /^https:\/\/github[.]com\/[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+[.]git$/)
                invalid("repository must be an HTTPS GitHub .git URL")
            if (seen_repository[$2]++) invalid("duplicate repository " $2)
            if (length($3) != 40 || $3 !~ /^[0-9a-f]+$/)
                invalid("commit must contain 40 lowercase hexadecimal characters")
            if (!safe_path($4)) invalid("unsafe license path " $4)
            source_count = split($5, sources, ",")
            if (source_count == 0) invalid("empty source list")
            for (i = 1; i <= source_count; ++i) {
                if (!safe_path(sources[i])) invalid("unsafe source path " sources[i])
            }
        }
        END {
            if (records != 10) invalid("expected ten competitor records")
            for (name in expected) {
                if (seen_name[name] != 1) invalid("missing competitor " name)
            }
        }
        function safe_path(path, count, parts, i) {
            if (path == "" || path ~ /^\// || path !~ /^[A-Za-z0-9._\/-]+$/)
                return 0
            count = split(path, parts, "/")
            for (i = 1; i <= count; ++i) {
                if (parts[i] == "" || parts[i] == "." || parts[i] == "..")
                    return 0
            }
            return 1
        }
        function invalid(message) {
            printf "%s:%d: %s\n", FILENAME, FNR, message > "/dev/stderr"
            exit 1
        }
    ' "$lock_file" || fail "lock validation failed"
}

verify_checkout()
{
    name=$1
    repository=$2
    commit=$3
    license_path=$4
    source_paths=$5
    target=$6

    test -d "$target" || fail "$name checkout is not a directory: $target"
    inside=$(git -C "$target" rev-parse --is-inside-work-tree 2>/dev/null) ||
        fail "$name target is not a Git worktree: $target"
    test "$inside" = true || fail "$name target is not a Git worktree: $target"

    head_commit=$(git -C "$target" rev-parse HEAD 2>/dev/null) ||
        fail "could not read $name HEAD"
    test "$head_commit" = "$commit" ||
        fail "$name is at $head_commit, expected $commit"
    if git -C "$target" symbolic-ref -q HEAD >/dev/null 2>&1; then
        fail "$name checkout is attached to a branch"
    fi

    origin=$(git -C "$target" remote get-url origin 2>/dev/null) ||
        fail "$name checkout has no origin"
    test "$origin" = "$repository" ||
        fail "$name origin is $origin, expected $repository"

    test -f "$target/$license_path" ||
        fail "$name license file is missing: $license_path"

    remaining_paths=$source_paths
    while test -n "$remaining_paths"
    do
        case $remaining_paths in
            *,*)
                source_path=${remaining_paths%%,*}
                remaining_paths=${remaining_paths#*,}
                ;;
            *)
                source_path=$remaining_paths
                remaining_paths=
                ;;
        esac
        test -e "$target/$source_path" ||
            fail "$name source path is missing: $source_path"
    done
}

fetch_all()
{
    destination=$1
    case $destination in
        ''|/|//|-) fail "refusing unsafe destination: $destination" ;;
        -*) fail "destination must not start with a dash: $destination" ;;
    esac

    if test -e "$destination" && ! test -d "$destination"; then
        fail "destination is not a directory: $destination"
    fi
    mkdir -p "$destination" || fail "could not create destination: $destination"
    destination=$(CDPATH='' cd "$destination" && pwd) ||
        fail "could not resolve destination: $destination"
    test "$destination" != / || fail "refusing the filesystem root as a destination"

    while IFS="$tab" read -r name repository commit license_path source_paths
    do
        case $name in
            ''|'#'*) continue ;;
        esac

        target=$destination/$name
        if test -e "$target" || test -L "$target"; then
            verify_checkout "$name" "$repository" "$commit" "$license_path" \
                "$source_paths" "$target"
            continue
        fi

        printf 'Fetching %s at %s\n' "$name" "$commit"
        git clone --filter=blob:none --no-checkout "$repository" "$target" ||
            fail "could not clone $name from $repository"
        git -C "$target" fetch --filter=blob:none origin "$commit" ||
            fail "could not fetch $name commit $commit"
        git -C "$target" checkout --detach "$commit" ||
            fail "could not check out $name commit $commit"
        verify_checkout "$name" "$repository" "$commit" "$license_path" \
            "$source_paths" "$target"
    done < "$lock_file"
}

case ${1-} in
    --check-lock)
        test "$#" -eq 1 || usage
        validate_lock
        ;;
    --dest)
        test "$#" -eq 2 || usage
        validate_lock
        fetch_all "$2"
        ;;
    *)
        usage
        ;;
esac
