# Run this python script inside the Ledger docker image

# This script will build the Ethereum and Exchange application that are needed for the Swap test setup
# Run the helper script helper_tool_clone_dependencies.py first to pull them in the correct location

from _helper_tool import build_and_copy_exchange, build_and_copy_ethereum

build_and_copy_exchange()
build_and_copy_ethereum()
