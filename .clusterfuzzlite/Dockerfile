FROM gcr.io/oss-fuzz-base/base-builder:v1
COPY . $SRC/app-boilerplate
COPY ./.clusterfuzzlite/build.sh $SRC/
RUN git clone https://github.com/LedgerHQ/ledger-secure-sdk.git $SRC/app-boilerplate/BOLOS_SDK
WORKDIR $SRC/app-boilerplate