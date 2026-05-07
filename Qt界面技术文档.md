# Qt 界面技术文档

本文档记录 `auto_cad` 项目第一阶段 Qt 界面的实现方案、代码结构、交互逻辑、构建方式，以及开发过程中遇到的主要错误和解决方法。

当前阶段目标不是重写 CAD、Excel、DXF 业务逻辑，而是在 VS2022 中新增一个 Qt Widgets 图形界面，用界面调度现有 `draw_cad.exe` 完成 CAD 生成工作流，并预留标书 Word 框架生成和 AI 内容生成的界面入口。

更新时间：2026-05-07

## 1. 第一阶段总体方案

第一阶段采用“GUI 壳 + 现有控制台后端”的方案。当前已接入后端的是 CAD 生成流程：

```text
auto_cad_gui.exe
    |
    | QProcess 顺序调用
    v
draw_cad.exe
    |
    | 现有业务模块
    v
symbols.json
{数据表名称}_board_lengths.json
{数据表名称}_disease_report.json
{数据表名称}.dxf
batch_output\warnings_errors.txt
```

这样做的原因：

1. `draw_cad` 已经具备稳定的命令行入口。
2. Qt 界面只负责外观病害总表选择、衬砌长度/板长数据源选择、流程调度、进度展示、日志弹窗和结果提示。
3. DXF、Excel、JSON、病害绘制等核心业务模块不需要大规模改动，风险低。
4. 标书 Word 和 AI 功能先完成界面原型落地，后续再接入真实解析、Word 生成和 AI 调用。
5. 后续如果需要更细粒度进度或数据预览，再进入第二阶段，把业务代码抽成 `draw_cad_core` 静态库。

## 2. 解决方案结构

当前解决方案目录：

```text
D:\vs2022 code\auto_cad\
├─ draw_cad\
│  ├─ main.cpp
│  ├─ BoardLengthJsonExporter.*
│  ├─ DiseaseReportJsonExporter.*
│  ├─ LiningPlanDxfWriter.*
│  └─ ...
├─ auto_cad_gui\
│  ├─ main.cpp
│  ├─ AutoCadGui.h
│  ├─ AutoCadGui.cpp
│  ├─ AutoCadController.h
│  ├─ AutoCadController.cpp
│  ├─ auto_cad_gui.vcxproj
│  └─ auto_cad_gui.vcxproj.filters
├─ draw_cad.sln
├─ Drawing1.dxf
├─ 2025年鹤大隧道外观整理--本溪原始版本.xlsx
├─ 板长分布表\
├─ batch_output\
└─ Qt界面技术文档.md
```

`draw_cad.sln` 中包含两个项目：

```text
draw_cad        控制台后端
auto_cad_gui    Qt Widgets 界面
```

`auto_cad_gui` 依赖 `draw_cad`，解决方案构建时应先生成后端程序，再生成 GUI。

## 3. Qt 界面当前形态

当前界面参考“报告转 CAD 工作台”的工作台风格，不再是传统表单堆叠界面。

主窗口由“左侧模块侧栏 + 顶部工作区标题栏 + 内容页栈”组成：

```text
AppShell
├─ Sidebar
│  ├─ 品牌区
│  ├─ CAD 生成 / 标书生成 / 文件库 / AI 设置
│  └─ 本地批量生成说明
└─ Workspace
   ├─ HeaderBar
   │  ├─ pageTitleLabel_
   │  ├─ pageSubtitleLabel_
   │  └─ 搜索 / 通知 / 用户头像
   └─ contentStack_
      ├─ CAD 生成页面
      ├─ 标书生成页面
      └─ AI 设置页面
```

页面切换由 `AutoCadGui::setActiveModule()` 控制。切换页面时会同步更新：

- 侧栏导航按钮的激活样式。
- 页面标题 `pageTitleLabel_`。
- 页面说明 `pageSubtitleLabel_`。
- `contentStack_` 当前页。

### 3.1 侧栏与工作区标题栏

侧栏和工作区标题栏在 `AutoCadGui::setupUi()` 中创建：

- 左侧侧栏：`ReportCAD` 品牌、模块导航和本地批量生成说明。
- 右侧工作区标题栏：页面标题、页面说明、搜索框、通知按钮、用户头像。

主要对象名：

```text
AppShell
Sidebar
Workspace
HeaderBar
BrandIcon
BrandText
ActiveNavButton
NavButton
SidebarStatus
SearchEdit
NotificationButton
UserAvatar
```

对应样式集中在 `AutoCadGui::applyStyles()` 中，用 QSS 统一控制。

当前导航状态：

```text
CAD 生成     已接入批量 DXF 后端流程
标书生成     已完成界面和基础文件选择交互
文件库       仅保留导航占位，暂未绑定页面
AI 设置      已完成界面布局，暂未接入真实 AI 调用
```

