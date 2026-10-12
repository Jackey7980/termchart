# 并发模型：单线程循环、投递、waker

## 决策：单线程主循环

渲染、布局、图表全部运行在主线程。依据：

- 终端输出是严格串行的字节流，多线程渲染没有任何收益；
- `color/colormap.hpp` 的 `stops()` 缓存**无锁**，其成立前提就是单线程主循环；
- `term::Terminal` 持有 tty 状态，不保证线程安全。

## 后台线程怎么参与：投递 + waker

后台线程（如数据采集）不直接碰终端，而是：

1. 投递数据/任务到主循环消费的队列（`util/task.hpp`、`util/scheduler.hpp`）；
2. 通过 `term/waker.hpp` 的 `Waker` 唤醒主循环的事件等待（跨平台的"踢一脚"机制，
   实现隔离在 `src/term/waker_win32.cpp` 等）；
3. 信号面（窗口尺寸变化等）收在 `term/signal.hpp`（POSIX 侧 SIGWINCH 处理）。

## 状态所有权

- 终端状态唯一持有者：`term::Terminal`（RAII，enter/leave 精确回滚）；
- 事件队列唯一消费点：主循环（`interaction::Dispatcher::dispatch` 在主线程调用）；
- 渲染器 `render::Renderer` 持有 prev 状态，同样只在主线程使用。

## 禁忌

- 后台线程直接写 stdout 或操作 `Terminal`；
- 在多线程环境调用 `colormap::stops()` 的首次构建；
- 跨线程共享 `CellBuffer` 无同步地读写（buffer 的 `std::span<Cell>` 视图
  不得跨 `resize` 保存，跨线程更是双重危险）。

## 未做

- 无公开的多线程渲染 API；
- `Waker` 的 POSIX 实现细节以头文件与 `src/term/` 为准，规格中的自管道/self-pipe
  方案未在文档层面固化。
