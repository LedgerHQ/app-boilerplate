from git import Repo
import os
import subprocess
from pathlib import Path

DEVICES_CONF = {
    "nanos+": {
        "name": "nanos2",
        "sdk": "NANOSP_SDK",
        "bin_path": "build/nanos2/bin/",
    },
    "nanox": {
        "name": "nanox",
        "sdk": "NANOX_SDK",
        "bin_path": "build/nanox/bin/",
    },
    "stax": {
        "name": "stax",
        "sdk": "STAX_SDK",
        "bin_path": "build/stax/bin/",
    },
    "flex": {
        "name": "flex",
        "sdk": "FLEX_SDK",
        "bin_path": "build/flex/bin/",
    },
}


APP_EXCHANGE_URL = "git@github.com:LedgerHQ/app-exchange.git"
APP_EXCHANGE_DIR = "./app-exchange"

APP_ETHEREUM_URL = "git@github.com:LedgerHQ/app-ethereum.git"
APP_ETHEREUM_DIR = "./app-ethereum"

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
    

# Build Exchange app for all devices
# test if the directory already exists
if not os.path.exists("main_app"):
    if not os.path.exists(APP_EXCHANGE_DIR):
        Repo.clone_from(APP_EXCHANGE_URL, APP_EXCHANGE_DIR)
    os.makedirs("main_app/exchange", exist_ok=True)
    flags = ' TESTING=1 TEST_PUBLIC_KEY=1 DEBUG=1'
    run_cmd("make clean", Path(APP_EXCHANGE_DIR))
    for e in DEVICES_CONF.values():
        run_cmd("make -j BOLOS_SDK=$"+e["sdk"]+flags, Path(APP_EXCHANGE_DIR))
        os.makedirs("main_app/exchange/"+e["bin_path"], exist_ok=True)
        run_cmd("cp "+e["bin_path"]+"app.elf ../main_app/exchange/"+e["bin_path"], Path(APP_EXCHANGE_DIR))
    run_cmd("rm -rf "+APP_EXCHANGE_DIR)

# Build Ethereum app for all devices
# test if the directory already exists
if not os.path.exists("lib_binaries"):
    if not os.path.exists(APP_ETHEREUM_DIR):
        Repo.clone_from(APP_ETHEREUM_URL, APP_ETHEREUM_DIR, multi_options=["--recurse-submodules"])
    os.makedirs("lib_binaries", exist_ok=True)
    flags = ' COIN=ethereum CHAIN=ethereum CAL_TEST_KEY=1 DOMAIN_NAME_TEST_KEY=1 SET_PLUGIN_TEST_KEY=1 NFT_TEST_KEY=1 TRUSTED_NAME_TEST_KEY=1'
    run_cmd("make clean", Path(APP_ETHEREUM_DIR))
    for e in DEVICES_CONF.values():
        run_cmd("make -j BOLOS_SDK=$"+e["sdk"]+flags, Path(APP_ETHEREUM_DIR))
        run_cmd("cp "+e["bin_path"]+"app.elf ../lib_binaries/ethereum_"+e["name"]+".elf", Path(APP_ETHEREUM_DIR))
    run_cmd("rm -rf "+APP_ETHEREUM_DIR)
