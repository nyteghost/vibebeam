#!/usr/bin/env python3
"""Admit only successful, original beta build artifacts for recovery publication."""
import json
from pathlib import Path
import re
import sys


def validate(run, jobs):
    if (run.get('name') != 'Beta Arch Linux validation'
            or run.get('repository', {}).get('full_name') != 'nyteghost/vibebeam'
            or run.get('head_repository', {}).get('full_name') != 'nyteghost/vibebeam'
            or run.get('head_branch') != 'beta/host-download-20261028'
            or run.get('event') not in {'push', 'workflow_dispatch'}
            or run.get('status') != 'completed' or run.get('conclusion') != 'success'):
        raise ValueError('Not a successful original repository beta run')
    sha = run.get('head_sha', '')
    attempt = run.get('run_attempt')
    if not re.fullmatch('[0-9a-f]{40}', sha) or type(attempt) is not int or attempt < 1:
        raise ValueError('Invalid immutable build identity')
    if not isinstance(jobs.get('jobs'), list) or jobs.get('total_count') != len(jobs['jobs']):
        raise ValueError('Incomplete job list')
    for name in ['arch-beta / Archlinux', 'update-contract']:
        matches = [job for job in jobs['jobs'] if job.get('name') == name]
        if len(matches) != 1 or matches[0].get('conclusion') != 'success' or matches[0].get('status') != 'completed':
            raise ValueError('Required original build gate did not pass')
        if matches[0].get('run_id') != run['id'] or matches[0].get('head_sha') != sha:
            raise ValueError('Job belongs to another source run')
    return f'SOURCE_SHA={sha}\nATTEMPT={attempt}\n'


if __name__ == '__main__':
    print(validate(json.loads(Path(sys.argv[1]).read_text()),
                   json.loads(Path(sys.argv[2]).read_text())), end='')