### 3.2 页面栈结构

主内容区使用：

```cpp
QStackedWidget* contentStack_;
```

对应页面创建函数：

```cpp
QWidget* createTenderGenerationPage();
QWidget* createAiSettingsPage();
```

CAD 生成页仍在 `setupUi()` 中沿用原有布局创建。标书生成页和 AI 设置页作为独立函数创建后加入 `contentStack_`：

```cpp
contentStack_->addWidget(cadPage);
contentStack_->addWidget(createTenderGenerationPage());
contentStack_->addWidget(createAiSettingsPage());
```

页面索引约定：

```text
0 -> CAD 生成
1 -> 标书生成
2 -> AI 设置
```

### 3.3 CAD 本次任务文件上传区域

CAD 生成页已经从旧的单文件 DXF 链路改成批量工作流。当前输入不再是“符号 DXF + 板长表 + 病害报告 + 单个输出 DXF”，而是：

```text
本次任务外观病害总表 Excel
衬砌长度/板长基础数据源目录
批量输出目录
```

实现位置：

```text
auto_cad_gui/AutoCadGui.cpp
AutoCadGui::setupUi()
AutoCadGui::chooseProjectFiles()
AutoCadGui::refreshUploadStatus()
AutoCadGui::addSelectedFileRow()
AutoCadGui::clearProjectFile()
```

上传区域使用 `QStackedWidget` 做两种状态切换：

```text
uploadStack_
├─ 第 0 页：初始上传提示页
└─ 第 1 页：已上传文件列表页
```

#### 初始上传页

未选择文件时显示：

```text
上传图标
拖拽外观病害 Excel 到此处，或者 点击上传
支持格式: .xlsx, .xlsm 多数据表外观病害总表
```

实现上，初始上传页本身是一个 `QPushButton`：

```cpp
auto* uploadPromptPage = new QPushButton(uploadStack_);
uploadPromptPage->setObjectName("UploadPromptPage");
connect(uploadPromptPage, &QPushButton::clicked, this, &AutoCadGui::chooseProjectFiles);
```

这样用户点击上传区域内任意位置，都能打开文件选择窗口。

#### 已上传文件列表页

选择文件后，初始提示页会隐藏，界面只显示本次任务文件：

```text
本次任务文件    外观病害 Excel 是本次生成任务的主输入

外观病害    2025年鹤大隧道外观整理--本溪原始版本.xlsx    ×
```

每个文件行由 `addSelectedFileRow()` 动态创建：

```cpp
addSelectedFileRow("外观病害", diseaseWorkbookEdit_);
```

删除按钮调用：

```cpp
clearProjectFile(lineEdit);
```

删除逻辑：

1. 清空 `diseaseWorkbookEdit_`。
2. 调用 `refreshUploadStatus()` 重建文件列表。
3. 如果病害总表为空，则 `uploadStack_` 自动切回初始上传页。

关键判断：

```cpp
uploadStack_->setCurrentIndex(selectedFilesLayout_->count() > 0 ? 1 : 0);
```

### 3.4 CAD 批量输入字段

当前 CAD 生成页使用两个隐藏输入控件保存批量工作流路径：

```cpp
QLineEdit* diseaseWorkbookEdit_;
QLineEdit* boardLengthSourceEdit_;
```

含义：

```text
diseaseWorkbookEdit_      批量病害总表 Excel
boardLengthSourceEdit_    衬砌长度/板长基础数据源目录
```

上传病害总表时调用：

```cpp
QFileDialog::getOpenFileName(...)
```

支持格式：

```text
Excel Workbooks (*.xlsx *.xlsm)
All Files (*)
```

板长目录通过“基础数据源”卡片选择，不再从上传文件名中自动识别，也不和外观病害 Excel 混在同一个上传列表中。

### 3.5 CAD 衬砌长度/板长基础数据源

衬砌长度相关 Excel 属于参考数据源，不是本次任务上传文件。CAD 生成页在“基础数据源与输出配置”区域使用独立数据源卡片展示：

```text
衬砌长度/板长数据源        默认 / 自定义 / 异常
基础数据源用于匹配病害数据表，不作为本次任务上传文件。

当前使用    板长分布表
已识别 80 个 Excel 板长表，生成时将按病害数据表名称自动匹配。

[更换数据源] [恢复默认]
```

相关函数和控件：

```text
createBoardLengthDataSourceCard()
chooseBoardLengthSource()
resetBoardLengthSource()
refreshBoardLengthSourceStatus()

boardLengthSourceNameLabel_
boardLengthSourceMetaLabel_
boardLengthSourceStatusLabel_
```

