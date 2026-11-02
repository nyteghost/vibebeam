import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('publication', ROOT / '.github/scripts/prepare_host_beta.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


def member(archive, name, data, mode=0o644):
    entry = tarfile.TarInfo(name)
    entry.size = len(data)
    entry.mode = mode
    archive.addfile(entry, io.BytesIO(data))


def artifact(root, source='a' * 40, name='vibebeam', executable=True):
    package = root / 'vibebeam-2.0.1beta.r20261102000000-7-x86_64.pkg.tar.zst'
    with tempfile.TemporaryFile() as source_tar:
        with tarfile.open(fileobj=source_tar, mode='w') as archive:
            member(archive, '.PKGINFO', f'pkgname = {name}\npkgver = 2.0.1beta.r20261102000000-7\narch = x86_64\n'.encode())
            member(archive, 'usr/share/vibebeam/beta-build.json', json.dumps(dict(source_commit=source)).encode())
            member(archive, 'usr/bin/vibebeam-update', b'#!/usr/bin/env python3\n', 0o755 if executable else 0o644)
        source_tar.seek(0)
        with package.open('wb') as target:
            subprocess.run(['zstd', '-c'], stdin=source_tar, stdout=target, check=True)
    (root / 'linux-provenance.json').write_text(json.dumps(dict(source_commit='a' * 40, workflow_run_id='12345', assets={package.name: hashlib.sha256(package.read_bytes()).hexdigest()})))
    with tarfile.open(root / 'matching-host-source.tar.gz', 'w:gz') as archive:
        member(archive, 'packaging/linux/vibebeam-update.py', b'# matching source\n')
    (root / 'vibepollo.pkg.tar.gz').write_bytes(b'build recipe fixture')
    return package


class PublicationTests(unittest.TestCase):
    def test_manifest_and_bootstrap_match_tested_package(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            package = artifact(root)
            result = p.prepare(root, 'a' * 40, '12345', '1')
            self.assertEqual(result['size'], package.stat().st_size)
            self.assertEqual(result['sha256'], hashlib.sha256(package.read_bytes()).hexdigest())
            self.assertIn('vibebeam-update.py', (root / 'SHA256SUMS').read_text())
            self.assertEqual((root / 'vibebeam-update.py').read_text(), '# matching source\n')

    def test_wrong_source_package_and_nonexecutable_updater_fail(self):
        for kwargs in [dict(source='b' * 40), dict(name='other'), dict(executable=False)]:
            with self.subTest(kwargs=kwargs), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                artifact(root, **kwargs)
                with self.assertRaises(ValueError): p.prepare(root, 'a' * 40, '12345', '1')

    def test_corrupt_artifact_and_wrong_run_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            package = artifact(root)
            with self.assertRaises(ValueError): p.prepare(root, 'a' * 40, '12346', '1')
            package.write_bytes(b'corrupted')
            with self.assertRaises(ValueError): p.prepare(root, 'a' * 40, '12345', '1')
