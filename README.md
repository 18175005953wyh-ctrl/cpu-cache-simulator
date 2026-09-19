# CPU Cache Simulator

使用 C11 编写的命令行 Cache 模拟器，读取十六进制访存记录，展示地址映射、Hit/Miss、LRU 替换和统计报告。

## 项目背景与学习目标

本项目把 408 计算机组成原理中的 Cache 地址映射与替换策略变成可运行的小实验。选择这个题目，是为了把“背公式”变成“输入地址、观察变化、解释原因”，同时练习 C 语言结构体、动态内存、文件解析与测试。

它只模拟单级 Cache 的地址与元数据，不执行 CPU 指令，也不保存内存里的真实数据。命中率是给定 Trace 的模拟结果，不是实际 CPU 的性能测试。

## 功能

- 可配置容量、块大小、相联度；支持直接映射、N 路组相联，以及组数为 1 时的全相联。
- 使用有效位、Tag 和最近访问时间戳实现 LRU；先填空行，只有替换有效行才计 eviction。
- R/W 都会更新 LRU；写缺失按**写分配**载入块。不模拟写直达、脏位或写回流量。
- 每条有效访问显示序号、类型、64 位地址、命中/缺失、替换、组索引、Tag 和偏移。
- 统计读写次数、Hit、Miss、Eviction 及比例，支持带 UTC 时间的文本报告。
- 检查参数、数值溢出、文件错误、损坏记录和超长行；错误记录带物理行号警告并跳过。

## Cache 基础

CPU 运算速度通常高于主存响应速度。Cache 保存近期可能再用的数据副本，使部分访问可以由更靠近 CPU 的快速存储满足。本模拟器不计算这些时间。

- **时间局部性**：刚访问过的数据，短时间内可能再次访问，例如循环中的变量。
- **空间局部性**：访问某个地址后，可能访问附近地址，例如顺序遍历数组。
- **数据块**：内存传入 Cache 的单位；块大小为 16 字节时，地址 0～15 属于同一块。
- **Cache 行**：存放一个块的位置及其有效位、Tag 等元数据。这里仅保存元数据。
- **组与相联度**：行被分成若干组，每组有 E 行；相联度就是 E。
- **直接映射**：E=1，每个块只有一个候选行，容易实现，但不同块可能争用同一行。
- **组相联**：E>1，一个块可放入对应组中的任一行，减少某些冲突，但查找和替换更复杂。

配置关系：`行数 = 容量 / 块大小`，`组数 = 行数 / 相联度`。
块大小与组数必须是 2 的整数次幂；相联度本身不必，例如 96 B、16 B、3 路有 2 组。

### 地址如何拆分

```text
| Tag | Set Index | Block Offset |
块号     = address / block_size
组索引   = 块号 % set_count
Tag      = 块号 / set_count
块内偏移 = address % block_size
```

例如容量 64 B、块大小 16 B、直接映射：共有 4 组，偏移占 4 位、组索引占 2 位。
地址 `0x97`（151）对应块号 9、组 1、Tag 2、偏移 7。
组索引确定在哪一组找；Tag 区分映射到同一组的不同块；偏移确定块内字节位置。
有效位为 0 的行即使 Tag 为 0 也不能算命中。

`decode_address()` 使用整数除法和取模，位数用整数循环计算，不依赖浮点 `log2()`。

### LRU

每次有效访问递增时钟。命中行或新装入行的 `last_used` 更新为当前时钟。
缺失时优先用空行，满组则替换时间戳最小的行。因此**命中也必须更新时间**。
每次访问最多扫描组内 E 行，时间复杂度 O(E)；M 次访问的核心计算为 O(ME)。
初始化 O(L)，元数据空间 O(L)，L 为总行数；Trace 逐行处理，不整体载入内存。

## 目录结构

