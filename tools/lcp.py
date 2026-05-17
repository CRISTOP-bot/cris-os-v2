#!/usr/bin/env python3
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

HOME = Path.home()
SCRIPT_DIR = Path(__file__).resolve().parent
LCP_DIR = HOME / '.lcp'
REPOS_FILE = LCP_DIR / 'repos.json'
INSTALLED_FILE = LCP_DIR / 'installed.json'
CACHE_DIR = LCP_DIR / 'cache'

EXIT_SUCCESS = 0
EXIT_ERROR = 1
EXIT_INVALID_COMMAND = 2
EXIT_NOT_FOUND = 3
EXIT_DEP_MISSING = 4
EXIT_INSTALL_FAIL = 5
EXIT_PERMISSIONS = 6
EXIT_REPO_DOWN = 7
EXIT_ALREADY_INSTALLED = 8
EXIT_CORRUPT = 9

DEFAULT_REPOS = [
    {
        'name': 'main',
        'url': f'file:{SCRIPT_DIR / "lcp_main_repo.json"}',
        'enabled': True,
    }
]

GENERAL_HELP = '''Usage: lcp <command> [args] [options]

Commands:
  help [command]          Show general help or help for a command
  search <term>           Search packages in enabled repos
  info <package>          Show package information
  install <package>       Install a package
  remove <package>        Remove an installed package
  update                  Refresh repository metadata
  upgrade [package]       Upgrade installed packages
  list [options]          List packages
  clean                   Remove cached repository data
  files <package>         Show files installed by a package
  depends <package>       Show package dependencies
  verify <package>        Verify installed package files
  repo <action> [...]     Manage repositories

Repo commands:
  repo list
  repo add <name> <url>
  repo remove <name>
  repo enable <name>
  repo disable <name>

Options for install:
  --force                 Reinstall even if already installed
  --no-deps               Do not install dependencies
  --root <path>           Use alternative root directory

Options for list:
  --installed
  --available
  --upgradable
'''

COMMAND_HELP = {
    'search': 'lcp search <term>\n  Search available packages in enabled repositories.',
    'info': 'lcp info <package>\n  Show package metadata for a package.',
    'install': 'lcp install <package> [--force] [--no-deps] [--root <path>]\n  Install a package and its dependencies.',
    'remove': 'lcp remove <package> [--purge]\n  Remove an installed package.',
    'update': 'lcp update\n  Download the latest repository metadata.',
    'upgrade': 'lcp upgrade [package]\n  Upgrade installed packages to the newest available versions.',
    'list': 'lcp list [--installed|--available|--upgradable]\n  List packages by state.',
    'clean': 'lcp clean\n  Remove cached repository data.',
    'files': 'lcp files <package>\n  Show files installed by a package.',
    'depends': 'lcp depends <package>\n  Show package dependencies.',
    'verify': 'lcp verify <package>\n  Check package files on disk.',
    'repo': 'lcp repo <list|add|remove|enable|disable> ...\n  Manage repository sources.',
}

def ensure_storage():
    LCP_DIR.mkdir(parents=True, exist_ok=True)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    if not REPOS_FILE.exists():
        save_json(REPOS_FILE, {'repos': DEFAULT_REPOS})
    if not INSTALLED_FILE.exists():
        save_json(INSTALLED_FILE, {'packages': {}, 'held': []})


def load_json(path, default):
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        return default
    except json.JSONDecodeError:
        return default


def save_json(path, data):
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)


def read_repos():
    data = load_json(REPOS_FILE, {'repos': DEFAULT_REPOS})
    repos = data.get('repos', DEFAULT_REPOS)
    return repos


def write_repos(repos):
    save_json(REPOS_FILE, {'repos': repos})


def read_installed():
    return load_json(INSTALLED_FILE, {'packages': {}, 'held': []})


def write_installed(data):
    save_json(INSTALLED_FILE, data)


