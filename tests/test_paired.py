"""Reject incomplete or invalid measurements before calculating a speedup."""
import importlib.util
from pathlib import Path
import unittest
import subprocess
import sys
import tempfile

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
