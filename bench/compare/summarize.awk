#!/usr/bin/awk -f

BEGIN {
    FS = "\t"
    OFS = "\t"
    uint64_max = "18446744073709551615"

    map_count = split("fhmap cc verstable khashl khash mlib stc stb_ds uthash tidwall pottery", map_order, " ")
    kind_count = split("u64 cstr", kind_order, " ")
    workload_count = split("insert_reserved insert_growing update lookup_hit lookup_miss erase iterate churn", workload_order, " ")

    for (i = 1; i <= map_count; ++i) map_known[map_order[i]] = 1
    for (i = 1; i <= kind_count; ++i) kind_known[kind_order[i]] = 1
    for (i = 1; i <= workload_count; ++i)
        workload_known[workload_order[i]] = 1

    metadata_known["format"] = 1
    metadata_known["uname"] = 1
    metadata_known["cpu_model"] = 1
    metadata_known["compiler"] = 1
    metadata_known["compiler_flags"] = 1
    metadata_known["branch"] = 1
    metadata_known["commit"] = 1
    metadata_known["counts"] = 1
    metadata_known["samples"] = 1
    metadata_known["seed"] = 1
    metadata_known["cpu_pin"] = 1
    for (i = 2; i <= map_count; ++i)
        metadata_known["lock_" map_order[i]] = 1
}

index($0, "\r") != 0 {
    fail("carriage return in input")
}

$1 == "meta" {
    if (NF != 3 || !($2 in metadata_known) || $3 == "")
        fail("invalid metadata row")
    if (metadata_seen[$2]++) fail("duplicate metadata row")
    if ($2 == "format" && $3 != "fhmap-benchmark-v1")
        fail("unsupported report format")
    if (($2 == "commit" || $2 ~ /^lock_/) &&
        (length($3) != 40 || $3 !~ /^[0-9a-f]+$/))
        fail("invalid commit metadata")
    if ($2 == "counts" && !valid_count_list($3))
        fail("invalid count metadata")
    if ($2 == "samples" && !canonical_positive_uint64($3))
        fail("invalid sample metadata")
    if ($2 == "seed" && !canonical_uint64($3))
        fail("invalid seed metadata")
    if ($2 == "cpu_pin" && $3 != "none" && !canonical_uint64($3))
        fail("invalid CPU metadata")
    metadata_value[$2] = $3
    ++metadata_rows
    next
}

$1 == "rank" {
    if (NF != 7 || !canonical_positive_uint64($2) ||
        !($3 in kind_known) ||
        !($4 in workload_known) || !($5 in map_known) ||
        !positive_metric($6) || !positive_metric($7)) {
        fail("invalid rank row")
    }
    summary_key = $2 SUBSEP $3 SUBSEP $4 SUBSEP $5
    if (rank_seen[summary_key]++) fail("duplicate rank row")
    rank_metric[summary_key] = $6
    rank_value[summary_key] = $7
    ++rank_rows
    next
}

$1 == "geomean" {
    if (NF != 3 || !($2 in map_known) || !positive_metric($3))
        fail("invalid geometric mean row")
    if (geomean_seen[$2]++) fail("duplicate geometric mean row")
    geomean_value[$2] = $3
    ++geomean_rows
    next
}

{
    if (NF != 15 || $1 != "result") fail("invalid result schema")
    if (!($2 in map_known)) fail("unexpected adapter: " $2)
    if (!($3 in kind_known)) fail("unexpected key kind: " $3)
    if (!($4 in workload_known)) fail("unexpected workload: " $4)
    if (!canonical_positive_uint64($5) ||
        !canonical_positive_uint64($6))
        fail("invalid count or sample count")
    if (!positive_metric($7)) fail("invalid primary metric")
    if (!nonnegative_metric($8)) fail("invalid dispersion metric")
    if ($9 != "ok") fail("non-ok primary row")
    for (i = 10; i <= 15; ++i) {
        if (!canonical_uint64($i)) fail("invalid result accounting")
    }

    if (!count_seen[$5]++) {
        counts[++count_count] = $5
        input_counts = input_counts (input_counts == "" ? "" : ",") $5
    }
    if (($5 in sample_count) && sample_count[$5] != $6)
        fail("inconsistent sample count")
    sample_count[$5] = $6

    scenario = $5 SUBSEP $2 SUBSEP $3 SUBSEP $4
    if (scenario in median_ns) fail("duplicate scenario")
    median_ns[scenario] = $7 + 0
    ++rows
}