```text
cpu-cache-simulator/
├── src/                 # main.c、cache.c、trace.c、report.c
├── include/             # cache.h、trace.h、report.h
├── data/                # sample/locality/conflict_trace.txt
├── tests/               # test_runner.c、test_cli.cmake
├── output/.gitkeep      # 用户生成报告的位置
├── screenshots/.gitkeep # 真实运行截图的位置
├── CMakeLists.txt
├── .gitignore
├── LICENSE
└── README.md
```

`main.c` 解析参数并连接各模块；`cache.c` 负责映射和 LRU；`trace.c` 负责记录校验、模拟和计数；`report.c` 负责输出。头文件只放类型和声明。

## 技术与构建环境

C11、标准库、`uint64_t`、`size_t`、动态数组、整数位运算、CMake ≥3.20、CTest。
没有 C++ 和第三方依赖。实际验证环境：Windows、Visual Studio 2022 MSVC 19.44.35225.0、CMake/CTest 3.31.6-msvc6，使用 NMake Debug 构建，`/W4 /WX` 无警告通过。
GCC/Clang 的警告选项已配置，但本次没有在 GCC/Clang、Linux 或 macOS 上实际运行。

**以下命令均在 `cpu-cache-simulator` 文件夹中执行。** 它是一个独立 CMake 项目。

### Visual Studio 2022

安装“使用 C++ 的桌面开发”工作负载（其中也包含 C 编译器）及 CMake 工具；源文件仍按 C11 编译。

最容易复现的运行方式：打开开始菜单中的 **x64 Native Tools Command Prompt for VS 2022**，进入本项目文件夹，执行：

```bat
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DSTRICT_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
build\cpu_cache_simulator.exe --cache-size 64 --block-size 16 --associativity 1 --trace data/sample_trace.txt
```

在 IDE 内查看代码可选择“文件 → 打开 → 文件夹”，选择本项目文件夹。程序需要命令行参数，直接点运行而未配置参数时会显示用法提示。

也可生成解决方案（使用不同构建目录，不混用生成器）：

```bat
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Debug
ctest --test-dir build-vs -C Debug --output-on-failure
build-vs\Debug\cpu_cache_simulator.exe --cache-size 64 --block-size 16 --associativity 1 --trace data/sample_trace.txt
```

打开生成的 `build-vs/CPUCacheSimulator.sln`，将 `cpu_cache_simulator` 设为启动项目，按 Ctrl+F5。CMake 为此生成器设置了示例参数和项目目录作为调试工作目录；不要选择 `cache_tests` 作为演示程序。

### GCC / Clang 与通用 CMake

有 C 编译器和构建工具后：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTRICT_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/cpu_cache_simulator --cache-size 64 --block-size 16 --associativity 1 --trace data/sample_trace.txt
```

可在首次配置时加 `-DCMAKE_C_COMPILER=gcc` 或 `-DCMAKE_C_COMPILER=clang`；更换编译器请换一个构建目录。以上单配置路径用于 Make/Ninja；多配置生成器需要指定 `--config`、CTest `-C`，可执行文件也会多一层配置目录。

## 参数与输入格式

| 参数 | 含义 |
|---|---|
| `--cache-size` | 模拟的数据容量，字节，不含元数据 |
| `--block-size` | 块大小，字节，正的 2 次幂 |
| `--associativity` | 每组行数，正整数 |
| `--trace` | 必填，访存记录路径 |
| `--report` | 可选，新报告文件路径 |
| `--help` | 单独使用，显示帮助 |

前三项使用十进制正整数，不接受符号、小数或后缀。所有必填项都必须出现；重复、未知及缺值参数返回非零。
容量须整除块大小，总行数须整除相联度，组数须为正的 2 次幂。最多 1,048,576 行，分配失败给出错误。
相对文件路径从**启动程序时的工作目录**解析，程序不会自动搜索 Trace。

```text
# R = read, W = write
R 0x00000000
W 0X00000010
R ff
```

操作符只接受大写 R/W；地址均按十六进制解释（`10` 也表示十六进制 0x10）。可省略前缀，支持 `0x/0X` 和大小写十六进制数字。范围是 0～UINT64_MAX。
允许前后空白、LF/CRLF、空行、末行无换行、前导空白后的 `#` 整行注释。不支持行尾注释、UTF-8 BOM 或其他附加字段。
每行最多 1023 字节（不含 LF，CR 计入）；超长行会完整跳过，嵌入 NUL 的行也拒绝。
损坏记录只警告，不影响其他合法行，退出状态仍为 0；文件无法打开、读取失败、报告失败等返回非零。
空文件或全部损坏时显示 `No valid accesses.`，两个比例为 `N/A`。