交互逻辑：

1. “更换数据源”调用 `QFileDialog::getExistingDirectory()`，只选择目录。
2. “恢复默认”把 `boardLengthSourceEdit_` 还原为 `defaultPath("板长分布表")`。
3. `refreshBoardLengthSourceStatus()` 会检查目录是否存在，并统计 `.xlsx/.xlsm/.xls` 数量。
4. 后端输入契约不变，`collectInput()` 仍把 `boardLengthSourceEdit_` 写入 `WorkflowInput::boardLengthSource`。
5. CAD 生成页放入 `ContentScroll` 可滚动工作区，数据源卡片和输出目录卡片使用固定高度并排显示，避免在较低窗口高度下与上传区域或底部操作按钮发生重叠。

### 3.6 默认输入输出策略

当前版本会按业务技术文档预填入默认路径：

```cpp
diseaseWorkbookEdit_->setText(defaultPath("2025年鹤大隧道外观整理--本溪原始版本.xlsx"));
boardLengthSourceEdit_->setText(defaultPath("板长分布表"));
outputDirEdit_->setText(defaultPath("batch_output"));
```

默认输入输出：

```text
批量病害总表：D:\vs2022 code\auto_cad\2025年鹤大隧道外观整理--本溪原始版本.xlsx
板长目录：    D:\vs2022 code\auto_cad\板长分布表
输出目录：    D:\vs2022 code\auto_cad\batch_output
```

说明：

- 默认会直接显示病害总表已选择。
- 用户可以删除后重新选择其它病害总表。
- 单个 `lining_plan.dxf` 输出路径已经删除。
- 批量输出文件由后端按数据表名称自动命名。

### 3.7 CAD 批量匹配与输出配置区域

输出配置区当前包含：

```text
衬砌长度/板长基础数据源卡片
输出目录
```

界面布局为：

```text
ContentScroll / CadDashboard
├─ GeneratorCard
│  ├─ HeroPanel
│  ├─ 本次任务文件 TaskFilePanel
│  ├─ 基础数据源卡片        输出目录卡片
│  └─ 开始生成 / 取消 / 打开输出目录
└─ QueueCard
```

创建函数：

```cpp
QHBoxLayout* AutoCadGui::createPathRow(...)
QWidget* AutoCadGui::createOutputDirectoryCard()
```

功能：

- 板长数据源卡片使用 `QFileDialog::getExistingDirectory()` 更换目录。
- 板长数据源卡片支持一键恢复默认 `板长分布表`。
- 板长数据源卡片显示当前目录识别到的 Excel 数量。
- 输出目录使用 `QFileDialog::getExistingDirectory()`。
- 输出目录默认为 `batch_output`。
- 不再提供单个 `输出 DXF` 输入框。

### 3.8 CAD 生成任务队列

右侧任务队列用于展示运行状态：

```text
生成任务队列
4 个任务

当前批量任务
生成进度
输出摘要
步骤状态标签
```

四个步骤：

```text
提取病害符号
识别病害数据表
匹配基础数据源
批量生成 DXF
```

步骤标签通过属性 `state` 控制样式：

```cpp
label->setProperty("state", "pending");
label->setProperty("state", "running");
label->setProperty("state", "done");
```

状态刷新函数：

```cpp
AutoCadGui::updateStepState()
AutoCadGui::setStepLabelState()
```

### 3.9 标书生成页面

标书生成页面由 `AutoCadGui::createTenderGenerationPage()` 创建。

页面标题：

```text
标书 Word 框架生成
上传招标文件，自动解析章节、评分项与格式要求，生成可编辑的 Word 标书框架。
```

该页面当前包含三列模块：

```text
1 上传招标文件
2 选择 Word 框架规则
3 预览生成结构
```

#### 3.9.1 上传招标文件

上传模块使用独立的：

```cpp
QStackedWidget* tenderUploadStack_;
QLineEdit* tenderFileEdit_;
QLabel* tenderFileNameLabel_;
QLabel* tenderFileMetaLabel_;
```

`tenderUploadStack_` 有两种状态：

```text
第 0 页：上传提示页
第 1 页：已选择文件页
```

上传提示页本身是 `QPushButton`，用户点击区域内任意位置都会打开文件选择窗口：

```cpp
connect(uploadPrompt, &QPushButton::clicked, this, &AutoCadGui::chooseTenderFile);
```

支持格式：

```text
*.pdf
*.docx
*.doc
*.txt
```

选择文件后，界面显示：

```text
文件类型标签
招标文件名
文件大小
待解析章节和格式要求
删除按钮
重新选择招标文件按钮
```

删除按钮会清空 `tenderFileEdit_`，再调用：

```cpp
refreshTenderFileStatus();
```

