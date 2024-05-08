# Quantum Resource Manager (QRM)

The entry point of the Quantum Resource Manager for selecting and applying LLVM passes to a Quantum Circuit described on a Quantum Intermediate Representation ([QIR](https://www.qir-alliance.org/projects/)) is `qresourcemanager_d`. This README provides instructions for installing and uninstalling `qresourcemanager_d`, as well as for running an example submitting a quantum task.

## Downloading

1. To clone this repository to your local machine, use the following command:
   ```bash
   git clone git@github.com:Munich-Quantum-Software-Stack/QRM.git qrm.git
   ```

2. After cloning, make sure you are at the right branch:
   ```bash
   cd qrm.git
   git checkout testing
   ```

3. Install `pre-commit` using `pip` (Python's package manager) and other required dependencies using `apt`:
   ```bash
   sudo apt update
   sudo apt install -y cmake clang-format pre-commit cmakelang python3-pip
   pip3 install cmake_format
   echo 'export PATH="$PATH:$HOME/.local/bin"' >> ~/.bashrc
   source ~/.bashrc
   ```

4. After installing `pre-commit`, set up the hooks specified in `.pre-commit-config.yaml`:
   ```bash
   pre-commit install
   make pre-commit
   ```

## Building

To install the Quantum Resource Manager daemon system wide, follow these steps:

1. Install the required dependencies:
   ```bash
   sudo apt update
   sudo apt install -y cmake llvm llvm-dev rabbitmq-server g++ curl libgtest-dev nlohmann-json3-dev libjansson-dev libcjson-dev libcurl4-openssl-dev
   ```

2. Set the following environment variables:
   ```bash
   export PASSES=/path/to/pass/libraries
   ```

3. Navigate to the `qrm.git` directory (if you are not already there):
   ```bash
   cd qrm.git/
   ```

4. Run `make` to install `qresourcemanager_d`:
   - One can install the daemon in the default directory, i.e., `$HOME/bin`, with the following command:
      ```bash
      make install
      ```

   - One may also specify the installation path, and a directory where the build files can be written to. Note that the equivalent command to the one above is:
      ```bash
      make INSTALL_PATH=$HOME \
           BUILD_DIR=build \
           install
      ```

## Uninstallation

If you ever need to uninstall `qresourcemanager_d`, follow these steps:

1. Navigate to the `qrm.git` directory (if you are not already there):
   ```bash
   cd qrm.git/
   ```

2. Run the uninstall target using sudo:
   ```bash
   sudo make uninstall
   ```

This will remove `qresourcemanager_d` from your system.

<!--
## Project Structure

The project structure is the following:

```
├─ .clang-format
├─ .gitignore
├─ .pre-commit-config.yaml
├─ CMakeLists.txt
├─ CODE_OF_CONDUCT.md
├─ CONTRIBUTING.md
├─ LICENSE
├─ Makefile
├─ README.md
├─ benchmarks
│  └─ test.ll
├─ cmake
│  ├─ Findfomac.cmake
│  ├─ Findqdmi.cmake
│  ├─ Findqinfo.cmake
│  └─ FindSphinx.cmake
├─ docs
│  ├─ CMakeLists.txt
│  ├─ Doxyfile.in
│  └─ html
│     ├─ index.html
│     └─ ...
├─ include
│  ├─ connection_handling.hpp
│  ├─ generator_runner
│  │  └─ GeneratorRunner.hpp
│  ├─ pass_runner
│  │  ├─ llvm.hpp
│  │  ├─ PassModule.hpp
│  │  ├─ PassRunner.hpp
│  │  └─ QirPassRunner.hpp
│  ├─ scheduler_runner
│  │  └─ SchedulerRunner.hpp
│  └─ selector_runner
│     └─ SelectorRunner.cpp
├─ scripts
│  ├─ kill_daemons.sh
│  └─ generate_docs.sh
├─ src
│  ├─ connection_handling.cpp
│  ├─ qresourcemanager_d.cpp
│  ├─ generator_runner
│  │  ├─ generators
│  │  │  ├─ CMakeLists.txt
│  │  │  ├─ generator_cutter.cpp
│  │  │  └─ ...
│  │  └─ GeneratorRunner.cpp
│  ├─ pass_runner
│  │  ├─ PassRunner.cpp
│  │  └─ QirPassRunner.cpp
│  ├─ scheduler_runner
│  │  ├─ schedulers
│  │  │  ├─ CMakeLists.txt
│  │  │  ├─ scheduler_round_robin.cpp
│  │  │  └─ ...
│  │  └─ SchedulerRunner.cpp
│  └─ selector_runner
│     ├─ selectors
│     │  ├─ CMakeLists.txt
│     │  ├─ selector_all.cpp
│     │  └─ ...
│     └─ SelectorRunner.cpp
└─ tests
   ├─ CMakeLists.txt
   └─ test.cpp
```
-->

## Documentation and Resources

This section provides links to project documentation and additional resources:

- [Documentation](https://lrz-qct-qis.gitlabpages.devweb.mwn.de/munich-quantum-compiler/quantum-resource-manager/files.html): Detailed documentation about the Quantum Resource Manager.
- [Wiki](https://gitlab-int.srv.lrz.de/lrz-qct-qis/munich-quantum-compiler/quantum-resource-manager/-/wikis/home): Project wiki with additional information and guides.
- [Contributing Guidelines](CONTRIBUTING.md): Document to understand the process for contributing to our project.
<!--
- Flowchart:
![Alt](flowcharts/flow.png)

## Building Documentation

You can build the Quantum Resource Manager and generate its documentation locally using Doxygen:

1. Install the required dependencies for Doxygen:
   ```bash
   sudo apt update
   sudo apt install -y cmake llvm rabbitmq-server g++ curl flex bison libgtest-dev nlohmann-json3-dev
   ```

2. Run make:
   - One can install the daemon in the default directory, i.e., `$HOME/bin`, and generate its documentation with the following command:
      ```bash
      make docs
      ```

   - One may also specify the installation path, and a directory where the build files can be written to. Note that the equivalent command to the one above is:
      ```bash
      make INSTALL_PATH=$HOME \
           BUILD_DIR=build \
           docs
      ```

3. Open the generated documentation in a web browser:
   ```bash
   xdg-open docs/html/index.html
   ```

   Alternatively, you can manually open the file `documentation/html/index.html` with your preferred web browser.

4. Once the forked branch is merged, the up-to-date documentation can be accessed online [here](https://lrz-qct-qis.gitlabpages.devweb.mwn.de/munich-quantum-compiler/quantum-resource-manager/index.html).
-->

## Running Examples

You can run the Quantum Resource Manager daemon and a test client as follows:

1. Install the QIR Passes project to obtain the passes:
   - Clone the project:
      ```bash
      git clone git@github.com:Munich-Quantum-Software-Stack/passes.git passes.git
      ```

   - Navigate to the `passes.git` directory (if you are not already there) and move to the right branch:
      ```bash
      cd passes.git
      ```

  - Build the passes as shared libraries:
      ```bash
      make INSTALL_PATH=$PASSES_LIBRARY_PATH install
      ```

  - Add the installation path to the environment variable `PASSES`:
      ```bash
      export PASSES=$PASSES:PASSES_LIBRARY_PATH
      ```

2. Navigate to the `qrm.git` directory (if you are not already there):
   ```bash
   cd qrm.git/
   ```

3. Run the following command:
   ```bash
   sh test.sh
   ```