## 示例与真实结果

三个文件容量均用 64 B、块大小均为 16 B：

| Trace | 路数 | 访问数 | 读/写 | Hit | Miss | Eviction | 命中率 |
|---|---:|---:|---|---:|---:|---:|---:|
| sample_trace.txt | 1 | 10 | 7/3 | 4 | 6 | 2 | 40.00% |
| locality_trace.txt | 2 | 6 | 6/0 | 5 | 1 | 0 | 83.33% |
| conflict_trace.txt | 1 | 6 | 6/0 | 0 | 6 | 5 | 0.00% |
| conflict_trace.txt | 2 | 6 | 6/0 | 4 | 2 | 0 | 66.67% |

sample 展示读写、同块命中与替换；locality 展示临近字节和重复访问；conflict 交替访问 0x0、0x40，两块争用直接映射的一行，但可共存于两路组相联的一组中。

Windows NMake 构建后的示例：

```bat
build\cpu_cache_simulator.exe --cache-size 64 --block-size 16 --associativity 2 --trace data/locality_trace.txt
build\cpu_cache_simulator.exe --cache-size 64 --block-size 16 --associativity 1 --trace data/conflict_trace.txt --report output/report.txt
build\cpu_cache_simulator.exe --cache-size 64 --block-size 16 --associativity 2 --trace data/conflict_trace.txt
```

sample 的真实终端输出节选：

```text
#1 R 0x0000000000000000 -> MISS | set=0 tag=0x0 offset=0
#2 R 0x0000000000000004 -> HIT | set=0 tag=0x0 offset=4
#3 W 0x0000000000000010 -> MISS | set=1 tag=0x0 offset=0
#4 R 0x0000000000000000 -> HIT | set=0 tag=0x0 offset=0
#5 W 0x0000000000000040 -> MISS EVICTION | set=0 tag=0x1 offset=0

===== Cache Simulation Summary =====
Cache size:        64 bytes
Block size:        16 bytes
Associativity:     1
Cache lines:       4
Number of sets:    4
Offset bits:       4
Index bits:        2
Total accesses:    10
Read accesses:     7
Write accesses:    3
Hits:              4
Misses:            6
Evictions:         2
Hit rate:          40.00%
Miss rate:         60.00%
```

## 报告与截图

`--report output/report.txt` 写入 UTC 模拟完成时间、Trace 的文件名、策略、配置和完整统计。不会写入开发机器路径；Trace 参数即使是绝对路径，报告也只保留文件名。
使用 C11 独占创建模式，**已有文件不覆盖**，再次运行换成 `output/report-2.txt` 等新文件名。这样也不会因路径别名而覆盖 Trace。
不自动创建父目录；仓库提供 `output/`，其他目录需自行创建。无法创建、写入或关闭文件时给出明确提示和非零退出码。写入失败可能留下不完整报告，应删除后重试。
生成的 `output/*.txt` 被忽略。将自己截取的真实界面放进 `screenshots/`；目前只有占位文件，没有虚构截图。

## 自动化测试与实际验证

测试不依赖第三方框架，使用会在 Release 模式下保留的 CHECK 宏；失败返回非零，CTest 能发现失败。

