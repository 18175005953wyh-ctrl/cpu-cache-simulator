# CPU Cache Simulator v1.1

使用 C11 编写的命令行 Cache 模拟器，读取十六进制访存记录，展示地址映射、Hit/Miss、LRU/FIFO 替换和同配置对照报告。

## 项目背景与学习目标

本项目把 408 计算机组成原理中的 Cache 地址映射与替换策略变成可运行的小实验。选择这个题目，是为了把“背公式”变成“输入地址、观察变化、解释原因”，同时练习 C 语言结构体、动态内存、文件解析与测试。

它只模拟单级 Cache 的地址与元数据，不执行 CPU 指令，也不保存内存里的真实数据。命中率是给定 Trace 的模拟结果，不是实际 CPU 的性能测试。

## 功能

- 可配置容量、块大小、相联度；支持直接映射、N 路组相联，以及组数为 1 时的全相联。
- 支持 LRU（默认）和 FIFO；先填空行，只有替换有效行才计 eviction。
- 支持同一 Trace 的独立冷缓存对照运行，输出表格和可选 CSV。
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

## Replacement Policies

### LRU

`--policy lru` 淘汰 `last_used` 最小的有效行。命中和装入都刷新最近访问时间；省略策略参数以及原来的 `cache_init()` 均保持 LRU。

### FIFO

`--policy fifo` 淘汰 `inserted_at` 最小的有效行。仅装入新块时记录进入时间，命中绝不刷新进入时间。两者都优先使用无效行；`select_victim()` 只选行，不更改计数。

### Key Difference

LRU 关注“上次使用”，FIFO 关注“本次进入”。命中说明块又被使用了，但它没有重新进入缓存，所以不能刷新 FIFO 的装入时间。共享代码仍维护 `last_used`，FIFO 选择受害行时只看 `inserted_at`。

每次有效访问只递增一次 `uint64_t` 时钟。直接映射只有一个候选行，接受两种策略参数，但结果不会受策略影响。

## Policy Comparison

### Reproduction Command

完成上面的构建步骤后，在项目目录运行（若使用 `build-v11`，将下面的 `build` 换成 `build-v11`）：

```bat
build\cpu_cache_simulator.exe --sets 4 --ways 2 --block-size 16 --policy lru data/policy_difference.trace
build\cpu_cache_simulator.exe --sets 4 --ways 2 --block-size 16 --policy fifo data/policy_difference.trace
build\cpu_cache_simulator.exe --sets 4 --ways 2 --block-size 16 --compare-policies data/policy_difference.trace --csv output/policy_comparison.csv
build\cpu_cache_simulator.exe --sets 4 --ways 2 --block-size 16 --compare-policies data/policy_fifo_advantage.trace
build\cpu_cache_simulator.exe --sets 4 --ways 2 --block-size 16 --compare-policies data/policy_equal.trace
```

程序名称保留 `cpu_cache_simulator`，原有 `--cache-size`、`--associativity`、`--trace` 参数继续可用。CSV 路径必须尚不存在。

### Fixed Cache Configuration

128 字节容量，4 组，每组 2 行，块大小 16 字节；初始为空，读写缺失均分配块，不模拟脏位或写回。比较前把输入复制到临时文件，再依次新建、运行、销毁两个缓存。非法记录只警告一次，两次运行跳过相同记录；不能继承前一次的缓存内容或计数。

### Trace Description

令 A = `0x00`、B = `0x40`、C = `0x80`。块号为 0、4、8，都映射到组 0，因此即使其他组空闲，这三个块也争用同组的两行。

- `policy_difference.trace`：A B A C A，共 5 次读访问。
- `policy_fifo_advantage.trace`：A B A C B，共 5 次读访问。
- `policy_equal.trace`：A B A B，共 4 次读访问。

### Actual Results

2026-09-27 使用本版本实际运行，`policy_difference.trace` 输出：

| Policy | Hits | Misses | Evictions | Hit Rate |
|---|---:|---:|---:|---:|
| LRU | 2 | 3 | 1 | 40.00% |
| FIFO | 1 | 4 | 2 | 20.00% |

另外两组实际结果：

| Trace | Policy | Accesses | Hits | Misses | Evictions | Hit Rate |
|---|---|---:|---:|---:|---:|---:|
| FIFO 占优 | LRU | 5 | 1 | 4 | 2 | 20.00% |
| FIFO 占优 | FIFO | 5 | 2 | 3 | 1 | 40.00% |
| 相同结果 | LRU | 4 | 2 | 2 | 0 | 50.00% |
| 相同结果 | FIFO | 4 | 2 | 2 | 0 | 50.00% |

