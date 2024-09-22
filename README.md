# mini_graph

this is a simple graph imp, which define a task depends, task run, and etc.

## how to build

if cross build or host build changed, please add --remove_old_build to remove old build.

```
update submodule: git submodule update --init --recursive
```

- host build
  ```
  release: python3 third_party/cmake_one/cmake_one.py --remove_old_build --repo_dir . --build_with_ninja_verbose  host_build
  debug  : python3 third_party/cmake_one/cmake_one.py --remove_old_build --build_type Debug --repo_dir . --build_with_ninja_verbose  host_build
  ```
- cross build for android with aarch64
  ```
  release: python3 third_party/cmake_one/cmake_one.py --remove_old_build --repo_dir . --build_with_ninja_verbose  cross_build  --cross_build_target_os ANDROID --cross_build_target_arch aarch64
  debug  : python3 third_party/cmake_one/cmake_one.py --remove_old_build --build_type Debug --repo_dir . --build_with_ninja_verbose  cross_build  --cross_build_target_os ANDROID --cross_build_target_arch aarch64
  ```
- cross build for OHOS with aarch64
  ```
  release: python3 third_party/cmake_one/cmake_one.py --remove_old_build --repo_dir . --build_with_ninja_verbose  cross_build  --cross_build_target_os OHOS --cross_build_target_arch aarch64
  debug  : python3 third_party/cmake_one/cmake_one.py --remove_old_build --build_type Debug --repo_dir . --build_with_ninja_verbose  cross_build  --cross_build_target_os OHOS --cross_build_target_arch aarch64
  ```

## how to test

run gtest with:

```
host build: ./build/test/mini_graph_test
cross build for android: adb push build/test/mini_graph_test /data/local/tmp/mini_graph_test && adb shell "cd  /data/local/tmp/ && /data/local/tmp/mini_graph_test"
```

## how to use

more use case, plsease refs to test/mini_graph.cpp

- define a task and add it`s depends

```
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });

    g.dependency("A", "B");

```

- use a subgraph

```
    Graph g(4);                                                                                                                                                                                                                           
                                                                                                                                                                                                                                          
    g.add_task("A", []() {
        printf("A start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        printf("A end\n");
    });

    g.add_task("B", []() {
        printf("B start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        printf("B end\n");
    });

    g.dependency("A", "B");

    Graph sub_g(2);

    sub_g.add_task("C", []() {
        printf("C start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("C end\n");
    });

    sub_g.add_task("D", []() {
        printf("D start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("D end\n");
    });

    sub_g.dependency("C", "D");
    sub_g.freezed();

    g.add_task("sub_g", [&sub_g]() { sub_g.execute(); });
    g.dependency("B", "sub_g");

    g.freezed();
    g.execute();

```
# TODO LIST
- [ ] auto search for best performance by random add virtual_dependency for node
- [ ] power ABI interface from linux `/sys/class/power_supply/battery/` for statistics of power consumption
- [ ] auto search for power consumption base power ABI interface
- [ ] linux-base(Android/OHOS/Linux) deadline sched attr export
