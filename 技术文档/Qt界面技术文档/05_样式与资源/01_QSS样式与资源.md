# QSS 样式与资源

更新时间：2026-06-01

## 1. 样式来源

| 文件 | 说明 |
| --- | --- |
| `auto_cad_gui/AntDesignStyle.h` | 样式接口、颜色和辅助函数声明 |
| `auto_cad_gui/AntDesignStyle.cpp` | 全局 QSS、控件样式和主题工具 |
| `auto_cad_gui/AutoCadGui.cpp` | 少量页面级 `objectName` 和局部样式 |

## 2. 当前主要样式对象

| 类型 | 示例 | 用途 |
| --- | --- | --- |
| 主窗口 | `AutoCadGui` | 全局背景和字体 |
| 导航按钮 | CAD 导航、文件库导航 | 顶部导航状态 |
| 路径输入行 | 路径输入框、浏览按钮 | 文件选择 |
| 操作按钮 | 生成、停止、打开结果 | CAD 执行动作 |
| 进度控件 | `AnimatedProgressWidget` | 运行状态 |
| 日志区域 | 文本显示控件 | 后端输出 |

## 3. 资源

| 资源 | 用途 |
| --- | --- |
| SVG 图标 | 导航和按钮图标 |
| 字体 | 默认跟随系统和 Qt 配置 |
| 颜色 token | 由 `AntDesignStyle` 集中维护 |

## 4. 维护规则

1. 新样式优先集中到 `AntDesignStyle`。
2. 页面控件 `objectName` 应稳定，避免 QSS 选择器失效。
3. 删除控件后同步清理 QSS 中不再使用的选择器。
4. 当前样式只服务 CAD 前端，不再维护标书/AI 页面选择器。