END {
    if (bad) exit 1
    if (rows == 0 || count_count == 0) fail_end("no primary rows")

    if (metadata_rows != 0) {
        if (metadata_rows != 10 + map_count)
            fail_end("partial metadata")
        for (name in metadata_known) {
            if (metadata_seen[name] != 1) fail_end("missing metadata")
        }
        if (metadata_value["counts"] != input_counts)
            fail_end("metadata count mismatch")
        for (count_text in sample_count) {
            if (metadata_value["samples"] != sample_count[count_text])
                fail_end("metadata sample mismatch")
        }
    }

    sort_counts()
    expected_rows = count_count * map_count * kind_count * workload_count
    if (rows != expected_rows) fail_end("partial result matrix")
    if ((rank_rows != 0 || geomean_rows != 0) &&
        (rank_rows != expected_rows || geomean_rows != map_count))
        fail_end("partial summary")

    for (ci = 1; ci <= count_count; ++ci) {
        count_text = counts[ci]
        for (ki = 1; ki <= kind_count; ++ki) {
            kind = kind_order[ki]
            for (wi = 1; wi <= workload_count; ++wi) {
                workload = workload_order[wi]
                for (mi = 1; mi <= map_count; ++mi) {
                    map = map_order[mi]
                    scenario = count_text SUBSEP map SUBSEP kind SUBSEP workload
                    if (!(scenario in median_ns))
                        fail_end("missing adapter scenario")
                }
                print_ranks(count_text, kind, workload)
            }
        }
    }

    total_scenarios = count_count * kind_count * workload_count
    for (mi = 1; mi <= map_count; ++mi) {
        map = map_order[mi]
        geometric_mean = exp(log_throughput[map] / total_scenarios)
        geometric_text = sprintf("%.6f", geometric_mean)
        if (geomean_rows != 0 && geomean_value[map] != geometric_text)
            fail_end("geometric mean mismatch")
        printf "geomean\t%s\t%s\n", map, geometric_text
    }
}

function print_ranks(count, kind, workload, mi, mj, map, other, key,
                     other_key, rank_key, metric, less, equal, rank) {
    for (mi = 1; mi <= map_count; ++mi) {
        map = map_order[mi]
        key = count SUBSEP map SUBSEP kind SUBSEP workload
        metric = median_ns[key]
        less = 0
        equal = 0
        for (mj = 1; mj <= map_count; ++mj) {
            other = map_order[mj]
            other_key = count SUBSEP other SUBSEP kind SUBSEP workload
            if (median_ns[other_key] < metric) ++less
            if (median_ns[other_key] == metric) ++equal
        }
        rank = 1 + less + (equal - 1) / 2
        rank_key = count SUBSEP kind SUBSEP workload SUBSEP map
        if (rank_rows != 0 &&
            (!(rank_key in rank_seen) ||
             rank_metric[rank_key] != sprintf("%.3f", metric) ||
             rank_value[rank_key] != sprintf("%.3f", rank))) {
            fail_end("rank mismatch")
        }
        printf "rank\t%s\t%s\t%s\t%s\t%.3f\t%.3f\n", \
            count, kind, workload, map, metric, rank
        log_throughput[map] += log(1000 / metric)
    }
}

function sort_counts(    i, j, value) {
    for (i = 2; i <= count_count; ++i) {
        value = counts[i]
        j = i - 1
        while (j >= 1 && decimal_greater(counts[j], value)) {
            counts[j + 1] = counts[j]
            --j
        }
        counts[j + 1] = value
    }
}

function canonical_uint64(value) {
    if (value !~ /^(0|[1-9][0-9]*)$/) return 0
    if (length(value) < length(uint64_max)) return 1
    if (length(value) > length(uint64_max)) return 0
    return !decimal_greater(value, uint64_max)
}

function canonical_positive_uint64(value) {
    return canonical_uint64(value) && value != "0"
}

function valid_count_list(value, count, parts, seen, i) {
    if (value !~ /^[0-9]+(,[0-9]+)*$/) return 0
    count = split(value, parts, ",")
    for (i = 1; i <= count; ++i) {
        if (!canonical_positive_uint64(parts[i]) || seen[parts[i]]++)
            return 0
    }
    return count > 0
}

function decimal_greater(left, right) {
    if (length(left) != length(right)) return length(left) > length(right)
    return ("x" left) > ("x" right)
}

function nonnegative_metric(value) {
    return value ~ /^[0-9]+([.][0-9]+)?$/ && length(value) <= 24 &&
           (value + 0) >= 0 && (value + 0) < 1e18
}

function positive_metric(value) {
    return nonnegative_metric(value) && (value + 0) > 0
}

function fail(message) {
    print "summarize.awk: " message | "cat 1>&2"
    close("cat 1>&2")
    bad = 1
    exit 1
}

function fail_end(message) {
    print "summarize.awk: " message | "cat 1>&2"
    close("cat 1>&2")
    exit 1
}
