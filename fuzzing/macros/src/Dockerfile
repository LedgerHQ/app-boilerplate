FROM ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest

WORKDIR /app

# Copies at /app-folder
COPY . /app

RUN apt-get update && apt-get install -y bear

# Executa o script
CMD ["/bin/bash", "/app/fuzzing/macros/src/script.sh"]
