import os
import subprocess
from git import Repo
from pathlib import Path

APP_EXCHANGE_URL = "git@github.com:LedgerHQ/app-exchange.git"
APP_EXCHANGE_DIR = ".test_dependencies/main/app-exchange/"
APP_EXCHANGE_CLONE_DIR = ".test_dependencies/app-exchange/"

APP_ETHEREUM_URL = "git@github.com:LedgerHQ/app-ethereum.git"
APP_ETHEREUM_DIR = ".test_dependencies/libraries/app-ethereum/"
APP_ETHEREUM_CLONE_DIR = ".test_dependencies/app-ethereum/"

DEVICES_CONF = {
    "nanos+": {
        "sdk": "NANOSP_SDK",
        "bin_path": "build/nanos2/bin/",
    },
    "nanox": {
        "sdk": "NANOX_SDK",
        "bin_path": "build/nanox/bin/",
    },
    "stax": {
        "sdk": "STAX_SDK",
        "bin_path": "build/stax/bin/",
    },
    "flex": {
        "sdk": "FLEX_SDK",
        "bin_path": "build/flex/bin/",
    },
}

def run_cmd(cmd: str,
            cwd: Path = Path('.'),
            print_output: bool = False,
            no_throw: bool = False) -> str:

    print(f"[run_cmd] Running: {cmd} from {cwd}")

    ret = subprocess.run(cmd,
                         shell=True,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT,
                         universal_newlines=True,
                         cwd=cwd)
    if no_throw is False and ret.returncode:
        print(f"[run_cmd] Error {ret.returncode} raised while running cmd: {cmd}")
        print("[run_cmd] Output was:")
        print(ret.stdout)
        raise ValueError()

    if print_output:
        print(f"[run_cmd] Output:\n{ret.stdout}")

    return ret.stdout.strip()

def clone_or_pull(repo_url: str, clone_dir: str):
    git_dir = os.path.join(clone_dir, ".git")
    if not os.path.exists(git_dir):
        print(f"Cloning into {clone_dir}")
        run_cmd(f"rm -rf {clone_dir}")
        Repo.clone_from(repo_url, clone_dir, recursive=True)
    else:
        print(f"Pulling latest changes in {clone_dir}")
        repo = Repo(clone_dir)
        origin = repo.remotes.origin
        origin.fetch()
        repo.git.reset('--hard', 'origin/master')

        # Update submodules
        print(f"Updating submodules in {clone_dir}")
        run_cmd("git submodule sync", cwd=Path(clone_dir))
        run_cmd("git submodule update --init --recursive", cwd=Path(clone_dir))

def build_app(clone_dir: str, flags: str):
    for d in DEVICES_CONF.values():
        sdk = d["sdk"]
        cmd = f"make -j BOLOS_SDK=${sdk} {flags}"
        run_cmd(cmd, cwd=Path(clone_dir))

def copy_build_output(clone_dir: str, dest_dir: str):
    run_cmd(f"mkdir -p {dest_dir}")
    run_cmd(f"cp -rT {clone_dir}/build {dest_dir}/build")


# ==== Build app-exchange ====
clone_or_pull(APP_EXCHANGE_URL, APP_EXCHANGE_CLONE_DIR)
build_app(APP_EXCHANGE_CLONE_DIR, flags="TESTING=1 TEST_PUBLIC_KEY=1 TRUSTED_NAME_TEST_KEY=1 DEBUG=1")
copy_build_output(APP_EXCHANGE_CLONE_DIR, APP_EXCHANGE_DIR)

# ==== Build app-ethereum ====
clone_or_pull(APP_ETHEREUM_URL, APP_ETHEREUM_CLONE_DIR)
build_app(APP_ETHEREUM_CLONE_DIR, flags="COIN=ethereum CHAIN=ethereum CAL_TEST_KEY=1 DOMAIN_NAME_TEST_KEY=1 SET_PLUGIN_TEST_KEY=1 NFT_TEST_KEY=1 TRUSTED_NAME_TEST_KEY=1")
copy_build_output(APP_ETHEREUM_CLONE_DIR, APP_ETHEREUM_DIR)
