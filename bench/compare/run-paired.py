#!/usr/bin/env python3
"""Alternate two fhmap benchmark binaries on one CPU and retain every result."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import tempfile

WORKLOADS = ('insert_reserved', 'insert_growing', 'update', 'lookup_hit',
             'lookup_miss', 'erase', 'iterate', 'churn')


def parse_rows(output, count, samples):
    rows = {}
    for line in output.splitlines():
        fields = line.split('\t')
        if len(fields) != 15 or fields[:2] != ['result', 'fhmap']:
            raise ValueError('unexpected benchmark row')
        key = (fields[2], fields[3])
        if key in rows or key[0] not in ('u64', 'cstr') or key[1] not in WORKLOADS:
            raise ValueError('duplicate or unknown scenario')
        if int(fields[4]) != count or int(fields[5]) != samples or fields[8] != 'ok':
            raise ValueError('benchmark configuration or status mismatch')
        median, mad = float(fields[6]), float(fields[7])
        if not math.isfinite(median) or median <= 0 or not math.isfinite(mad) or mad < 0:
            raise ValueError('invalid timing')
        metrics = list(map(int, fields[9:]))
        if any(value < 0 for value in metrics):
            raise ValueError('invalid validation or memory metric')
        rows[key] = {'median_ns': median, 'mad_ns': mad, 'metrics': metrics}
    if len(rows) != 16:
        raise ValueError('incomplete benchmark matrix')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--counts', default='32,4096,262144,1048576')
    parser.add_argument('--samples', type=int, default=9)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--cpu', type=int, default=min(os.sched_getaffinity(0)))
    args = parser.parse_args()
    counts = [int(item) for item in args.counts.split(',')]
    if (not counts or len(set(counts)) != len(counts) or min(counts) < 1
            or args.samples < 1 or args.rounds < 2):
        parser.error('use unique positive counts, positive samples, and at least two rounds')
    if args.cpu not in os.sched_getaffinity(0):
        parser.error('CPU is outside this process affinity')
    if args.report.exists() or args.report.with_suffix('.jsonl').exists():
        parser.error('report destination already exists')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = {'format': 'fhmap-paired-v1', 'platform': platform.platform(),
              'counts': counts, 'samples': args.samples, 'rounds': args.rounds,
              'cpu_pin': args.cpu, 'seed': 7640891576956012809,
              'binaries': {}, 'runs': [], 'scenarios': []}
    report['cpu_model'] = next((line.split(':', 1)[1].strip()
        for line in Path('/proc/cpuinfo').read_text().splitlines()
        if line.startswith('model name')), 'unknown')
    with tempfile.TemporaryDirectory(prefix='fhmap-paired-') as temporary:
        for name in ('baseline', 'candidate'):
            source = getattr(args, name).resolve(strict=True)
            snapshot = Path(temporary) / name
            shutil.copyfile(source, snapshot)
            snapshot.chmod(0o700)
            report['binaries'][name] = {'path': str(source),
                'sha256': hashlib.sha256(snapshot.read_bytes()).hexdigest()}
        with args.report.with_suffix('.jsonl').open('x') as journal:
            journal.write(json.dumps({k: v for k, v in report.items()
                                      if k not in ('runs', 'scenarios')}) + '\n')
            for round_index in range(args.rounds):
                for count in counts:
                    order = ('baseline', 'candidate') if round_index % 2 == 0 else ('candidate', 'baseline')
                    for name in order:
                        print(f'Round {round_index + 1}, {count} entries: {name}', flush=True)
                        command = ['taskset', '-c', str(args.cpu), str(Path(temporary) / name),
                                   '--count', str(count), '--samples', str(args.samples),
                                   '--seed', str(report['seed'])]
                        output = subprocess.run(command, check=True, capture_output=True,
                                                text=True, timeout=1800).stdout
                        parse_rows(output, count, args.samples)
                        run = {'round': round_index, 'count': count, 'binary': name, 'tsv': output}
                        report['runs'].append(run)
                        journal.write(json.dumps(run) + '\n')
                        journal.flush()
        for count in counts:
            runs = {(r['round'], r['binary']): parse_rows(r['tsv'], count, args.samples)
                    for r in report['runs'] if r['count'] == count}
            for kind in ('u64', 'cstr'):
                for workload in WORKLOADS:
                    key = kind, workload
                    ratios = []
                    for r in range(args.rounds):
                        before, after = runs[r, 'baseline'][key], runs[r, 'candidate'][key]
                        if before['metrics'] != after['metrics']:
                            raise ValueError(f'validation/memory differs: {count} {key}')
                        ratios.append(before['median_ns'] / after['median_ns'])
                    report['scenarios'].append({'count': count, 'key': kind, 'workload': workload,
                        'speedup_ratios': ratios, 'median_speedup': statistics.median(ratios)})
        report['geomean_speedup'] = math.exp(statistics.mean(
            math.log(s['median_speedup']) for s in report['scenarios']))
        args.report.write_text(json.dumps(report, indent=2) + '\n')
        print(f"Equal-weight geometric mean speedup: {report['geomean_speedup']:.4f}x")
        print(args.report)


if __name__ == '__main__':
    main()