2026-09-19 实测：**36 项 C 测试 + 24 项命令行测试全部通过**，CTest 两个测试入口均通过。
其中一个 C 测试用独立的“最近访问块列表”模型核对 1～4 路共 4000 次访问的 Hit 与 Eviction，避免只复述时间戳实现。

覆盖配置、64 位最大地址、首次缺失、同块命中、组隔离、空行填充、LRU 命中更新、三路全相联、写分配统计、不合法行、超长行、嵌入 NUL、空数据、无有效记录、计数溢出保护、报告生成及拒绝覆盖。
命令行测试另覆盖缺失参数、重复参数、负数、数值溢出、错误文件路径和报告父目录不存在。

实际从仓库根目录执行的构建/测试命令（在已加载 MSVC 环境中）：

```bat
cmake -S cpu-cache-simulator -B work/cache-nmake -G "NMake Makefiles" -DSTRICT_WARNINGS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build work/cache-nmake
ctest --test-dir work/cache-nmake --output-on-failure
```

构建目录位于仓库忽略的 `work/cache-nmake` 下，不纳入项目提交；复现时也可按前面命令重新构建。在构建目录直接运行 `cache_tests.exe` 也能测试，测试会在当前目录创建并清理 `test-trace.tmp` 和 `test-report.tmp`。

## 当前限制与改进方向

- 每条记录只代表对一个地址所在块的一次访问，不包含访问长度和跨块拆分。
- 只模拟一个 Cache 层级，初始为空；不保存数据，不模拟脏位、写回、时延、一致性和地址转换。
- 所有缺失统一计为 Miss，尚未区分冷、容量与冲突缺失；报告不包含逐条记录，终端包含。
- 最多 1,048,576 行；最多处理 UINT64_MAX 次有效访问，达到时钟上限后明确停止，避免 LRU 时间戳回绕。
- 库接口要求先初始化、用后销毁；重新初始化前需先销毁。直接调用 `cache_access` 时调用方须保证时钟尚未到上限，文件处理函数已经检查。
- 后续可加入 FIFO/随机替换、写直达与写回、脏位、多级 Cache、平均访存时间和配置对比图表。

## 练习到的能力

结构体与模块划分、动态数组的申请和释放、无符号整数范围检查、逐行文件处理、缓存地址映射、LRU、统计不变量、CMake/CTest 和通过小例子验证算法。
建议先读 `cache.c`，再读 `trace.c`，最后看 `main.c` 如何把流程连起来。

## 复试讲解要点

1. **为什么 Cache 能提高速度？** 利用局部性保留近期可能用到的数据，命中时不必等待较慢主存。收益依赖命中率和硬件延迟，本程序仅计算命中情况。
2. **时间局部性是什么？** 同一数据很快被重复使用，如 Trace 中反复访问地址 0。
3. **空间局部性是什么？** 临近地址被连续访问，装入一个块可满足其中多个字节的后续访问。
4. **地址怎样找到组？** 先除块大小得到块号，再对组数取余。例如块号 9、4 组时选组 1。
5. **Tag 的作用？** 组索引只定位组，Tag 用来判断其中的行是否装着目标块，还须检查有效位。
6. **直接映射为何冲突？** 不同块可能映射到同一行并交替覆盖，即使其他组有空闲也无法借用。
7. **组相联的优缺点？** 同组能保存多个块，减少部分冲突；但需更多比较和替换管理，并非相联度越高就一定更快。
8. **LRU 为什么记录最近使用时间？** 需要区分谁最久未被访问，满组时才知道替换谁。只更新缺失而不更新命中会变成错误策略。
9. **时间复杂度是多少？** 每次访问组内最多检查 E 行，为 O(E)，不是所有配置都 O(1)。处理 M 次访问为 O(ME)，另有文本读取和输出成本。
10. **如何升级？** 可先实现 FIFO 并用同一 Trace 对比，再添加脏位、写策略、多级缓存或带延迟参数的平均访存时间。
