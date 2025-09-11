# This script will clone the Ethereum and Exchange application that are needed for the Swap test setup
# Run the helper script helper_tool_build_dependencies.py after to build and dispatch them in the correct location

from _helper_tool import clone_and_pull_exchange, clone_and_pull_ethereum

clone_and_pull_exchange()
clone_and_pull_ethereum()