当招标文件路径为空时，页面自动回到初始上传提示页。

#### 3.9.2 选择 Word 框架规则

第二列用于配置 Word 框架生成规则。

当前展示的规则项：

```text
章节结构      根据招标文件目录自动生成一级/二级/三级标题
封面与目录    创建封面、自动目录、页眉页脚占位
格式规范      宋体/黑体、字号、行距、页边距
评分响应点    把评分办法映射到技术响应章节
AI 内容占位   每个章节保留 AI 生成内容入口
```

每一项由 `createOptionRow()` 创建，左侧使用蓝色勾选圆点表示已启用。

底部按钮：

```text
生成 Word 框架
保存配置方案
```

当前按钮只完成界面占位，尚未接入真实 Word 生成逻辑。

#### 3.9.3 预览生成结构

第三列用于预览将要生成的 Word 文档骨架。

对象名：

```text
DocumentPreview
OutlineLine
OutlineIndex
OutlineTitle
OutlineTag
OutlineTagAi
```

当前预览结构：

```text
01 封面                 格式
02 投标函               AI
03 项目理解与总体方案     AI
04 施工组织设计           AI
05 质量与安全保障措施     AI
06 主要设备材料响应表     表格
07 评分办法响应索引       AI
08 商务偏离表             表格
09 报价文件占位           格式
```

标签含义：

```text
AI     后续可由 AI 自动生成内容
表格   后续应生成 Word 表格或表格占位
格式   后续应应用 Word 格式模板
```

该模块目前是静态预览，用于展示目标 Word 框架结构；后续接入招标文件解析后，应由解析结果动态生成。

### 3.10 AI 设置页面

AI 设置页面由 `AutoCadGui::createAiSettingsPage()` 创建。

页面标题：

```text
AI 设置与提示词管理
配置 AI API Key、模型参数和自定义提示词，生成内容可应用到标书 Word 框架中的章节。
```

该页面当前包含三列模块：

```text
1 API Key 与模型
2 自定义提示词
3 应用到 Word 框架
```

#### 3.10.1 API Key 与模型

第一列用于配置 AI 接口参数。

字段：

```text
服务商
API Base URL
API Key
默认模型
```

输入框由 `createSettingsInput()` 创建。`API Key` 输入框使用：

```cpp
input->setEchoMode(QLineEdit::Password);
```

底部按钮：

```text
测试连接
保存设置
```

当前状态提示：

```text
连接状态：待测试
测试成功后才能启用 AI 内容生成。
```

当前按钮只完成界面占位，尚未保存到配置文件，也未真实调用 AI 接口。

#### 3.10.2 自定义提示词

第二列用于维护提示词模板。

当前标签页：

```text
技术方案
商务响应
评分响应
```

提示词编辑区使用：

```cpp
QTextEdit* promptEditor;
promptEditor->setObjectName("PromptEditor");
```

默认提示词：

```text
你是资深投标文件编制专家。
请基于 {招标文件摘要}、{章节名称}、{评分标准}
生成符合招标要求的技术响应内容。
要求：结构清晰、避免夸大、可直接写入 Word。
```

可用变量：

```text
{招标文件摘要}
{章节名称}
{评分标准}
{项目类型}
```

底部按钮：

```text
预览生成内容
应用到标书章节
```

当前按钮只完成界面占位，尚未接入真实内容生成。

#### 3.10.3 应用到 Word 框架

第三列用于说明 AI 内容应用流程。

流程步骤由 `createFlowStep()` 创建：

```text
1 读取招标文件摘要
2 选择 Word 章节
3 套用提示词生成内容
4 进入候选内容区
5 写入 Word 框架
```

其中第 3 步使用蓝色圆点表示当前核心步骤。

底部候选内容预览：

```text
本章节将从项目理解、施工组织、质量安全、进度保障四个方面展开...
```

当前候选内容仍为静态示例，后续应由 AI 返回结果填充。

## 4. 日志弹窗设计

当前主界面没有日志区域。

运行时点击“开始生成”，会弹出独立日志窗口：

```text
运行日志
状态文本
进度条
日志文本框
关闭按钮
```

实现函数：

```cpp
AutoCadGui::ensureLogDialog()
AutoCadGui::setLogDialogRunning()
AutoCadGui::setLogDialogFinished()
AutoCadGui::appendLog()
```

日志窗口是非模态 `QDialog`：

```cpp
logDialog_ = new QDialog(this);
logDialog_->setModal(false);
```

这样用户可以一边查看日志，一边观察主界面的任务状态。

日志文本框：

```cpp
logEdit_ = new QTextEdit(logDialog_);
logEdit_->setReadOnly(true);
logEdit_->setLineWrapMode(QTextEdit::NoWrap);
logEdit_->document()->setMaximumBlockCount(3000);
```