def parse_version(version):
    parts = re.split(r'[._-]', version)
    result = []
    for part in parts:
        if part.isdigit():
            result.append(int(part))
        else:
            result.append(part)
    return tuple(result)


def compare_versions(a, b):
    return (parse_version(a) > parse_version(b)) - (parse_version(a) < parse_version(b))


def normalize_url(url):
    if url.startswith('file://'):
        return url
    if url.startswith('file:'):
        return 'file://' + url[5:]
    return url


def resolve_file_url(url):
    path = url
    if path.startswith('file://'):
        path = path[7:]
    elif path.startswith('file:'):
        path = path[5:]
    if not os.path.isabs(path):
        path = str((SCRIPT_DIR / path).resolve())
    return path


def fetch_repo_index(repo):
    url = normalize_url(repo.get('url', ''))
    if url.startswith('file://'):
        filename = resolve_file_url(url)
        try:
            with open(filename, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return data
        except FileNotFoundError:
            raise FileNotFoundError(f'Repo file not found: {filename}')
        except json.JSONDecodeError as exc:
            raise ValueError(f'Invalid JSON in repo index: {exc}')
    try:
        with urllib.request.urlopen(url, timeout=10) as response:
            if response.status != 200:
                raise urllib.error.URLError(f'Status {response.status}')
            payload = response.read().decode('utf-8')
            return json.loads(payload)
    except (urllib.error.URLError, ValueError) as exc:
        raise ConnectionError(str(exc))


def cache_name(repo):
    key = re.sub(r'[^a-zA-Z0-9_-]', '_', repo.get('name', 'repo'))
    return CACHE_DIR / f'{key}.json'


def update_repos():
    repos = read_repos()
    updated = []
    failed = []
    for repo in repos:
        if not repo.get('enabled', True):
            continue
        try:
            index = fetch_repo_index(repo)
            if not isinstance(index, dict) or 'packages' not in index:
                raise ValueError('Repo index missing packages field')
            for pkg in index['packages']:
                pkg['repo'] = repo['name']
            cache_path = cache_name(repo)
            save_json(cache_path, {'packages': index['packages'], 'updated': __import__('time').time()})
            updated.append(repo['name'])
        except FileNotFoundError as exc:
            failed.append((repo['name'], f'File not found: {exc}'))
        except ConnectionError as exc:
            failed.append((repo['name'], str(exc)))
        except Exception as exc:
            failed.append((repo['name'], str(exc)))
    if updated:
        print(f'Updated repos: {", ".join(updated)}')
    if failed:
        for name, reason in failed:
            print(f'Failed to update {name}: {reason}')
    return len(failed) == 0


def load_cached_packages():
    repos = read_repos()
    packages = {}
    for repo in repos:
        cache_path = cache_name(repo)
        data = load_json(cache_path, {})
        for pkg in data.get('packages', []):
            name = pkg.get('name')
            if not name:
                continue
            if name not in packages or compare_versions(pkg.get('version', '0.0.0'), packages[name].get('version', '0.0.0')) > 0:
                packages[name] = pkg
    return packages


def find_package(name):
    packages = load_cached_packages()
    return packages.get(name)


def search_packages(term):
    term = term.lower()
    matches = []
    for pkg in load_cached_packages().values():
        if term in pkg.get('name', '').lower() or term in pkg.get('description', '').lower():
            matches.append(pkg['name'])
    return sorted(matches)


def print_package_info(pkg):
    console_print(f'name: {pkg.get("name", "")}')
    console_print(f'version: {pkg.get("version", "")}')
    console_print(f'description: {pkg.get("description", "")}')
    console_print(f'size: {pkg.get("size", 0)} bytes')
    console_print(f'dependencies: {", ".join(pkg.get("dependencies", [])) or "none"}')
    console_print(f'repository: {pkg.get("repo", "")}')
    console_print(f'author: {pkg.get("maintainer", "")}')
    console_print(f'license: {pkg.get("license", "")}')


def console_print(message):
    sys.stdout.write(message + '\n')


def ensure_root(root):
    root_path = Path(root).resolve()
    root_path.mkdir(parents=True, exist_ok=True)
    return root_path


def install_package(pkg, root_path, installed, force=False):
    name = pkg['name']
    if name in installed['packages'] and not force:
        return EXIT_ALREADY_INSTALLED
    if name in installed['packages'] and force:
        console_print(f'Reinstalling {name}...')
    else:
        console_print(f'Installing {name}...')
    file_paths = []
    try:
        for rel in pkg.get('files', []):
            path = root_path / rel
            if rel.endswith('/'):
                path.mkdir(parents=True, exist_ok=True)
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            content = f'Package {name} ({pkg.get("version", "")}) installed by lcp.\n'
            path.write_text(content, encoding='utf-8')
            file_paths.append(str(path.relative_to(root_path)))
    except PermissionError:
        return EXIT_PERMISSIONS
    except OSError:
        return EXIT_INSTALL_FAIL
    installed['packages'][name] = {
        'version': pkg.get('version', ''),
        'files': file_paths,
        'dependencies': pkg.get('dependencies', []),
        'repo': pkg.get('repo', ''),
        'description': pkg.get('description', ''),
        'license': pkg.get('license', ''),
        'arch': pkg.get('arch', ''),
        'maintainer': pkg.get('maintainer', ''),
        'size': pkg.get('size', 0),
        'state': 'installed',
    }
    return EXIT_SUCCESS


def resolve_dependencies(pkg, installed, no_deps):
    if no_deps:
        return [pkg], []
    packages = load_cached_packages()
    order = []
    seen = set()
    missing = []

    def visit(item):
        if item in seen:
            return
        seen.add(item)
        if item in installed['packages']:
            return
        dep_pkg = packages.get(item)
        if not dep_pkg:
            missing.append(item)
            return
        for dep in dep_pkg.get('dependencies', []):
            visit(dep)
        order.append(dep_pkg)

    for dep_name in pkg.get('dependencies', []):
        visit(dep_name)
    order.append(pkg)
    return order, missing


def remove_package(name, root_path, installed, purge=False):
    pkg = installed['packages'].get(name)
    if not pkg:
        return EXIT_NOT_FOUND
    dependents = [n for n, data in installed['packages'].items() if name in data.get('dependencies', [])]
    if dependents:
        console_print(f'Cannot remove {name}: other packages depend on it: {", ".join(dependents)}')
        return EXIT_INSTALL_FAIL
    for rel in pkg.get('files', []):
        path = root_path / rel
        try:
            if path.exists():
                path.unlink()
        except PermissionError:
            return EXIT_PERMISSIONS
        except OSError:
            pass
    if purge:
        config_path = root_path / 'etc' / f'{name}.conf'
        if config_path.exists():
            try:
                config_path.unlink()
            except OSError:
                pass
    installed['packages'].pop(name, None)
    return EXIT_SUCCESS


def list_packages(args, installed):
    available = load_cached_packages()
    if args.get('available'):
        for name in sorted(available):
            print(name)
        return EXIT_SUCCESS
    if args.get('upgradable'):
        upgrades = []
        for name, data in installed['packages'].items():
            remote = available.get(name)
            if remote and compare_versions(remote.get('version', '0.0.0'), data.get('version', '0.0.0')) > 0:
                upgrades.append(name)
        for name in sorted(upgrades):
            print(name)
        return EXIT_SUCCESS
    if args.get('installed') or not any(args.values()):
        for name in sorted(installed['packages']):
            print(name)
        return EXIT_SUCCESS
    return EXIT_SUCCESS


def upgrade_packages(target, root_path, installed):
    available = load_cached_packages()
    candidates = []
    if target:
        current = installed['packages'].get(target)
        if not current:
            print(f'Package not installed: {target}')
            return EXIT_NOT_FOUND
        remote = available.get(target)
        if not remote:
            print(f'Package not found in repos: {target}')
            return EXIT_NOT_FOUND
        if compare_versions(remote.get('version', '0.0.0'), current.get('version', '0.0.0')) <= 0:
            print(f'{target} is already up to date.')
            return EXIT_SUCCESS
        candidates = [remote]
    else:
        for name, data in installed['packages'].items():
            remote = available.get(name)
            if remote and compare_versions(remote.get('version', '0.0.0'), data.get('version', '0.0.0')) > 0:
                candidates.append(remote)
    if not candidates:
        print('No upgrades available.')
        return EXIT_SUCCESS
    for pkg in candidates:
        code = install_package(pkg, root_path, installed, force=True)
        if code != EXIT_SUCCESS:
            return code
        print(f'Upgraded {pkg["name"]} to {pkg["version"]}.')
    return EXIT_SUCCESS


def verify_package(name, root_path, installed):
    pkg = installed['packages'].get(name)
    if not pkg:
        return EXIT_NOT_FOUND
    missing = []
    for rel in pkg.get('files', []):
        path = root_path / rel
        if not path.exists():
            missing.append(rel)
    if missing:
        print('Missing files:')
        for rel in missing:
            print(f'  {rel}')
        return EXIT_INSTALL_FAIL
    print(f'{name} is installed and all files are present.')
    return EXIT_SUCCESS


def repo_command(args):
    repos = read_repos()
    if not args:
        print(COMMAND_HELP['repo'])
        return EXIT_INVALID_COMMAND
    action = args[0]
    if action == 'list':
        for repo in repos:
            status = 'enabled' if repo.get('enabled', True) else 'disabled'
            print(f"{repo.get('name')}: {repo.get('url')} ({status})")
        return EXIT_SUCCESS
    if action == 'add' and len(args) == 3:
        name, url = args[1], args[2]
        if any(r['name'] == name for r in repos):
            print(f'Repository already exists: {name}')
            return EXIT_ERROR
        repos.append({'name': name, 'url': url, 'enabled': True})
        write_repos(repos)
        print(f'Added repository {name}.')
        return EXIT_SUCCESS
    if action in ('remove', 'enable', 'disable') and len(args) == 2:
        name = args[1]
        repo = next((r for r in repos if r['name'] == name), None)
        if not repo:
            print(f'Repository not found: {name}')
            return EXIT_NOT_FOUND
        if action == 'remove':
            repos = [r for r in repos if r['name'] != name]
            write_repos(repos)
            print(f'Removed repository {name}.')
            return EXIT_SUCCESS
        if action == 'enable':
            repo['enabled'] = True
            write_repos(repos)
            print(f'Enabled repository {name}.')
            return EXIT_SUCCESS
        if action == 'disable':
            repo['enabled'] = False
            write_repos(repos)
            print(f'Disabled repository {name}.')
            return EXIT_SUCCESS
    print(COMMAND_HELP['repo'])
    return EXIT_INVALID_COMMAND


def help_command(args):
    if not args:
        print(GENERAL_HELP)
        return EXIT_SUCCESS
    target = args[0]
    print(COMMAND_HELP.get(target, f'No detailed help for {target}.'))
    return EXIT_SUCCESS


def show_files(name, root_path, installed):
    pkg = installed['packages'].get(name)
    if not pkg:
        return EXIT_NOT_FOUND
    for rel in pkg.get('files', []):
        print(rel)
    return EXIT_SUCCESS


def show_dependencies(name, installed):
    pkg = installed['packages'].get(name)
    if pkg:
        deps = pkg.get('dependencies', [])
    else:
        pkg = find_package(name)
        if not pkg:
            return EXIT_NOT_FOUND
        deps = pkg.get('dependencies', [])
    if not deps:
        print('No dependencies.')
        return EXIT_SUCCESS
    for dep in deps:
        print(dep)
    return EXIT_SUCCESS


def main():
    ensure_storage()
    if len(sys.argv) < 2:
        return help_command([])
    cmd = sys.argv[1]
    args = sys.argv[2:]
    installed = read_installed()

    if cmd == 'help':
        return help_command(args)
    if cmd == 'search' and len(args) == 1:
        results = search_packages(args[0])
        if results:
            for name in results:
                print(name)
        return EXIT_SUCCESS
    if cmd == 'info' and len(args) == 1:
        pkg = find_package(args[0])
        if not pkg:
            print(f'Package not found: {args[0]}')
            return EXIT_NOT_FOUND
        print_package_info(pkg)
        return EXIT_SUCCESS
    if cmd == 'install' and args:
        force = False
        no_deps = False
        root = Path.cwd()
        name = None
        i = 0
        while i < len(args):
            if args[i] == '--force':
                force = True
            elif args[i] == '--no-deps':
                no_deps = True
            elif args[i] == '--root' and i + 1 < len(args):
                root = Path(args[i + 1])
                i += 1
            elif not name:
                name = args[i]
            else:
                print(f'Unknown option: {args[i]}')
                return EXIT_INVALID_COMMAND
            i += 1
        if not name:
            print('install requires a package name')
            return EXIT_INVALID_COMMAND
        pkg = find_package(name)
        if not pkg:
            print(f'Package not found: {name}')
            return EXIT_NOT_FOUND
        order, missing = resolve_dependencies(pkg, installed, no_deps)
        if missing:
            print(f'Missing dependencies: {", ".join(missing)}')
            return EXIT_DEP_MISSING
        root_path = ensure_root(root)
        for item in order:
            code = install_package(item, root_path, installed, force=force)
            if code != EXIT_SUCCESS:
                return code
        write_installed(installed)
        return EXIT_SUCCESS
    if cmd == 'remove' and args:
        purge = False
        name = None
        for arg in args:
            if arg == '--purge':
                purge = True
            elif not name:
                name = arg
            else:
                print(f'Unknown option: {arg}')
                return EXIT_INVALID_COMMAND
        if not name:
            print('remove requires a package name')
            return EXIT_INVALID_COMMAND
        root_path = Path.cwd()
        code = remove_package(name, root_path, installed, purge=purge)
        if code == EXIT_SUCCESS:
            write_installed(installed)
        return code
    if cmd == 'update' and not args:
        success = update_repos()
        return EXIT_SUCCESS if success else EXIT_REPO_DOWN
    if cmd == 'upgrade':
        target = args[0] if args else None
        root_path = Path.cwd()
        code = upgrade_packages(target, root_path, installed)
        if code == EXIT_SUCCESS:
            write_installed(installed)
        return code
    if cmd == 'list':
        opts = {'installed': False, 'available': False, 'upgradable': False}
        for arg in args:
            if arg == '--installed':
                opts['installed'] = True
            elif arg == '--available':
                opts['available'] = True
            elif arg == '--upgradable':
                opts['upgradable'] = True
            else:
                print(f'Unknown option: {arg}')
                return EXIT_INVALID_COMMAND
        return list_packages(opts, installed)
    if cmd == 'clean' and not args:
        for item in CACHE_DIR.iterdir():
            try:
                item.unlink()
            except OSError:
                pass
        print('Cache cleared.')
        return EXIT_SUCCESS
    if cmd == 'files' and len(args) == 1:
        return show_files(args[0], Path.cwd(), installed)
    if cmd == 'depends' and len(args) == 1:
        return show_dependencies(args[0], installed)
    if cmd == 'verify' and len(args) == 1:
        return verify_package(args[0], Path.cwd(), installed)
    if cmd == 'repo':
        return repo_command(args)
    if cmd in ('init', 'build', 'publish', 'check', 'repair', 'reinstall', 'hold', 'unhold'):
        print(f'{cmd} command is not implemented in this minimal lcp prototype.')
        return EXIT_SUCCESS
    print(f'Unknown command: {cmd}')
    print('Type "lcp help" for a list of supported commands.')
    return EXIT_INVALID_COMMAND


if __name__ == '__main__':
    code = main()
    sys.exit(code)
