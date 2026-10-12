# 后台线程投递

## 模型

单线程主循环（见 design/concurrency.md）：渲染与布局只在主线程；
后台线程只做两件事——**投递数据**、**唤醒主循环**。

## 组件

| 头文件 | 角色 |
| --- | --- |
| util/task.hpp | 任务对象 |
| util/scheduler.hpp | 调度（主循环消费侧） |
| term/waker.hpp | `Waker`：跨平台唤醒主循环的事件等待 |

## 推荐模式

```
后台线程                                主线程
────────                               ────────
采集/计算数据
    │
构造任务（util/task.hpp）
    │ 入队（队列自身线程安全）
Waker::wake()  ──────────────────→   事件等待被唤醒
                                     1. 取队列任务
                                     2. 更新 Dataset / 模型
                                     3. 重画（差分输出）
```

要点：

- 后台线程**不碰** `Terminal`、`Renderer`、`CellBuffer`、stdout；
- 主循环每帧先消化队列再渲染，帧内数据一致；
- 唤醒是"踢一脚"语义：多次 wake 可合并，主循环醒来批量消化。

## 尺寸变化信号

POSIX 侧窗口尺寸变化经 `term/signal.hpp`（SIGWINCH）转成主循环事件，
最终以 `interaction::ResizeEvent` 分发——同样走"投递 + 唤醒"，
信号处理器内不做任何渲染。

## 禁忌

- 后台线程直接调用渲染/输出 API；
- 跨线程共享可变缓冲（`CellBuffer` 的 span 视图绝不过线程边界）；
- 在信号处理器里做分配或加锁。

## 未做

- 公开的多线程渲染 API；
- 队列的优先级/丢弃策略文档化（以 util/scheduler.hpp 为准）。