日志颜色规则：

1. `error:`、`stderr:`、`failed` 使用红色。
2. `warning:` 使用黄色。
3. 普通日志使用浅色。

## 5. AutoCadController 工作流

文件：

```text
auto_cad_gui/AutoCadController.h
auto_cad_gui/AutoCadController.cpp
```

职责：

1. 校验输入路径。
2. 创建输出目录。
3. 组装后端命令。
4. 使用 `QProcess` 顺序运行 `draw_cad.exe`。
5. 捕获标准输出和标准错误。
6. 通过 Qt signal 通知界面更新日志、进度和最终状态。

输入结构：

```cpp
struct WorkflowInput {
    QString exePath;
    QString diseaseWorkbook;
    QString boardLengthSource;
    QString outputDir;
};
```

字段含义：

```text
exePath             draw_cad.exe 路径
diseaseWorkbook     批量病害总表 Excel
boardLengthSource   板长分布表目录
outputDir           批量输出目录
```

关键原则：

```text
不要拼接整条命令字符串。
使用 QProcess::setProgram() + QProcess::setArguments()。
```

原因：

- 能正确处理中文路径。
- 能正确处理路径中的空格。
- 避免命令行转义错误。

## 6. GUI 调用后端流程

当前 Qt 界面调用新的批量后端入口：

```text
draw_cad.exe --batch-workflow <病害总表 Excel> <板长目录> <输出目录>
```

`AutoCadController::buildCommands()` 当前只组装一个后端命令：

```cpp
commands_.push_back({
    "批量生成衬砌平面图 DXF",
    {"--batch-workflow", input.diseaseWorkbook, input.boardLengthSource, input.outputDir}
});
```

后端内部会完成：

```text
1. 提取 Drawing1.dxf 中的病害符号
2. 生成 batch_output\symbols.json
3. 识别病害总表内所有病害数据表
4. 每个数据表导出 {数据表名称}_disease_report.json
5. 按数据表名称匹配板长目录中的对应 Excel
6. 每个数据表导出 {数据表名称}_board_lengths.json
7. 每个数据表生成 {数据表名称}.dxf
8. 写出 warnings_errors.txt
```

输出目录内的文件命名规则：

```text
symbols.json
{数据表名称}_disease_report.json
{数据表名称}_board_lengths.json
{数据表名称}.dxf
warnings_errors.txt
```

### 6.1 为什么改为 `--batch-workflow`

旧 GUI 曾经显式调用四步：

```text
--symbols
--board-lengths
--disease-report
--lining-plan
```

该链路只适合“一个板长表 + 一个病害报告 + 一个输出 DXF”的旧流程。

当前业务已经变为批量流程：

```text
一个病害总表内有多个病害数据表
每个数据表匹配一个板长 Excel
每个数据表生成一个 DXF
```

因此 GUI 不再逐步拼四个命令，而是把批量输入交给后端 `--batch-workflow`，由后端统一完成数据表识别、板长匹配、中间 JSON 和多个 DXF 输出。

### 6.2 标书和 AI 页面当前状态

当前 `标书生成` 和 `AI 设置` 页面只完成界面层：

```text
标书生成
├─ 招标文件选择
├─ 解析结果静态展示
├─ Word 框架规则静态配置
└─ Word 结构静态预览

AI 设置
├─ API 参数输入框
├─ 提示词编辑区
├─ 可用变量展示
└─ AI 应用流程静态展示
```

尚未完成：

1. 招标文件解析。
2. Word 文档生成。
3. API Key 本地持久化。
4. AI 接口真实调用。
5. AI 内容写入 Word 框架。

后续实现这些功能时，应新增独立业务控制器，不要把所有逻辑继续塞进 `AutoCadGui.cpp`。

## 7. 后端程序自动定位

界面中不显示“后端程序”路径，但内部仍然需要找到 `draw_cad.exe`。

当前查找逻辑：

1. 从 `auto_cad_gui.exe` 所在目录开始向上查找项目根目录。
2. 判断依据：
   - 是否存在 `draw_cad.sln`
   - 或是否存在 `Drawing1.dxf`
3. 按候选路径查找后端：

```text
auto_cad_gui.exe 同目录\draw_cad.exe
项目根目录\x64\Debug\draw_cad.exe
项目根目录\draw_cad\x64\Debug\draw_cad.exe
项目根目录\x64\Release\draw_cad.exe
项目根目录\draw_cad\x64\Release\draw_cad.exe
```

如果都找不到，则默认使用：

```text
项目根目录\draw_cad\x64\Debug\draw_cad.exe
```

该路径不会显示在界面上，只用于内部校验和运行。

## 8. UI 样式方案

