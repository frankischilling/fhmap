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
    if test -n "${active_staging-}" &&
            { test -e "$active_staging" || test -L "$active_staging"; }; then
        printf '%s\n' \
            "fetch-competitors: inspect or remove the incomplete checkout before retrying: $active_staging" \
            >&2
    fi
    exit 1
}

validate_lock()
{
    test -f "$lock_file" || fail "lock file not found: $lock_file"
    test -s "$lock_file" || fail "lock file is empty: $lock_file"
    last_character=$(tail -c 1 "$lock_file") ||
        fail "could not read the end of the lock file: $lock_file"
    test -z "$last_character" ||
        fail "lock file must end with a newline: $lock_file"

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
    verify_name=$1
    verify_repository=$2
    verify_commit=$3
    verify_license_path=$4
    verify_source_paths=$5
    verify_target=$6

    test ! -L "$verify_target" ||
        fail "$verify_name checkout must not be a symbolic link: $verify_target"
    test -d "$verify_target" ||
        fail "$verify_name checkout is not a directory: $verify_target"
    verify_target_physical=$(CDPATH='' cd "$verify_target" && pwd -P) ||
        fail "could not resolve the $verify_name checkout path: $verify_target"
    verify_inside=$(git -C "$verify_target" rev-parse \
        --is-inside-work-tree 2>/dev/null) ||
        fail "$verify_name target is not a Git worktree: $verify_target"
    test "$verify_inside" = true ||
        fail "$verify_name target is not a Git worktree: $verify_target"
    verify_top_level=$(git -C "$verify_target" rev-parse \
        --show-toplevel 2>/dev/null) ||
        fail "could not find the $verify_name worktree root: $verify_target"
    verify_top_physical=$(CDPATH='' cd "$verify_top_level" && pwd -P) ||
        fail "could not resolve the $verify_name worktree root: $verify_top_level"
    test "$verify_top_physical" = "$verify_target_physical" ||
        fail "$verify_name target is nested inside another Git worktree: $verify_target"

    verify_head_commit=$(git -C "$verify_target" rev-parse HEAD 2>/dev/null) ||
        fail "could not read $verify_name HEAD"
    test "$verify_head_commit" = "$verify_commit" ||
        fail "$verify_name is at $verify_head_commit, expected $verify_commit"
    if git -C "$verify_target" symbolic-ref -q HEAD >/dev/null 2>&1; then
        fail "$verify_name checkout is attached to a branch"
    fi

    verify_origin=$(git -C "$verify_target" config --local --get \
        remote.origin.url 2>/dev/null) ||
        fail "$verify_name checkout has no origin"
    test "$verify_origin" = "$verify_repository" ||
        fail "$verify_name origin is $verify_origin, expected $verify_repository"

    verify_status=$(GIT_OPTIONAL_LOCKS=0 git -C "$verify_target" status \
        --porcelain=v1 --untracked-files=all 2>/dev/null) ||
        fail "could not read the $verify_name checkout status"
    test -z "$verify_status" ||
        fail "$verify_name checkout has local changes: $verify_target"

    test -f "$verify_target/$verify_license_path" ||
        fail "$verify_name license file is missing: $verify_license_path"
    verify_tracked_path=$(git -C "$verify_target" ls-tree --name-only HEAD -- \
        "$verify_license_path" 2>/dev/null) ||
        fail "could not inspect the $verify_name license path: $verify_license_path"
    test "$verify_tracked_path" = "$verify_license_path" ||
        fail "$verify_name license path is not tracked at HEAD: $verify_license_path"

    verify_remaining_paths=$verify_source_paths
    while test -n "$verify_remaining_paths"
    do
        case $verify_remaining_paths in
            *,*)
                verify_source_path=${verify_remaining_paths%%,*}
                verify_remaining_paths=${verify_remaining_paths#*,}
                ;;
            *)
                verify_source_path=$verify_remaining_paths
                verify_remaining_paths=
                ;;
        esac
        test -e "$verify_target/$verify_source_path" ||
            fail "$verify_name source path is missing: $verify_source_path"
        verify_tracked_path=$(git -C "$verify_target" ls-tree --name-only \
            HEAD -- "$verify_source_path" 2>/dev/null) ||
            fail "could not inspect the $verify_name source path: $verify_source_path"
        test "$verify_tracked_path" = "$verify_source_path" ||
            fail "$verify_name source path is not tracked at HEAD: $verify_source_path"
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
    destination=$(CDPATH='' cd "$destination" && pwd -P) ||
        fail "could not resolve destination: $destination"
    case $destination in
        ''|/|//) fail "refusing the filesystem root as a destination" ;;
    esac

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

        staging=$destination/.fhmap-fetch-$name
        if test -e "$staging" || test -L "$staging"; then
            fail "$name staging checkout already exists; inspect or remove it before retrying: $staging"
        fi

        active_staging=$staging
        printf 'Fetching %s at %s\n' "$name" "$commit"
        git clone --filter=blob:none --no-checkout "$repository" "$staging" ||
            fail "could not clone $name from $repository"
        git -C "$staging" fetch --filter=blob:none origin "$commit" ||
            fail "could not fetch $name commit $commit"
        git -C "$staging" checkout --detach "$commit" ||
            fail "could not check out $name commit $commit"
        verify_checkout "$name" "$repository" "$commit" "$license_path" \
            "$source_paths" "$staging"
        if test -e "$target" || test -L "$target"; then
            fail "$name target appeared while its checkout was being prepared: $target"
        fi
        mv "$staging" "$target" || fail "could not publish the $name checkout"
        active_staging=
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
