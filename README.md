The repository is forked from [dslab-epfl/concord](https://github.com/dslab-epfl/concord.git).

### Prerequisites

Make sure the submodule `benchmarks/overhead` is cloned.

Ensure that `clang-11`, `clang++-11`, and `opt-11` are installed. You can verify their availability with:

```bash
clang-11 --version
clang++-11 --version
opt-11 --version
```

You might be able to install them via:

```bash
sudo apt install llvm-11
```

Other dependencies are listed in `setup.sh`. Please **review the script carefully** before running it to avoid overwriting existing library versions.

You will also need input data to run the benchmarks:

- For the **Phoenix** benchmark suite:  
  Download [this](https://drive.google.com/file/d/1MbDowfcB9jSgHlOKnAuv3dtJyJrPEtUb/view?usp=share_link) into `benchmarks/overhead/phoenix`, then run:
  ```bash
  benchmarks/overhead/phoenix/dataset.sh
  ```

- For the **PARSEC** benchmark suite:  
  Download [this](https://drive.google.com/file/d/1i6iv_kPt55wa3zUKJPtB7SAf2ALE47dM/view?usp=share_link) into `benchmarks/overhead/parsec-benchmark`, then run:
  ```bash
  benchmarks/overhead/parsec-benchmark/dataset.sh
  ```

### Running Experiments

First, activate the environment variables:

```bash
source env.sh
```

Then run benchmarks using different preemption mechanisms:

```bash
./run.sh uintr
# Output: benchmarks/overhead/overhead_results-uintr.txt

./run.sh concord
# Output: benchmarks/overhead/overhead_results-concord.txt

./run.sh signal
# Output: benchmarks/overhead/overhead_results-signal.txt
```

Each result file reports slowdown (e.g., `1.1` means 10% slowdown).

### Notes

- You can adjust the evaluted quantum values in `run.sh` line 5:
  ```bash
  quantum=(200 100 50 30 20 15 10 5 3)
  ```
  The slowdown results for these quantum values are recorded in the output file in the same order as they appear in the array.

- To reduce the number of trials (at the cost of increased noise), uncomment line 60 in `benchmarks/overhead/run.py`.

- Core usage is hardcoded and may vary depending on the specific machine:
  - Timer core: `src/lib/concord.c`, line 37  
    ```c
    #define DISPATCHER_CORE 2
    ```
  - Program cores for PARSEC benchmarks:  
    `benchmarks/overhead/parsec-benchmark/pkgs/perf_test.sh`, line 78  
  - Program cores for Phoenix benchmarks:  
    `benchmarks/overhead/phoenix/phoenix-2.0/perf_test.sh`, line 58