当前界面没有引入第三方 UI 库，主要原因是：

1. 第一阶段优先保证 VS2022 + Qt Widgets 工程稳定。
2. 第三方 UI 库会引入额外编译配置和部署成本。
3. 当前视觉效果可以先用 Qt Widgets + QSS 实现。

样式集中在：

```cpp
AutoCadGui::applyStyles()
```

主要样式对象：

```text
TopBar
ContentStack
ModulePage
ModuleCard
GeneratorCard
QueueCard
UploadBox
UploadPromptPage
SelectedFileRow
TenderUploadStack
TenderUploadPrompt
UploadedTenderFile
OptionRow
DocumentPreview
OutlineLine
OutlineTag
OutlineTagAi
SettingsInput
PromptEditor
VariableChip
FlowStep
CandidateBox
PrimaryButton
SecondaryButton
RunLog
LogDialog
```

CAD 批量页相关对象：

```text
UploadBox
UploadPromptPage
SelectedFilesPage
SelectedFileRow
SelectedFileType
SelectedFileName
RemoveFileButton
PathEdit
IconButton
StepPill
```

新增通用构造函数：

```cpp
createModuleCard()
createInfoRow()
createOptionRow()
createFlowStep()
createSettingsInput()
createBodyText()
```

这些函数用于减少标书页面和 AI 设置页面中的重复 UI 代码。

已调研的第三方 Qt Widgets UI 库：

1. `ElaWidgetTools`：Fluent 风格，适合后续真正升级整套控件。
2. `Qlementine`：现代 `QStyle`，适合做全局视觉皮肤。
3. `QSkinny`：更像完整 UI 框架，改造成本更高。

如果下一阶段要引入第三方库，建议优先评估 `ElaWidgetTools`。

## 9. 构建配置

### 9.1 Qt 项目配置

文件：

```text
auto_cad_gui/auto_cad_gui.vcxproj
```

关键配置：

```xml
<Keyword>QtVS_v304</Keyword>
<QtModules>core;gui;widgets</QtModules>
<SubSystem>Windows</SubSystem>
<LanguageStandard>stdcpp20</LanguageStandard>
```

当前 Qt 安装路径：

```xml
<QtInstall>D:\Qt\6.10.1\msvc2022_64</QtInstall>
```

当前额外 qmake 参数：

```xml
<QMakeExtraArgs>QMAKE_MSC_VER=1944;QMAKE_MSC_FULL_VER=194435225</QMakeExtraArgs>
```

当前额外编译参数：

```xml
<AdditionalOptions>/utf-8 /Zc:__cplusplus %(AdditionalOptions)</AdditionalOptions>
```

### 9.2 推荐构建命令

由于当前环境可能同时存在 `PATH` 和 `Path` 两个环境变量，命令行构建前需要规范化环境变量：

```powershell
& cmd.exe /c '"D:\vs2022\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && powershell -NoProfile -Command "$pathValue = $env:Path; [Environment]::SetEnvironmentVariable(''PATH'', $null, ''Process''); [Environment]::SetEnvironmentVariable(''Path'', $pathValue, ''Process''); & ''D:\vs2022\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'' ''auto_cad_gui\auto_cad_gui.vcxproj'' /t:Build /p:Configuration=Debug /p:Platform=x64 /v:minimal"'
```

如果在 VS2022 中操作：

```text
右键 auto_cad_gui
  -> Set as Startup Project
  -> 本地 Windows 调试器
```

## 10. 已验证输出

单独构建 GUI 项目时输出：

```text
D:\vs2022 code\auto_cad\auto_cad_gui\x64\Debug\auto_cad_gui.exe
```

解决方案整体构建时可能输出到：

```text
D:\vs2022 code\auto_cad\x64\Debug\auto_cad_gui.exe
D:\vs2022 code\auto_cad\x64\Debug\draw_cad.exe
```

当前最近一次验证时间：2026-05-06。

```text
auto_cad_gui.vcxproj -> D:\vs2022 code\auto_cad\auto_cad_gui\x64\Debug\auto_cad_gui.exe
```

## 11. 关键错误与解决方法

### 11.1 中文路径传给后端后乱码

现象：

```text
BoardLengthJsonExporter failed(60): Failed to export board length json:
file open failed (m_ArchivePath: D:/vs2022 code/auto_cad/�峤�ֲ���.xlsx)
```

原因：

- Qt 的 `QProcess` 传参本身没有问题。
- 问题出在 Windows 下 `draw_cad` 的入口是：

```cpp
int main(int argc, char* argv[]);
```

- `char* argv[]` 在 Windows 下不是可靠的 UTF-8 参数来源。
- 中文路径进入后端后变成乱码，OpenXLSX 打开错误路径，最终报 `file open failed`。