CSV 中 `hit_rate` 是 0～1 的比例，例如 `0.400000000`，终端是百分数。无有效访问时终端显示 N/A、CSV 比例字段留空，避免把未定义比例当成 0%。

### Result Analysis

前两次访问装入 A 和 B。第三次 A 命中：LRU 将 A 标为最近使用；FIFO 中 A 仍是最早进入。第四次访问 C 缺失时，LRU 淘汰 B，FIFO 淘汰 A。因此第五次若访问 A，LRU 命中而 FIFO 缺失；若改为 B，结论正好反转。相同结果序列只用两个块，不触发替换，因而两者相同。

这些短序列用于揭示机制，不能代表真实工作负载整体表现，更不能推出 LRU 总是优于 FIFO。

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

`main.c` 解析参数、固定对照输入并连接各模块；`cache.c` 负责映射和 LRU/FIFO；`trace.c` 负责记录校验、模拟和计数；`report.c` 负责输出。头文件只放类型和声明。

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
| `--trace` | 访存记录路径，也可直接提供一个位置参数 |
| `--sets` | 组数，与 `--cache-size` 二选一；容量 = 组数 × 路数 × 块大小 |
| `--ways` | `--associativity` 的别名，不可重复提供 |
| `--policy` | 只接受小写 `lru` 或 `fifo`，默认 `lru` |
| `--compare-policies` | 用同配置、同输入分别运行 LRU/FIFO，不与 `--policy`、`--report` 混用 |
| `--csv` | 仅用于比较模式，新 CSV 文件路径 |
| `--report` | 可选，新报告文件路径 |
| `--help` | 单独使用，显示帮助 |

容量或组数、块大小、路数使用十进制正整数，不接受符号、小数或后缀。缓存配置与 Trace 都必须提供；重复、未知及缺值参数返回非零。
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
生成的 `output/*.txt` 和 `output/*.csv` 被忽略。真实界面截图位于 `screenshots/`；旧版截图不代表 v1.1 的对照结果。

## Test Coverage

测试不依赖第三方框架，使用会在 Release 模式下保留的 CHECK 宏；失败返回非零，CTest 能发现失败。

2026-09-27 在 Windows x64、MSVC 19.44、NMake Debug、`/W4 /WX` 下实测：**52 项 C 测试 + 47 项命令行测试全部通过**，CTest 两个入口均通过。原有 36 + 24 项保留；新增 16 + 23 项。

- FIFO 空行优先、最早进入者淘汰、命中保留装入时间、新装入时间刷新。
- LRU 命中刷新最近访问时间、默认 LRU、非法策略及大小写拒绝。
- 直接映射一致、单次访问、空 Trace、独立冷缓存、三种对照结果与 CSV 精确核对。
- 参数溢出、参数冲突、文件错误、CSV/文本报告拒绝覆盖。
- 原有配置、映射、解析、写分配统计和时间戳溢出保护测试继续通过。
- 独立 LRU 列表模型与 FIFO 队列模型各核对 1～4 路共 4000 次访问。FIFO 参考模型不使用时间戳；同时验证 `hits + misses == accesses`、`evictions <= misses` 和每次访问时钟仅加一。

在项目目录、已加载 MSVC 环境的终端中复现：

```bat
cmake -S . -B build-v11 -G "NMake Makefiles" -DSTRICT_WARNINGS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-v11
ctest --test-dir build-v11 --output-on-failure
```

构建目录被忽略，不纳入提交。测试在测试工作目录内创建并清理临时文件。

## 当前限制与改进方向

- 每条记录只代表对一个地址所在块的一次访问，不包含访问长度和跨块拆分。
- 只模拟一个 Cache 层级，初始为空；不保存数据，不模拟脏位、写回、时延、一致性和地址转换。
- 所有缺失统一计为 Miss，尚未区分冷、容量与冲突缺失；报告不包含逐条记录，终端包含。
- 最多 1,048,576 行；最多处理 UINT64_MAX 次有效访问，达到时钟上限后明确停止，避免 LRU/FIFO 时间戳回绕。
- 库接口要求先初始化、用后销毁；重新初始化前需先销毁。直接调用 `cache_access` 时调用方须保证时钟尚未到上限，文件处理函数已经检查。
- 对照模式需要可写的系统临时目录及容纳 Trace 快照的磁盘空间；两次扫描共享快照，逐行解析，内存不随 Trace 大小增长。

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
10. **如何升级？** v1.1 已实现 FIFO 与对照实验；下一步先解释已有实验与局限，再考虑新的模拟维度。
