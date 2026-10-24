"""Reject incomplete or invalid measurements before calculating a speedup."""
import importlib.util
from pathlib import Path
import unittest
import subprocess
import sys
import tempfile
import json
import math
import statistics

spec = importlib.util.spec_from_file_location('paired', Path(__file__).resolve().parents[1] / 'bench/compare/run-paired.py')
paired = importlib.util.module_from_spec(spec)
spec.loader.exec_module(paired)


class Rows(unittest.TestCase):
    def setUp(self):
        self.rows = '\n'.join('\t'.join(['result', 'fhmap', kind, workload, '32', '9', '10.0', '1.0', 'ok', '42', '32', '100', '200', '2', '64'])
                              for kind in ('u64', 'cstr') for workload in paired.WORKLOADS)

    def test_complete(self):
        self.assertEqual(len(paired.parse_rows(self.rows, 32, 9)), 16)

    def test_journal_path_cannot_be_final_report(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'results.jsonl'
            result = subprocess.run([sys.executable, spec.origin, '--baseline', 'unused',
                '--candidate', 'unused', '--report', str(report)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('different path', result.stderr)
            self.assertFalse(report.exists())

    def test_saved_probe_report(self):
        path = Path(__file__).resolve().parents[1] / 'bench/results/2026-10-24-probes-paired.json'
        report = json.loads(path.read_text())
        runs = {(r['round'], r['count'], r['binary']):
                paired.parse_rows(r['tsv'], r['count'], report['samples'])
                for r in report['runs']}
        self.assertEqual(len(runs), len(report['runs']))
        self.assertEqual(len(runs), report['rounds'] * len(report['counts']) * 2)
        seen = set()
        medians = []
        for scenario in report['scenarios']:
            count = scenario['count']
            key = scenario['key'], scenario['workload']
            self.assertNotIn((count, key), seen)
            seen.add((count, key))
            ratios = []
            for r in range(report['rounds']):
                before = runs[r, count, 'baseline'][key]
                after = runs[r, count, 'candidate'][key]
                self.assertEqual(before['metrics'], after['metrics'])
                ratios.append(before['median_ns'] / after['median_ns'])
            self.assertEqual(ratios, scenario['speedup_ratios'])
            median = statistics.median(ratios)
            self.assertAlmostEqual(median, scenario['median_speedup'], places=12)
            medians.append(median)
        self.assertEqual(len(seen), len(report['counts']) * 16)
        self.assertAlmostEqual(math.exp(statistics.mean(map(math.log, medians))),
                               report['geomean_speedup'], places=12)

    def test_reject_invalid(self):
        for invalid in (self.rows + '\n' + self.rows.splitlines()[0],
                        '\n'.join(self.rows.splitlines()[:-1]),
                        self.rows.replace('\t10.0\t', '\tnan\t', 1),
                        self.rows.replace('\t10.0\t', '\t0\t', 1),
                        self.rows.replace('\tok\t', '\tfail\t', 1),
                        self.rows.replace('\t1.0\t', '\t-1\t', 1),
                        self.rows.replace('\t42\t', '\t-1\t', 1)):
            with self.subTest(invalid=invalid[:80]):
                with self.assertRaises(ValueError):
                    paired.parse_rows(invalid, 32, 9)
        with self.assertRaises(ValueError):
            paired.parse_rows(self.rows, 64, 9)
        with self.assertRaises(ValueError):
            paired.parse_rows(self.rows, 32, 3)


if __name__ == '__main__':
    unittest.main()
