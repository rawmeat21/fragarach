# Fragarach

**Fragarach** is a Linux based behavioral malware detection system implemented as a command line tool. It executes a target binary within a fully isolated sandbox, observes its runtime behavior through an eBPF-powered syscall tracing layer, constructs a weighted syscall transition graph, and classifies the binary as benign or malicious using a Graph Neural Network. The tool operates entirely at the kernel interface level and requires no source code, signatures, or prior knowledge of the binary.


## Architecture

Fragarach is composed of four sequential stages:

### Stage 1: Sandbox
The target binary is executed within an isolated environment constructed using Linux kernel primitives:

- **Namespaces**:  PID, mount, and network namespaces provide process, filesystem, and network isolation
- **Overlay filesystem**: A writable overlay is mounted over a read-only Debian rootfs. All filesystem writes are directed to a disposable upper layer that is discarded after each run
- **cgroups**: Memory, CPU, and PID limits are enforced to prevent resource exhaustion
- **Capability dropping**: All Linux capabilities are cleared via `cap_clear()` before the binary is executed
- **Seccomp notify**: A seccomp filter intercepts dangerous syscalls (like `ptrace`, `init_module`, `kexec_load`, `reboot`, `setuid`, and others), denies them with `EPERM`, and logs them as blocked events

A synchronisation pipe ensures the eBPF tracer is fully attached before the binary begins execution.

### Stage 2: eBPF Observation Layer
An eBPF program is attached to the `tracepoint/raw_syscalls/sys_enter` tracepoint. On every syscall, it checks whether the calling PID matches the target. If it does, it writes an event record (containing the PID, syscall number, and timestamp) into a BPF ring buffer. The userspace tracer polls both the ring buffer and the seccomp notify file descriptor simultaneously, collecting all events including those blocked by seccomp.

### Stage 3: Syscall Graph Construction
The flat event stream is converted into a directed weighted graph in COO (coordinate) format:

- **Nodes**: unique syscall numbers observed during execution, annotated with a `blocked` flag
- **Edges**: directed transitions between consecutive syscalls
- **Edge weights**: the number of times each transition occurred

The graph is serialised to JSON and either saved to disk (training mode) or passed directly to the inference engine (inference mode).

### Stage 4: GNN Classification
The graph is classified by a Graph Neural Network implemented in PyTorch Geometric and exported for C++ inference via TorchScript. The model architecture consists of:

- An embedding layer mapping syscall numbers to 16-dimensional vectors
- Two GCNConv message-passing layers (hidden dimension 64) with ReLU activations and dropout
- Global mean pooling to produce a fixed-size graph-level representation
- A linear classifier producing scores for two classes: benign and malicious

The C++ inference engine loads the TorchScript model via LibTorch, converts the COO graph to tensors, and returns a verdict with a confidence score.

---

## Requirements

- Linux (Debian-based distribution recommended)
- Kernel 5.8 or later with eBPF support
- cgroups v2 enabled
- Root access is required to run the executable
- CMake 3.15+
- Clang
- `bpftool`

---

## Installation

Clone the repository and run the installation script:

```bash
git clone https://github.com/rawmeat21/fragarach
cd fragarach
sudo ./install.sh
```

The installation script performs the following steps:

1. Installs all required system dependencies via `apt` (change this line if you use a different package manager!)
2. Constructs a minimal Debian rootfs at `/opt/fragarach/rootfs` using `debootstrap`
3. Downloads and installs LibTorch (CPU build) to `/opt/libtorch`
4. Copies a pretrained model to `/opt/fragarach/model_scripted.pt`

Build the project:

```bash
cmake -B build
cmake --build build
sudo cp build/fragarach /usr/local/bin/fragarach
```

---

## Usage

**Inference mode**: Classify an unknown binary:

```bash
sudo fragarach /path/to/binary
```

Example output:
```
Verdict: MALICIOUS
Confidence Score: 0.9871
```

**Training mode**: collect behavioral data from a known sample:

```bash
sudo fragarach /path/to/binary 0   # benign
sudo fragarach /path/to/binary 1   # malicious
```

Syscall graphs are saved as JSON files to `/opt/fragarach/raw/`.

---

## Training a Custom Model

Collect a sufficient number of labeled samples in both classes, then execute the training script:

```bash
cd src/GNN
python train.py
```

The script loads graphs from `/opt/fragarach/raw/`, trains the GNN for 100 epochs using the Adam optimizer and cross-entropy loss, and saves the following outputs:

- `/opt/fragarach/model.pt`: model weights (state dict)
- `/opt/fragarach/model_scripted.pt`: TorchScript model for C++ inference

Dependencies:

```bash
pip install torch torch-geometric
```

---

## Collecting Malware Samples

Linux ELF malware samples may be obtained from [MalwareBazaar](https://bazaar.abuse.ch).
Benign samples may be collected from standard system binaries:

```bash
cp /usr/bin/* /opt/fragarach/benign/
```

> **Warning:** Malware samples MUST only be executed inside a virtual machine. NEVER execute malware samples directly on your host system.

---

## Pretrained Model

The model included in `models/model_scripted.pt` was trained on approximately 1,800 benign samples and 600 malicious samples drawn from several Linux malware families including Mirai, Gafgyt, and XorDDoS. Achieved test accuracy: **98.71%**.

Users requiring higher accuracy or coverage of additional malware families are encouraged to collect a larger and more diverse dataset and retrain the model using the provided training script.

---

## Security Controls

| Control | Implementation |
|---|---|
| Network isolation | `CLONE_NEWNET`- blank network stack, no external connectivity |
| Filesystem isolation | Overlay FS over read-only Debian rootfs |
| Process isolation | `CLONE_NEWPID`- sandbox has its own PID namespace |
| Capability dropping | `cap_clear()` before `execve` |
| Dangerous syscall blocking | Seccomp notify mode with `EPERM` response |
| Resource limits | cgroups v2 - memory, CPU, and PID caps |
| Symlink safety | `lstat` based directory traversal prevents symlink following |

---

## License

This project is provided for educational and research purposes.
