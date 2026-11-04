import copy
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('recovery', ROOT / '.github/scripts/validate_host_recovery.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)


def fixture():
    run = dict(id=123, name='Beta Arch Linux validation', repository=dict(full_name='nyteghost/vibebeam'),
               head_repository=dict(full_name='nyteghost/vibebeam'), head_branch='beta/host-download-20261028',
               event='push', status='completed', conclusion='success', head_sha='a' * 40, run_attempt=1)
    jobs = dict(total_count=2, jobs=[dict(name=name, status='completed', conclusion='success', run_id=123, head_sha='a' * 40)
                                   for name in ['arch-beta / Archlinux', 'update-contract']])
    return run, jobs


class RecoveryTests(unittest.TestCase):
    def test_successful_original_gates(self):
        run, jobs = fixture()
        self.assertEqual(r.validate(run, jobs), 'SOURCE_SHA=' + 'a' * 40 + '\nATTEMPT=1\n')

    def test_failed_fork_wrong_branch_and_incomplete_run_are_rejected(self):
        for key, value in [('conclusion', 'failure'), ('head_branch', 'master'), ('event', 'pull_request'),
                           ('head_sha', 'unknown'), ('run_attempt', True),
                           ('head_repository', dict(full_name='fork/vibebeam'))]:
            run, jobs = fixture()
            run[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): r.validate(run, jobs)

    def test_each_gate_must_pass_and_match_source(self):
        for key, value in [('conclusion', 'skipped'), ('status', 'in_progress'), ('run_id', 999), ('head_sha', 'b' * 40)]:
            run, jobs = fixture()
            jobs['jobs'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): r.validate(run, jobs)
        run, jobs = fixture()
        jobs['jobs'].pop()
        with self.assertRaises(ValueError): r.validate(run, jobs)