解决方法：

Windows 下改用 Unicode 命令行：

```cpp
GetCommandLineW()
CommandLineToArgvW()
WideCharToMultiByte(CP_UTF8, ...)
```

修复位置：

```text
draw_cad/main.cpp
```

修复后验证：

```powershell
& 'D:\vs2022 code\auto_cad\x64\Debug\draw_cad.exe' `
  --board-lengths `
  'D:\vs2022 code\auto_cad\板长分布表\新开岭隧道上行.xlsx' `
  'D:\vs2022 code\auto_cad\board_lengths.json'
```

成功输出：

```text
Board length JSON generated
exported rows: 118
```

### 11.2 `shellapi.h` 编译错误

现象：

```text
shellapi.h error C4430
shellapi.h error C2146
CommandLineToArgvW: 找不到标识符
```

原因：

`shellapi.h` 依赖 `windows.h` 中定义的基础 Windows 类型。

错误顺序：

```cpp
#include <shellapi.h>
#include <windows.h>
```

正确顺序：

```cpp
#include <windows.h>
#include <shellapi.h>
```

### 11.3 Qt 项目提示没有分配 Qt 版本

现象：

```text
There's no Qt version assigned to project auto_cad_gui.vcxproj for configuration Debug/x64.
```

原因：

Qt VS Tools 的注册表版本信息在命令行环境中没有被正确读取。

解决方法：

在 `auto_cad_gui.vcxproj` 中直接配置当前机器实际存在的 Qt 路径：

```xml
<QtInstall>D:\Qt\6.10.1\msvc2022_64</QtInstall>
```

### 11.4 qmake 报 `QMAKE_MSC_VER isn't set`

现象：

```text
Variable QMAKE_CXX.COMPILER_MACROS is not defined.
Project ERROR: msvc-version.conf loaded but QMAKE_MSC_VER isn't set
```

原因：

QtMsBuild 调用 qmake 读取 Qt 配置时，当前环境没有让 qmake 正确推断 MSVC 编译器版本。

当前 VS2022 编译器版本：

```text
cl 19.44.35225
```

对应：

```text
QMAKE_MSC_VER=1944
QMAKE_MSC_FULL_VER=194435225
```

解决方法：

```xml
<QMakeExtraArgs>QMAKE_MSC_VER=1944;QMAKE_MSC_FULL_VER=194435225</QMakeExtraArgs>
```

注意：

如果以后升级 VS/MSVC，可能需要同步调整这两个值。

### 11.5 Qt 编译要求 `/Zc:__cplusplus`

现象：

```text
Qt requires a C++17 compiler, and a suitable value for __cplusplus.
On MSVC, you must pass the /Zc:__cplusplus option to the compiler.
```

原因：

MSVC 默认 `__cplusplus` 宏值不反映真实 C++ 标准版本。

解决方法：

```xml
<AdditionalOptions>/utf-8 /Zc:__cplusplus %(AdditionalOptions)</AdditionalOptions>
```

其中：

- `/utf-8` 保证源码中的中文按 UTF-8 编译。
- `/Zc:__cplusplus` 保证 Qt 能正确识别 C++ 标准版本。

### 11.6 `PATH` 和 `Path` 重复导致 MSBuild 失败

现象：

```text
error MSB6001: “CL.exe”的命令行开关无效。
System.ArgumentException: 已添加项。字典中的关键字:“PATH”所添加的关键字:“Path”
```

原因：

Windows 环境变量大小写不敏感，但当前进程里同时存在 `PATH` 和 `Path`。

解决方法：

```powershell
$pathValue = $env:Path
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $pathValue, 'Process')
```

### 11.7 头文件缺少 Qt 类前置声明

现象：

```text
AutoCadGui.h error C2143: 缺少“;”
QHBoxLayout 找不到
QLayout 找不到
```

原因：

`AutoCadGui.h` 中声明了返回类型和参数：

```cpp
QHBoxLayout*
QLayout*
```

但头文件没有声明对应 Qt 类型。

解决方法：

```cpp
class QHBoxLayout;
class QLayout;
class QStackedWidget;
```

### 11.8 文字后面出现灰色背景

现象：

步骤标题等普通文本后面出现灰色底色。

原因：

全局 `QWidget` 样式设置了背景色，`QLabel` 也继承了背景绘制效果。

解决方法：

在 QSS 中增加：

```css
QLabel {
    background: transparent;
}
```

注意：

有明确对象名的标签仍然可以单独设置背景，例如：

```text
StepNumber
FeatureIcon
TaskCount
UserAvatar
```

### 11.9 主界面日志区域不符合当前 UI

早期版本把日志区放在主界面下方，并使用 `QSplitter` 调整大小。

