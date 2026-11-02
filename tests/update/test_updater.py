import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('updater', ROOT / 'packaging/linux/vibebeam-update.py')
u = importlib.util.module_from_spec(spec)
spec.loader.exec_module(u)


def fixture():
    tag = 'host-beta-12345-1'
    package = 'vibebeam-2.0.1beta.r20261102120000.g' + 'a' * 40 + '-7-x86_64.pkg.tar.zst'
    manifest = dict(schema_version=1, repository=u.REPO, branch=u.BRANCH, channel='beta',
                    architecture='x86_64', source_commit='a' * 40, workflow_run_id=12345,
                    run_attempt=1, package=package, package_version='2.0.1-7', size=20,
                    sha256='b' * 64)
    release = dict(tag_name=tag, prerelease=True, draft=False, target_commitish='a' * 40,
                   assets=[dict(name=package, size=20, browser_download_url=u.asset_url(tag, package))])
    return release, manifest


class UpdaterTests(unittest.TestCase):
    def test_expected_manifest(self):
        release, manifest = fixture()
        self.assertEqual(u.validate_manifest(release, manifest), release['assets'][0]['browser_download_url'])

    def test_bad_manifest_identity(self):
        for key, value in [('repository', 'attacker/repo'), ('branch', 'main'), ('channel', 'stable'),
                           ('architecture', 'aarch64'), ('source_commit', 'unknown'),
                           ('workflow_run_id', 10), ('run_attempt', 2), ('size', True),
                           ('size', u.MAX_PACKAGE + 1), ('sha256', 'invalid'),
                           ('package', '../escape.pkg.tar.zst')]:
            with self.subTest(key=key):
                release, manifest = fixture()
                manifest[key] = value
                with self.assertRaises(ValueError):
                    u.validate_manifest(release, manifest)

    def test_release_substitution(self):
        for change in ['commit', 'asset', 'size', 'draft', 'stable', 'tag']:
            release, manifest = fixture()
            if change == 'commit': release['target_commitish'] = 'c' * 40
            if change == 'asset': release['assets'][0]['browser_download_url'] = 'https://evil.example/a'
            if change == 'size': release['assets'][0]['size'] = 21
            if change == 'draft': release['draft'] = True
            if change == 'stable': release['prerelease'] = False
            if change == 'tag': release['tag_name'] = 'v2.0.1-beta.9'
            with self.subTest(change=change), self.assertRaises(ValueError):
                u.validate_manifest(release, manifest)

    def test_stable_tv_and_drafts_not_selected(self):
        for tag in ['v2.0.1-beta.9', 'tv-beta-11001', 'host-beta-1-x', '../host-beta-1-1']:
            self.assertIsNone(u.release_order(dict(tag_name=tag, prerelease=True)))
        self.assertGreater(u.release_order(dict(tag_name='host-beta-100-2', prerelease=True)),
                           u.release_order(dict(tag_name='host-beta-99-5', prerelease=True)))

    def test_https_boundary(self):
        for url in ['http://github.com/a', 'https://github.com.evil.example/a',
                    'https://user:pass@github.com/a', 'https://github.com:444/a',
                    'https://evil.example/a', 'https://github.com/a#fragment']:
            with self.subTest(url=url), self.assertRaises(ValueError): u.checked_url(url)
        self.assertEqual(u.checked_url('https://release-assets.githubusercontent.com/a'),
                         'https://release-assets.githubusercontent.com/a')

    def test_redirect_revalidates_before_request(self):
        with self.assertRaises(ValueError):
            u.Redirects().redirect_request(None, None, 302, '', {}, 'http://evil.example/a')

    def test_rollback_requires_exact_installed_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            package = Path(folder) / 'rollback.pkg.tar.zst'
            package.write_bytes(b'old')
            with patch.object(u, 'package_identity', return_value=('vibebeam', 'old')):
                self.assertEqual(u.find_rollback(('vibebeam', 'old'), str(package)), package)
                with self.assertRaises(ValueError):
                    u.find_rollback(('vibebeam', 'other'), str(package))
            link = Path(folder) / 'link'
            link.symlink_to(package)
            with self.assertRaises(ValueError): u.find_rollback(('vibebeam', 'old'), str(link))

    def test_wrong_package_cannot_be_installed(self):
        with patch.object(subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'other 1.0\n')):
            with self.assertRaises(ValueError): u.package_identity('/tmp/example')

    def test_bounded_download(self):
        class Response(io.BytesIO):
            pass
        class Opener:
            def open(self, request, timeout): return Response(b'x' * 25)
        with patch.object(u.urllib.request, 'build_opener', return_value=Opener()):
            with self.assertRaises(ValueError):
                u.download('https://github.com/example', io.BytesIO(), 20)
            output = io.BytesIO()
            self.assertEqual(u.download('https://github.com/example', output, 25), 25)
            self.assertEqual(output.getvalue(), b'x' * 25)

    def test_check_and_cancel_never_request_privilege(self):
        release, manifest = fixture()
        for args, answer in [(['--check'], 'UPDATE'), ([], 'cancel')]:
            with self.subTest(args=args), \
                 patch.object(u.os, 'geteuid', return_value=1000), \
                 patch.object(u, 'acquire_lock', return_value=None), \
                 patch.object(u.platform, 'machine', return_value='x86_64'), \
                 patch.object(u.shutil, 'which', return_value='/usr/bin/pacman'), \
                 patch.object(u, 'get_json', side_effect=[[release], manifest]), \
                 patch.object(u, 'find_rollback', return_value=Path('/old-package')), \
                 patch.object(u.subprocess, 'check_output', return_value='vibebeam 2.0.1-7'), \
                 patch.object(u.subprocess, 'run') as run, \
                 patch.object(u.sys.stdin, 'isatty', return_value=True), \
                 patch('builtins.input', return_value=answer), \
                 patch('sys.stdout', new_callable=io.StringIO):
                self.assertEqual(u.main(args), 0)
                run.assert_not_called()

    def test_bad_checksum_stops_before_sudo_and_leaves_no_package(self):
        release, manifest = fixture()
        with tempfile.TemporaryDirectory() as home, \
             patch.object(u.os, 'geteuid', return_value=1000), \
             patch.object(u, 'acquire_lock', return_value=None), \
             patch.object(u.platform, 'machine', return_value='x86_64'), \
             patch.object(u.shutil, 'which', return_value='/usr/bin/pacman'), \
             patch.object(u.Path, 'home', return_value=Path(home)), \
             patch.object(u, 'get_json', side_effect=[[release], manifest]), \
             patch.object(u, 'find_rollback', return_value=Path('/old-package')), \
             patch.object(u.subprocess, 'check_output', return_value='vibebeam 2.0.1-7'), \
             patch.object(u.subprocess, 'run') as run, \
             patch.object(u.sys.stdin, 'isatty', return_value=True), \
             patch('builtins.input', return_value='UPDATE'), \
             patch('sys.stdout', new_callable=io.StringIO):
            def bad_download(url, target, limit):
                target.write(b'x' * 20)
                return 20
            with patch.object(u, 'download', side_effect=bad_download):
                with self.assertRaisesRegex(ValueError, 'checksum'):
                    u.main([])
            run.assert_not_called()
            self.assertEqual(list(Path(home).rglob('*.pkg.tar.zst')), [])

    def test_backup_program_is_syntax_valid_and_private(self):
        compile(u.BACKUP_PROGRAM, '<root-backup>', 'exec')
        self.assertIn('0o700', u.BACKUP_PROGRAM)
        self.assertIn('0o600', u.BACKUP_PROGRAM)
        self.assertNotIn('extractall', u.BACKUP_PROGRAM)
        self.assertNotIn('shell=True', u.BACKUP_PROGRAM)


if __name__ == '__main__': unittest.main()
