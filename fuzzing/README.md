# Fuzzing on transaction parser

## Fuzzing

Fuzzing allows us to test how a program behaves when provided with invalid, unexpected, or random data as input.

In the case of the harness `fuzz_tx_parser.c`, we want to test the code that is responsible for parsing the transaction data, which is `transaction_deserialize()`. To test `transaction_deserialize()`, our fuzz target, `fuzz_tx_parser.c`, needs to implement `int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)`, which provides an array of random bytes that can be used to simulate a serialized transaction. If the application crashes, or a [sanitizer](https://github.com/google/sanitizers) detects any kind of access violation, the fuzzing process is stopped, a report regarding the vulnerability is shown, and the input that triggered the bug is written to disk under the name `crash-*`. The vulnerable input file created can be passed as an argument to the fuzzer to triage the issue.

> **Note**: Usually we want to write a separate fuzz target for each functionality.

However, it is also possible to target the main function/dispatcher, so that we can cover more code, as it is done in `fuzz_dispatcher.c`.

## Manual usage based on Ledger container

### Preparation

The fuzzer can be run using the Docker image `ledger-app-dev-tools`. You can download it from the `ghcr.io` docker repository:

```console
docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```

You can then enter this development environment by executing the following command from the repository root directory:

```console
docker run --rm -ti -v "$(realpath .):/app" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```
*Or use this one while we wait for the SDK_FUZZING_FRAMEWORK release* (setting the path/to/sdk)

```console
docker run --rm -ti -v "$(realpath .):/app" -v "$(realpath /path/to/sdk):/ledger-secure-sdk" ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest
```
### Writing your Harness
When writing your harness, keep the following points in mind:
* An SDK's interface for compilation is provided via the target ```extra``` in CMakeLists.txt
* If you are running it for the first time, consider using the script ```local_run``` from inside the Docker container using the flags build=1 and ```re-generate-macros=2```, if you need to manually add/remove macros you can then do it using the files macros/add_macros.txt or macros/exclude_macros.txt and regenerate it, or directly change the macros/generated/macros.txt and then using ```re-generate-macros=0```.
* If your fuzzer uses the function ```os_sched_exit()```, your harness must have the code:
    ```console
    #include <setjmp.h>
    ...
    jmp_buf fuzz_exit_jump_buf;
    int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
        if (setjmp(fuzz_exit_jump_buf) == 0){
            ### harness code ###
        }
        return 0;
    }
    ``` 
    This allows a return point when the ```os_sched_exit()``` function is mocked.
* To provide an SDK interface, we automatically generate syscall mock functions located in ```SECURE_SDK_PATH/fuzzing/mock/generated/generated_syscalls.c```, if you need a more specific mock, you can define it in ```APP_PATH/fuzzing/mock``` with the same name and without the WEAK attribute.

### Compile and run the fuzzer from the container

Once inside the container, navigate to the ```fuzzing``` folder to compile the fuzzer:

```console
cd fuzzing

./local_run.sh --build=1 --re-generate-macros=1 --TARGET_DEVICE=stax --BOLOS_SDK=/ledger-secure-sdk/ --fuzzer=build/fuzz_dispatcher --run-fuzzer=1 --compute-coverage=1
```

### About local_run.sh

| Parameter | Type     | Description                |
| :-------- | :------- | :------------------------- |
| `--TARGET_DEVICE` | `flex or stax` | **Optional**. Whether it is a flex or stax device (default: flex) |
| `--BOLOS_SDK` | `PATH TO BOLOS SDK` | **Required**. Path to the BOLOS SDK |
| `--re-generate-macros` | `bool` | **Optional**. Whether to regenerate macros or not (default: 0) |
| `--build` | `bool` | **Optional**. Whether to build the project (default: 0) |
| `--fuzzer` | `PATH` | **Required**. Path to the fuzzer binary |
| `--compute-coverage` | `bool` | **Optional**. Whether to compute coverage after fuzzing (default: 0) |
| `--run-fuzzer` | `bool` | **Optional**. Whether to run or not the fuzzer (default: 1) |
| `--help` |  | **Optional**. Display help message |

### Visualizing code coverage

After running your fuzzer, if ```--compute-coverage=1``` you will be prompted with the coverage, but you can also run the following code to have a web view of it.
```console
xdg-open out/index.html
```

## Full usage based on `clusterfuzzlite` container - TODO after SDK FUZZING RELEASE

Exactly the same context as the CI, directly using the `clusterfuzzlite` environment.

More info can be found here:
<https://google.github.io/clusterfuzzlite/>

### Preparation

The principle is to build the container, and run it to perform the fuzzing.

> **Note**: The container contains a copy of the sources (they are not cloned), which means the `docker build` command must be re-executed after each code modification.

```console
# Prepare directory tree
mkdir fuzzing/{corpus,out}
# Container generation
docker build -t app-boilerplate --file .clusterfuzzlite/Dockerfile .
```

### Compilation

```console
docker run --rm --privileged -e FUZZING_LANGUAGE=c -v "$(realpath .)/fuzzing/out:/out" -ti app-boilerplate
```

### Run

```console
docker run --rm --privileged -e FUZZING_ENGINE=libfuzzer -e RUN_FUZZER_MODE=interactive -v "$(realpath .)/fuzzing/corpus:/tmp/fuzz_corpus" -v "$(realpath .)/fuzzing/out:/out" -ti gcr.io/oss-fuzz-base/base-runner run_fuzzer fuzz_tx_parser
```