当前版本已改为：

```text
主界面不显示日志
运行时弹出独立日志窗口
```

原因：

- 主界面更像任务工作台。
- 日志属于运行过程细节，不应长期占据主界面空间。
- 弹窗可在需要时查看，关闭后不影响任务继续运行。

### 11.10 上传后继续显示“拖拽文件”提示不好看

现象：

选择文件后，上传提示仍然保留，只在底部追加文件名，视觉上混乱。

解决方法：

用 `QStackedWidget` 拆成两页：

```text
未选择文件 -> 初始上传提示页
已选择文件 -> 文件列表页
```

文件全部删除后，自动回到初始上传提示页。

### 11.11 只有点击文字才能上传

现象：

上传区域很大，但只有文字按钮能打开文件选择。

解决方法：

将初始上传页本身改为 `QPushButton`，内部放图标和普通 `QLabel`：

```cpp
auto* uploadPromptPage = new QPushButton(uploadStack_);
connect(uploadPromptPage, &QPushButton::clicked, this, &AutoCadGui::chooseProjectFiles);
```

这样点击整个上传区域都能上传。

## 12. 当前运行注意事项

1. CAD 生成页负责调度 `draw_cad.exe --batch-workflow`，不直接解析 Excel 或 DXF。
2. CAD 页默认填入批量病害总表、板长目录和 `batch_output` 输出目录。
3. 用户可以删除或重选病害总表，也可以修改板长目录和输出目录。
4. 如果用户选择了不同输出目录，中间 JSON、多个 DXF 和 `warnings_errors.txt` 会写到该输出目录。
5. 后端命令输出按 UTF-8 读取：

```cpp
QString::fromUtf8(...)
```

6. 后端 `draw_cad` 已经设置控制台 UTF-8：

```cpp
SetConsoleOutputCP(CP_UTF8);
SetConsoleCP(CP_UTF8);
std::setlocale(LC_ALL, ".UTF-8");
```

7. 板长目录中“人行横洞 / 车行横洞”的 warning 是业务校验提示，不阻断 DXF 生成。
8. 如果点击开始生成时缺少病害总表、板长目录或输出目录，`AutoCadController::validateInput()` 会阻止运行，并在日志弹窗里显示错误。
9. 标书生成页当前只完成招标文件路径选择和静态预览，不会真正解析招标文件。
10. AI 设置页当前只完成参数输入和提示词编辑界面，不会保存 API Key，也不会发起网络请求。
11. 文件库导航当前是占位按钮，尚未绑定页面。

## 13. 后续维护建议

### 13.1 第一阶段继续增强方向

仍保持 CAD 生成调用 `draw_cad.exe` 的前提下，可以继续增强：

1. 支持真正的拖拽文件到上传区域。
2. 保存最近一次输入路径到本地配置文件。
3. 运行前检查病害总表扩展名、板长目录是否包含可匹配 Excel。
4. 将 warning 单独汇总显示。
5. 增加“打开批量输出目录”和“打开 warning/error 日志”按钮。
6. 文件列表中显示病害总表大小和最后修改时间。
7. 增加任务完成后的桌面通知。
8. 新增招标文件解析控制器，输出结构化章节、评分项和格式要求。
9. 新增 Word 文档生成模块，生成 `.docx` 框架。
10. 新增 AI 配置保存模块，建议先本地加密或至少本机配置文件保存。
11. 新增 AI 调用控制器，将提示词变量替换后生成候选内容。
12. 将 AI 候选内容写入 Word 框架前，增加用户确认和编辑步骤。

### 13.2 第二阶段重构方向

当 GUI 流程稳定后，可以考虑拆分核心库：

```text
draw_cad_core      静态库，包含业务模块
draw_cad_cli       命令行入口
auto_cad_gui       Qt 图形界面
word_tender_core   招标文件解析和 Word 生成模块
ai_content_core    AI 配置、提示词和内容生成模块
```

目标结构：

```cpp
class WorkflowRunner {
public:
    WorkflowResult runBatchWorkflow(const WorkflowOptions& options);
    StepResult exportSymbols(...);
    StepResult discoverDiseaseSheets(...);
    StepResult resolveBoardLengthWorkbook(...);
    StepResult exportBoardLengthsForSheet(...);
    StepResult exportDiseaseReportForSheet(...);
    StepResult writeLiningPlan(...);
};
```

第二阶段收益：

- Qt 可以直接获取结构化进度。
- 不需要解析控制台输出。
- 可以展示统计数据、warning 列表和数据预览。
- 可以为每一步提供更细粒度的取消和错误恢复。

第二阶段会改动工程边界和链接结构，风险高于第一阶段，应在当前 GUI 稳定后再做。
