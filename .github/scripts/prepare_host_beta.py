#!/usr/bin/env python3
"""Validate a successful Arch artifact and produce the public beta manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile


def prepare(root, sha, run, attempt):
    if not re.fullmatch('[0-9a-f]{40}', sha):
        raise ValueError('Invalid source SHA')
    provenance = json.loads((root / 'linux-provenance.json').read_text())
    if provenance['source_commit'] != sha or str(provenance['workflow_run_id']) != str(run):
        raise ValueError('Artifact provenance mismatch')
    packages = list(root.glob('vibebeam-*-x86_64.pkg.tar.zst'))
    if len(packages) != 1:
        raise ValueError('Expected one x86_64 package')
    package = packages[0]
    checksum = hashlib.sha256(package.read_bytes()).hexdigest()
    if provenance['assets'].get(package.name) != checksum:
        raise ValueError('Artifact checksum mismatch')
    with tempfile.TemporaryFile() as decoded:
        subprocess.run(['zstd', '-dc', str(package)], stdout=decoded, check=True)
        decoded.seek(0)
        with tarfile.open(fileobj=decoded, mode='r:') as archive:
            info = archive.extractfile('.PKGINFO').read().decode()
            fields = dict(line.split(' = ', 1) for line in info.splitlines() if ' = ' in line)
            if fields.get('pkgname') != 'vibebeam' or fields.get('arch') != 'x86_64':
                raise ValueError('Wrong package identity')
            identity = json.load(archive.extractfile('usr/share/vibebeam/beta-build.json'))
            if identity['source_commit'] != sha:
                raise ValueError('Packaged source identity mismatch')
            updater = archive.getmember('usr/bin/vibebeam-update')
            if not updater.isfile() or not updater.mode & 0o111:
                raise ValueError('Missing executable updater')
    source = root / 'matching-host-source.tar.gz'
    with tarfile.open(source, 'r:gz') as archive:
        if not archive.getmember('packaging/linux/vibebeam-update.py').isfile():
            raise ValueError('Missing matching updater source')
    manifest = dict(schema_version=1, repository='nyteghost/vibebeam',
                    channel='beta', branch='beta/host-download-20261028', architecture='x86_64',
                    source_commit=sha, workflow_run_id=int(run), run_attempt=int(attempt),
                    package=package.name, package_version=fields['pkgver'],
                    size=package.stat().st_size, sha256=checksum)
    (root / 'vibebeam-beta.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (root / 'host-update-identity.txt').write_text('\n<!-- host-update: ' + json.dumps(manifest, separators=(',', ':')) + ' -->\n')
    # A bootstrap copy is convenient for older packages without the updater command.
    with tarfile.open(source, 'r:gz') as archive:
        (root / 'vibebeam-update.py').write_bytes(archive.extractfile('packaging/linux/vibebeam-update.py').read())
    assets = [package, source, root / 'vibepollo.pkg.tar.gz', root / 'linux-provenance.json',
              root / 'vibebeam-beta.json', root / 'vibebeam-update.py']
    (root / 'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in assets))
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('sha')
    parser.add_argument('run')
    parser.add_argument('attempt')
    args = parser.parse_args()
    print(json.dumps(prepare(args.directory, args.sha, args.run, args.attempt)))
