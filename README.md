# mini_graph

this is a simple graph imp, which define a task depends, task run, task.

## how to build
if cross build or host build changed, please add --remove_old_build to remove old build.
  ```
  update submodule: git submodule update --init --recursive
  ```

- host build
  ```
  release: python3 third_party/cmake_one/cmake_one.py --repo_dir . --build_with_ninja_verbose  host_build
  debug  : python3 third_party/cmake_one/cmake_one.py --build_type Debug --repo_dir . --build_with_ninja_verbose  host_build
  ```

- cross build for android with aarch64
  ```
  release: python3 third_party/cmake_one/cmake_one.py --repo_dir . --build_with_ninja_verbose  cross_build  --cross_build_target_os ANDROID --cross_build_target_arch aarch64
  debug  : python3 third_party/cmake_one/cmake_one.py --build_type Debug --repo_dir . --build_with_ninja_verbose  cross_build  --cross_build_target_os ANDROID --cross_build_target_arch aarch64
  ```

## how to test
run gtest with:
  ```
  host build: ./build/test/mini_graph_test
  cross build for android: adb push build/test/mini_graph_test /data/local/tmp/mini_graph_test && adb shell /data/local/tmp/mini_graph_test
  ```
