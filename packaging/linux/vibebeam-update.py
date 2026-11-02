#!/usr/bin/env python3
"""Manual Arch/CachyOS beta updates. Network and downloads run without root."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

REPO = 'nyteghost/vibebeam'
BRANCH = 'beta/host-download-20261028'
API = f'https://api.github.com/repos/{REPO}'
MAX_PACKAGE = 512 * 1024 * 1024
ALLOWED_HOSTS = {'api.github.com', 'github.com', 'release-assets.githubusercontent.com',
                 'objects.githubusercontent.com'}


def checked_url(url):
    parsed = urllib.parse.urlsplit(url)
    if (parsed.scheme != 'https' or parsed.hostname not in ALLOWED_HOSTS
            or parsed.username or parsed.password or parsed.fragment
            or parsed.port not in (None, 443)):
        raise ValueError('Untrusted download URL')
    return url


class Redirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        checked_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(url, target, limit):
    req = urllib.request.Request(checked_url(url), headers={'User-Agent': 'Vibebeam-beta-updater'})
    with urllib.request.build_opener(Redirects()).open(req, timeout=30) as response:
        total = 0
        while chunk := response.read(1024 * 1024):
            total += len(chunk)
            if total > limit:
                raise ValueError('Download exceeds size limit')
            target.write(chunk)
    return total


def get_json(url):
    import io
    output = io.BytesIO()
    download(url, output, 2 * 1024 * 1024)
    return json.loads(output.getvalue())


def release_order(release):
    match = re.fullmatch(r'host-beta-(\d+)-(\d+)', str(release.get('tag_name', '')))
    if not match or release.get('draft') or release.get('prerelease') is not True:
        return None
    return tuple(map(int, match.groups()))


def asset_url(tag, name):
    if not re.fullmatch(r'[A-Za-z0-9_.+-]+', name):
        raise ValueError('Unsafe asset name')
    return f'https://github.com/{REPO}/releases/download/{tag}/{name}'


def validate_manifest(release, manifest):
    order = release_order(release)
    if (not order or manifest.get('schema_version') != 1 or manifest.get('repository') != REPO
            or manifest.get('branch') != BRANCH or manifest.get('channel') != 'beta'
            or manifest.get('architecture') != 'x86_64'
            or str(manifest.get('workflow_run_id')) != str(order[0])
            or str(manifest.get('run_attempt')) != str(order[1])):
        raise ValueError('Wrong beta manifest identity')
    sha = manifest.get('source_commit', '')
    if not re.fullmatch(r'[0-9a-f]{40}', sha) or release.get('target_commitish') != sha:
        raise ValueError('Release/source commit mismatch')
    package = manifest.get('package', '')
    if not re.fullmatch(r'vibebeam-[A-Za-z0-9_.+-]+-x86_64\.pkg\.tar\.zst', package):
        raise ValueError('Unexpected package name')
    size = manifest.get('size')
    if type(size) is not int or not 0 < size <= MAX_PACKAGE:
        raise ValueError('Invalid package size')
    if not re.fullmatch(r'[0-9a-f]{64}', manifest.get('sha256', '')):
        raise ValueError('Invalid checksum')
    expected = asset_url(release['tag_name'], package)
    if not any(a.get('name') == package and a.get('browser_download_url') == expected
               and a.get('size') == size for a in release.get('assets', [])):
        raise ValueError('Package not present in release')
    return expected


def package_identity(path):
    result = subprocess.run(['pacman', '-Qp', str(path)], check=True,
                            capture_output=True, text=True).stdout.split()
    if len(result) != 2 or result[0] != 'vibebeam':
        raise ValueError('Package is not Vibebeam')
    return tuple(result)


def find_rollback(installed, supplied):
    candidates = [Path(supplied)] if supplied else [
        *sorted(Path('/var/cache/pacman/pkg').glob('vibebeam-*.pkg.tar.zst')),
        *sorted((Path.home() / '.cache/vibebeam-updates').glob('vibebeam-*.pkg.tar.zst')),
    ]
    for candidate in candidates:
        if not candidate.is_file() or candidate.is_symlink():
            continue
        try:
            if package_identity(candidate) == installed:
                return candidate.resolve()
        except (subprocess.CalledProcessError, ValueError):
            continue
    raise ValueError('No rollback package matches the installed version. Re-run with '
                     '--rollback /absolute/path/to/your/verified-old.pkg.tar.zst')


# Only this fixed program runs under sudo. It never executes downloaded code.
# Root-owned, private snapshots stay separate from user-writable download paths.
BACKUP_PROGRAM = r'''
import os, pathlib, shutil, stat, subprocess, sys, tempfile
base = pathlib.Path('/var/lib/vibepollo-package-backups')
if not base.exists():
    base.mkdir(mode=0o700)
s = base.lstat()
assert stat.S_ISDIR(s.st_mode) and s.st_uid == 0 and stat.S_IMODE(s.st_mode) == 0o700, 'Unsafe backup directory'
folder = pathlib.Path(tempfile.mkdtemp(prefix='update-', dir=base))
os.chmod(folder, 0o700)
rollback = pathlib.Path(sys.argv[1])
assert rollback.is_file() and not rollback.is_symlink()
shutil.copyfile(rollback, folder / 'rollback.pkg.tar.zst')
os.chmod(folder / 'rollback.pkg.tar.zst', 0o600)
identity = subprocess.check_output(['pacman', '-Qp', str(folder / 'rollback.pkg.tar.zst')], text=True).strip()
assert identity == sys.argv[2], 'Rollback changed during copy'
paths = []
for name in ['etc/vibepollo', 'var/lib/vibepollo']:
    path = pathlib.Path('/') / name
    if path.is_symlink():
        raise RuntimeError('Refusing symlinked machine state')
    if path.exists():
        paths.append(name)
assert paths, 'No machine state found; stopping before installation'
subprocess.run(['tar', '--acls', '--xattrs', '-C', '/', '-czf', str(folder / 'state.tar.gz'), '--', *paths], check=True)
os.chmod(folder / 'state.tar.gz', 0o600)
subprocess.run(['tar', '-tzf', str(folder / 'state.tar.gz')], check=True, stdout=subprocess.DEVNULL)
(folder / 'installed.txt').write_text(sys.argv[2] + '\n')
os.chmod(folder / 'installed.txt', 0o600)
print(folder)
'''


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Only check; no sudo or installation')
    parser.add_argument('--rollback', help='Existing package matching the installed version')
    args = parser.parse_args(argv)
    if os.geteuid() == 0:
        raise ValueError('Run as your ordinary desktop user, without sudo')
    runtime = Path(f'/run/user/{os.getuid()}')
    if not runtime.is_dir() or runtime.is_symlink() or runtime.stat().st_uid != os.getuid():
        raise ValueError('A private desktop runtime directory is required')
    lock = os.open(runtime / 'vibebeam-update.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    # Keep this descriptor alive until process exit; the OS releases the lock.
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        os.close(lock)
        raise ValueError('Another Vibebeam updater is already running')
    if platform.machine() != 'x86_64' or not shutil.which('pacman'):
        raise ValueError('This updater currently supports Arch/CachyOS x86_64 only')
    releases = []
    for page in range(1, 4):
        items = get_json(f'{API}/releases?per_page=100&page={page}')
        if not isinstance(items, list):
            raise ValueError('Invalid GitHub release response')
        releases.extend(r for r in items if release_order(r))
        if len(items) < 100:
            break
    if not releases:
        raise ValueError('No published, tested host beta is available yet')
    release = max(releases, key=release_order)
    manifest = get_json(asset_url(release['tag_name'], 'vibebeam-beta.json'))
    url = validate_manifest(release, manifest)
    print(f"Available beta: {manifest['source_commit'][:8]} ({release['tag_name']})", flush=True)
    identity_file = Path('/usr/share/vibebeam/beta-build.json')
    if identity_file.is_file():
        installed_build = json.loads(identity_file.read_text())
        previous_run = str(installed_build.get('workflow_run_id', ''))
        if previous_run.isdigit() and int(previous_run) > manifest['workflow_run_id']:
            raise ValueError('Refusing a beta older than the installed build')
        if installed_build.get('source_commit') == manifest['source_commit']:
            print('This beta commit is already installed. No changes made.')
            return 0
    if args.check:
        return 0
    installed = tuple(subprocess.check_output(['pacman', '-Q', 'vibebeam'], text=True).split())
    if len(installed) != 2 or installed[0] != 'vibebeam':
        raise ValueError('Existing Vibebeam installation required')
    rollback = find_rollback(installed, args.rollback)
    if not sys.stdin.isatty():
        raise ValueError('Run interactively in a terminal')
    print('Close all streaming clients first. Authenticated live session detection is not configured.')
    print('Installation runs package hooks and restarts the host. A private state/rollback snapshot will be saved.')
    if input('Type UPDATE after disconnecting clients: ').strip() != 'UPDATE':
        print('Cancelled. No installation performed.')
        return 0
    cache = Path.home() / '.cache/vibebeam-updates'
    cache.mkdir(mode=0o700, parents=True, exist_ok=True)
    if cache.is_symlink() or cache.stat().st_uid != os.getuid():
        raise ValueError('Unsafe update cache directory')
    cache.chmod(0o700)
    with tempfile.TemporaryDirectory(prefix='download-', dir=cache) as scratch:
        package = Path(scratch) / manifest['package']
        print('Downloading verified beta package...', flush=True)
        with package.open('xb') as target:
            size = download(url, target, manifest['size'])
        if size != manifest['size'] or hashlib.sha256(package.read_bytes()).hexdigest() != manifest['sha256']:
            raise ValueError('Package size/checksum mismatch')
        if package_identity(package)[1] != manifest['package_version']:
            raise ValueError('Package metadata/version mismatch')
        # Retain verified incoming packages for the next update's rollback lookup.
        retained = cache / package.name
        os.replace(package, retained)
        package = retained
        subprocess.run(['sudo', '/usr/bin/python3', '-c', BACKUP_PROGRAM,
                        str(rollback), ' '.join(installed)], check=True)
        # Keep pacman's normal confirmation; never use --noconfirm or overwrite flags.
        subprocess.run(['sudo', 'pacman', '-U', str(package)], check=True)
    subprocess.run(['sudo', 'systemctl', 'restart', 'vibepollo.service'], check=True)
    subprocess.run(['systemctl', 'is-active', '--quiet', 'vibepollo.service'], check=True)
    tray = Path('/usr/bin/vibepollo-tray')
    if tray.is_file() and (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
        subprocess.Popen([str(tray)], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, start_new_session=True)
    print('Beta package installed; service is active. Verify a real streaming session next.')
    print('Dashboard: https://127.0.0.1:47990/v2/')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'Update stopped: {error}', file=sys.stderr)
        sys.exit(1)
    except (KeyboardInterrupt, EOFError):
        print('\nUpdate interrupted. Check package/service status before retrying.', file=sys.stderr)
        sys.exit(1)
