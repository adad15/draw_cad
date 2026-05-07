# auto_cad 检查表生成病害展开图技术方案

## 1. 目标

目标是把检查表中的病害记录自动转换为病害展开图 DXF，输入形态参考截图 1，输出形态参考截图 2。

本阶段只形成技术方案，不修改现有源码。

最终要解决的是一条完整链路：

1. 读取检查表数据。
2. 从“病害位置”“检查内容”“病害描述”中提取结构化病害参数。
3. 将病害记录映射到“衬砌板块号 + 展开图分区”的二维坐标。
4. 根据病害类型选择绘制策略，生成符号、裂缝线、面积云线、标注文字。
5. 输出排版完整、可在 CAD 中直接打开的 DXF。

## 2. 当前工程现状评估

结合当前仓库代码，现有能力和缺口如下。

### 2.1 已有能力

1. `draw_cad/DxfSymbolCollector.*`
   负责从 DXF 中提取 `SYM_` 前缀块，支持 `LINE / ARC / CIRCLE / LWPOLYLINE` 转内部图元。

2. `draw_cad/SymbolJsonWriter.*`
   负责把提取出的符号块归一化并写成 `symbols.json`。

3. `draw_cad/JsonSymbolDxfWriter.*`
   已经能读取 `symbols.json`，把符号写回 DXF。
   当前支持：
   - 读取 JSON 图元
   - 对 `stretch_x` 做 X 向缩放
   - 把多个符号从左到右排布输出

4. `symbols.json`
   已经有部分病害符号资产，例如：
   - `crack_long`
   - `crack_trans`
   - `crack_diag`
   - `crack_alligator`
   - `damage`
   - `seepage`
   - `rebar`

5. `libxfrw`
   当前项目已集成 libdxfrw。库本身支持 `writeText` 和 `writeMText`，因此后续可以直接输出病害标注文字，不需要更换底层 DXF 库。

### 2.2 当前缺口

当前仓库距离“检查表直接生成病害展开图”还缺以下核心能力：

1. 没有 Excel 或 CSV 读取模块。
2. 没有“病害描述”文本解析模块。
3. 没有“病害位置 -> 展开图区带”的业务映射模块。
4. 没有“板块号 -> 图纸列坐标”的分页和排版模块。
5. 现有 DXF 输出只支持线和多段线，不支持文字标注的项目封装。
6. 现有 `JsonSymbolDxfWriter` 只能顺序平铺符号，不能按任意坐标落图。
7. 现有 `stretch_x` 模型不够表达所有裂缝方向。

### 2.3 一个必须提前指出的问题

当前 `symbols.json` 中：

1. `crack_long` 是水平图形，适合做 X 向拉伸。
2. `crack_trans` 是竖向图形，但当前仍被归类为 `stretch_x`。
3. `crack_diag` 是斜向图形，也不是单纯 X 向拉伸就能准确表达长度。

这意味着：

“所有裂缝都通过符号块 + stretch_x 缩放” 这条路线不够稳。

建议后续采用分流策略：

1. 纵向裂缝、环向裂缝、斜向裂缝、露筋
   直接由代码按几何参数绘制。

2. 渗水、破损、网裂、错台、变形
   优先继续复用 `symbols.json` 符号库。

这样能显著降低复杂度，也能保证长度、角度、跨板显示更准确。

## 3. 业务目标定义

以截图 1 的表格字段为基础，至少需要使用以下列：

1. `桩号`
2. `衬砌板块号`
3. `项目名称`
4. `病害位置`
5. `检查内容`
6. `病害描述`
7. `判定结论`
8. `检测日期`

推荐的业务规则如下：

1. `项目名称`
   当前先按“衬砌”处理，后续可扩展其他项目。

2. `判定结论`
   默认仅输出需要处置或判定为病害的记录。
   建议做成配置项，例如：
   - 仅输出 `1`
   - 输出全部
   - 输出 `1` 并将 `0` 置灰

3. `病害位置`
   是展开图纵向分区的主要依据。

4. `检查内容`
   是病害类型的第一优先级来源。

5. `病害描述`
   是几何参数、修补信息、数量信息、面积信息的主要来源。

6. `衬砌板块号`
   是展开图横向列定位的主要依据。

## 4. 输出图纸目标形态

目标 DXF 需要接近截图 2 的表达方式。

### 4.1 图纸内容

1. 页眉文字
   - 隧道名，例如“新开岭”
   - 行别，例如“上行”

2. 展开图网格
   每页按固定列数排布若干板块，每个板块一个单元格。

3. 纵向分区线
   至少包含：
   - 左侧边墙底线
   - 左侧拱腰线
   - 拱顶线
   - 右侧拱腰线
   - 右侧边墙底线

4. 板块编号行
   每列底部显示板块号，例如 `001`、`014`。

5. 病害图元
   - 裂缝线
   - 露筋线
   - 渗水云线
   - 破损范围
   - 网裂范围
   - 其他病害符号

6. 标注文字
   - `L=4m`
   - `W=0.30mm`
   - `A=1.5m2`
   - `已修`
   - `2处`
   - 必要时保留原文摘要

### 4.2 版式参数建议

建议把版式做成配置，不写死在代码里。最少包括：

1. 每页列数，例如 5 列。
2. 每页图区个数。
3. 单板宽度。
4. 各分区高度。
5. 页眉高度。
6. 板块号栏高度。
7. 文字高度。
8. 相邻页或相邻图区之间的间距。

## 5. 总体技术路线

建议采用“五层结构”，把业务解析和 CAD 渲染解耦。

### 5.1 五层结构

1. 数据读取层
   负责读取 Excel 或 CSV。

2. 业务解析层
   负责把自然语言描述转成结构化病害参数。

3. 定位排版层
   负责把病害映射到某个板块的某个展开图区带、某个相对坐标。

4. 渲染组装层
   负责把结构化病害转换成可绘制图元和文字标注。

5. DXF 输出层
   负责按图层、样式、页布局写入 DXF。

### 5.2 推荐主流程

```text
检查表(xlsx/csv)
  -> InspectionTableReader
  -> DefectTextParser
  -> DefectNormalizer
  -> SlabLayoutEngine
  -> DefectRenderPlanner
  -> DiseaseLayoutDxfWriter
  -> out_defect_layout.dxf
```

## 6. 输入层设计

### 6.1 Excel 读取策略

当前工程没有 Excel 依赖库，因此建议分两阶段：

#### 方案 A：先做 CSV MVP

1. 由人工把 Excel 导出为 UTF-8 CSV。
2. 程序先支持 CSV。
3. 等业务规则稳定后，再补 `.xlsx` 直读。

优点：

1. 实施快。
2. 依赖少。
3. 调试容易。

缺点：

1. 用户多一步导出操作。

#### 方案 B：直接支持 xlsx

可选库：

1. OpenXLSX
2. xlnt
3. LibreOffice CLI 预转 CSV

建议：

先用 CSV 打通算法，再决定是否引入 xlsx 库。

### 6.2 输入配置

建议增加一个输入配置文件，例如 `layout_config.json`，内容至少包含：

```json
{
  "input": {
    "format": "csv",
    "path": "data/inspection.csv",
    "encoding": "utf-8"
  },
  "sheet_mapping": {
    "stake_no": "桩号",
    "slab_no": "衬砌板块号",
    "project_name": "项目名称",
    "defect_location": "病害位置",
    "check_item": "检查内容",
    "defect_desc": "病害描述",
    "judgement": "判定结论",
    "inspect_date": "检测日期"
  }
}
```

这样可以避免把列名硬编码到代码里。

## 7. 结构化数据模型设计

### 7.1 原始行模型

```cpp
struct InspectionRow {
    std::string stake_no;
    std::string slab_no_raw;
    std::string project_name;
    std::string defect_location_raw;
    std::string check_item_raw;
    std::string defect_desc_raw;
    std::string judgement_raw;
    std::string inspect_date_raw;
    int source_row = 0;
};
```

### 7.2 结构化病害模型

```cpp
enum class DefectKind {
    LongitudinalCrack,
    TransverseCrack,
    DiagonalCrack,
    AlligatorCrack,
    Seepage,
    Damage,
    ExposedRebar,
    Deformation,
    StepOffset,
    Unknown
};

enum class ZoneBand {
    LeftWall,
    LeftWaist,
    Crown,
    RightWaist,
    RightWall,
    Unknown
};

struct DefectFeature {
    DefectKind kind = DefectKind::Unknown;
    ZoneBand zone = ZoneBand::Unknown;

    std::vector<int> slab_nos;
    double offset_from_slab_start_m = -1.0;
    double offset_from_crown_center_m = -1.0;

    double length_m = -1.0;
    double width_mm = -1.0;
    double area_m2 = -1.0;
    int count = 1;

    bool repaired = false;
    bool patch_damaged = false;
    bool parse_warning = false;

    std::string stake_no;
    std::string inspect_date;
    std::string original_location;
    std::string original_check_item;
    std::string original_desc;
    std::string note;
};
```

### 7.3 渲染目标模型

```cpp
struct DefectPlacement {
    DefectFeature feature;
    int page_index = 0;
    int column_index = 0;
    double x_m = 0.0;
    double y_m = 0.0;
    double angle_deg = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
};
```

## 8. 病害文本解析方案

核心原则：

1. 先用 `检查内容` 判主类型。
2. 再用 `病害描述` 提参数。
3. 两者冲突时保留告警信息，不静默吞掉。

### 8.1 病害类型识别优先级

建议按以下优先级判断：

1. 优先使用 `检查内容`
2. 若 `检查内容` 不可靠或为空，再回退到 `病害描述`

映射建议：

1. `纵向裂缝` -> `LongitudinalCrack`
2. `环向裂缝` -> `TransverseCrack`
3. `斜向裂缝` -> `DiagonalCrack`
4. `网状裂缝` -> `AlligatorCrack`
5. `渗水印迹`、`渗水`、`泛碱` -> `Seepage`
6. `露筋`、`露筋锈蚀` -> `ExposedRebar`
7. `破损`、`剥落`、`起皮脱落` -> `Damage`
8. `变形` -> `Deformation`
9. `错台` -> `StepOffset`

### 8.2 病害位置识别

建议先做标准词典映射：

1. `左侧墙`、`左边墙` -> `LeftWall`
2. `左拱腰` -> `LeftWaist`
3. `拱顶`、`拱顶线` -> `Crown`
4. `右拱腰` -> `RightWaist`
5. `右侧墙`、`右边墙` -> `RightWall`

如果 `病害位置` 缺失，可从 `病害描述` 中补识别，例如出现“左侧墙环向裂缝”。

### 8.3 数值解析规则

建议全部用正则 + 单位归一，至少支持以下模式。

#### 1. 板块号

1. 单板：
   - `47`
   - `001`

2. 跨板：
   - `1~2号板`
   - `1-2号板`
   - `001~002`

解析后统一为整数列表，例如 `[1]` 或 `[1,2]`。

#### 2. 距板端

正则建议：

```text
距(?:\d+号板)?板端(\d+(?:\.\d+)?)m
```

示例：

1. `距板端5.5m`
2. `距1号板板端5m`

输出：

`offset_from_slab_start_m = 5.5`

#### 3. 距拱顶中心线

正则建议：

```text
距拱顶中心线(\d+(?:\.\d+)?)m
```

这个值主要用于分区内的二次微调，不能替代主分区。

#### 4. 长度

正则建议：

```text
长度为(\d+(?:\.\d+)?)m
累计长度为(\d+(?:\.\d+)?)m
长(?:度)?(\d+(?:\.\d+)?)m
```

#### 5. 宽度

正则建议：

```text
宽度为(\d+(?:\.\d+)?)mm
宽(\d+(?:\.\d+)?)mm
```

#### 6. 面积

正则建议：

```text
面积为(\d+(?:\.\d+)?)m2
面积为(\d+(?:\.\d+)?)m²
面积(\d+(?:\.\d+)?)m2
```

同时支持乘积形式：

```text
(\d+(?:\.\d+)?)\s*[x×*]\s*(\d+(?:\.\d+)?)m2
```

若出现乘积形式，则按乘积计算面积。

#### 7. 数量

正则建议：

```text
(\d+)处
```

用于“2处露筋锈蚀”。

#### 8. 修补状态

关键词：

1. `已修`
2. `已修补`
3. `已修复`
4. `修补后`

若命中则：

`repaired = true`

#### 9. 修补异常

关键词：

1. `修补处局部起皮脱落`
2. `修补后破损`

这类信息建议保存在 `note`，并在图中附短注。

### 8.4 解析结果置信度

建议为每条记录保留解析状态：

1. `OK`
2. `PARTIAL`
3. `FAILED`

原因：

“病害描述”不是标准语法，后续一定会遇到脏数据。

失败时不能丢记录，必须降级出图。

## 9. 定位与坐标映射方案

这一部分是整个系统最关键的业务层。

### 9.1 坐标系定义

建议整个内部统一用米为单位。

原因：

1. 当前 `symbols.json` 已经以米为主。
2. 病害长度、距离板端本身就是米。
3. 后续如需输出毫米，只在 DXF 写出前统一乘比例。

### 9.2 板块列坐标

建议每个板块在展开图中对应一个固定宽度列：

```text
global_x = page_origin_x + column_index * slab_cell_width + x_local
```

其中：

1. `column_index`
   是当前页内第几块板。

2. `slab_cell_width`
   是该板块列宽。

3. `x_local`
   是病害在本板内的相对横坐标。

### 9.3 病害在板内的横向定位

建议优先级如下：

1. 有 `距板端 Xm`
   则按板块长度比例定位：

```text
x_local = slab_draw_width * (offset_from_slab_start_m / slab_length_m)
```

2. 无 `距板端` 但有跨板信息
   放到跨板边界附近。

3. 都没有
   放在板块中心。

### 9.4 板长配置

实际工程中板长不一定完全一致，因此建议支持：

1. 全局默认板长，例如 `10.0m`
2. 按板块号覆盖板长

例如：

```json
{
  "slab_length_default_m": 10.0,
  "slab_length_overrides": {
    "14": 8.5,
    "15": 8.5
  }
}
```

### 9.5 纵向分区坐标

建议把展开图分成五条主带：

1. `LeftWall`
2. `LeftWaist`
3. `Crown`
4. `RightWaist`
5. `RightWall`

每条带定义一个中心线：

```text
zone_center_y[LeftWall]
zone_center_y[LeftWaist]
zone_center_y[Crown]
zone_center_y[RightWaist]
zone_center_y[RightWall]
```

病害默认放到所在带中心。

### 9.6 分区内微调

若解析到以下信息，可做带内偏移：

1. `距拱顶中心线 Xm`
2. `距侧墙底线 Xm`
3. `左` / `右` 倾向词

建议规则：

1. 先按主分区放带中心。
2. 再在带高的 20% 到 30% 范围内微调。
3. 若缺乏可靠信息，不微调。

这样稳定性高，不容易把病害画出格。

### 9.7 跨板病害

例如：

`1~2号板，距1号板板端5m，纵向裂缝，长度10m`

建议支持两种策略：

#### 策略 A：逻辑一条，绘图时跨列延展

适合长裂缝。

优点：

1. 图面真实。
2. 方便表达跨板连续病害。

#### 策略 B：拆成两段

按板缝切成两条绘图对象，但共享同一记录 ID。

建议：

MVP 可先做策略 B，后续再升级策略 A。

### 9.8 重叠消解

同一板、同一分区内很容易有多个病害重叠。

建议规则：

1. 先按 `x_local` 排序。
2. 同类病害相距过近时，沿 Y 方向上下抖动。
3. 抖动幅度限制在该带高度的 10% 以内。
4. 文本标注优先避让，不强行压在线上。

## 10. 渲染策略设计

### 10.1 病害分类渲染

建议按“参数型”和“符号型”分开。

#### A. 参数型病害

优先直接代码绘制：

1. 纵向裂缝
2. 环向裂缝
3. 斜向裂缝
4. 露筋

原因：

1. 长度是主要信息。
2. 方向清晰。
3. 用代码直接画比用符号拉伸更稳定。

#### B. 符号型病害

继续复用 `symbols.json`：

1. 渗水
2. 破损
3. 网裂
4. 变形
5. 错台

原因：

1. 形状更像图例。
2. 一般不要求严格几何长度。
3. 适合 uniform 缩放。

### 10.2 参数型病害具体绘法

#### 1. 纵向裂缝

在展开图中通常画成与隧道纵向一致的裂缝线。

建议：

1. 基于中心点 + 长度生成折线。
2. 可做轻微锯齿或波折效果，避免太机械。
3. 标注 `L=` 与 `W=`。

#### 2. 环向裂缝

在展开图中通常更接近竖向穿带线。

建议：

1. 不要继续依赖当前 `crack_trans` 的 `stretch_x`。
2. 直接按目标方向生成几何。
3. 标注长度和宽度。

#### 3. 斜向裂缝

建议：

1. 根据病害类型给默认角度，例如 `45°` 或 `-45°`。
2. 若描述中将来出现角度，可覆盖默认值。
3. 长度按 `length_m` 控制。

#### 4. 露筋

建议：

1. 以一条主筋线 + 若干短横线表示。
2. 若 `count > 1`，可以文字标注 `2处`，不必画多套复杂图元。
3. 若只有累计长度，则按累计长度决定主线长度。

### 10.3 符号型病害具体绘法

#### 1. 渗水

优先复用 `seepage` 符号。

缩放策略：

1. 有面积时按面积开方确定统一缩放倍数。
2. 无面积时使用默认尺寸。

#### 2. 破损

优先复用 `damage` 符号。

如果描述为“局部起皮脱落”“修补后破损”，可在旁边追加短注。

#### 3. 网裂

优先复用 `crack_alligator`。

若有面积则统一缩放；若只有长度无面积，则按默认尺寸并附注原文。

### 10.4 文字标注策略

文字标注必须单独设计，不能后补。

推荐写法：

1. 裂缝：
   `L=4m`
   `W=0.30mm`
   `已修`

2. 渗水：
   `A=1.5m2`

3. 露筋：
   `L=1m`
   `2处`

4. 无法可靠解析：
   保留一行摘要，例如原文前 18 到 24 个字符。

建议使用单行 `TEXT` 即可，只有超长说明才用 `MTEXT`。

## 11. 对现有 symbols.json 的扩展建议

如果后续仍要深入使用符号库，建议把 schema 从当前版本扩展为：

```json
{
  "id": "crack_diag",
  "kind": "DiagonalCrack",
  "params": {
    "scale_mode": "uniform",
    "base_length_m": 1.0,
    "length_axis": "path",
    "default_angle_deg": 45.0,
    "label_anchor": [0.5, 0.6]
  }
}
```

新增字段说明：

1. `length_axis`
   - `x`
   - `y`
   - `path`

2. `default_angle_deg`
   缺少方向信息时的默认旋转角。

3. `label_anchor`
   推荐文字放置参考点。

但从工程风险上讲，裂缝类仍建议优先代码直画，不建议过度依赖图块拉伸。

## 12. 模块拆分建议

建议新增以下模块，职责明确分开。

### 12.1 `InspectionTableReader`

职责：

1. 读取 CSV 或 xlsx。
2. 输出 `InspectionRow` 列表。
3. 处理编码、空行、列映射。

### 12.2 `DefectTextParser`

职责：

1. 从 `InspectionRow` 解析出 `DefectFeature`。
2. 给出解析告警。
3. 统一单位。

### 12.3 `LayoutConfig`

职责：

1. 提供板长、分页、分区高、图层名、字高等配置。
2. 提供隧道名、行别、起始板号、每页列数等版式信息。

### 12.4 `SlabLayoutEngine`

职责：

1. 计算板块在页中的位置。
2. 计算病害中心点坐标。
3. 处理跨板、重叠避让。

### 12.5 `DefectRenderPlanner`

职责：

1. 决定某条病害采用“直接绘制”还是“符号实例化”。
2. 生成中间绘图对象，例如：
   - line
   - polyline
   - text
   - symbol instance

### 12.6 `DiseaseLayoutDxfWriter`

职责：

1. 输出页眉、网格、板块号。
2. 输出病害图元。
3. 输出文字标注。
4. 写图层和文字样式。

这也是后续最合适的总写图类名。

## 13. DXF 写出层设计

### 13.1 推荐中间绘图对象

建议不要让业务模块直接操作 `DRW_*`，先定义项目自己的中间对象：

```cpp
struct DrawLine { Point2D a, b; std::string layer; };
struct DrawPolyline { std::vector<Point2D> pts; bool closed; std::string layer; };
struct DrawText { Point2D pos; double height; double angle_deg; std::string text; std::string layer; };
```

然后由 `DiseaseLayoutDxfWriter` 统一转换到 libdxfrw。

好处：

1. 业务层不依赖 DXF 库细节。
2. 后续更容易单元测试。
3. 将来如果想输出 SVG 或 PDF，也能复用中间层。

### 13.2 图层规划建议

建议至少分层：

1. `FRAME`
   网格、边框、板块线

2. `TEXT`
   页眉、板块号、一般标注

3. `DEFECT_CRACK`
   裂缝

4. `DEFECT_WATER`
   渗水

5. `DEFECT_DAMAGE`
   破损

6. `DEFECT_REBAR`
   露筋

7. `DEFECT_NOTE`
   修补、原文摘要、异常说明

### 13.3 文字样式建议

建议统一：

1. 字高
2. 旋转角
3. 对齐方式
4. 中文字体样式

如初期字体样式控制不稳定，可先使用默认 `STANDARD`，先保证内容可见。

## 14. 排版配置建议

建议单独提供一个配置文件，例如：

```json
{
  "project": {
    "tunnel_name": "新开岭",
    "line_name": "上行"
  },
  "layout": {
    "columns_per_panel": 5,
    "panel_gap_y": 2.0,
    "page_margin_left": 1.5,
    "page_margin_top": 1.0,
    "slab_draw_width": 3.0,
    "zone_heights": {
      "LeftWall": 1.2,
      "LeftWaist": 1.2,
      "Crown": 1.2,
      "RightWaist": 1.2,
      "RightWall": 1.2
    },
    "label_height": 0.25
  },
  "slab": {
    "default_length_m": 10.0
  }
}
```

说明：

这里的 `slab_draw_width` 是“图上显示宽度”，不一定等于真实板长。
真实板长用于比例换算。

## 15. 错误处理和降级策略

这个系统不能只处理理想数据，必须有降级出图能力。

### 15.1 解析失败时的处理

1. 病害类型识别失败
   放到 `Unknown`，绘制一个默认占位符并附原文摘要。

2. 位置识别失败
   放到当前板块的中部，并打告警。

3. 长度缺失
   使用默认长度，例如 `1.0m` 或默认符号尺寸。

4. 面积缺失
   使用默认 uniform 缩放。

5. 板块号缺失
   放入“未定位”页，不能直接丢弃。

### 15.2 输出诊断文件

建议每次出图同时生成一个诊断文件，例如：

`out_defect_layout_report.json`

记录：

1. 总记录数
2. 成功解析数
3. 部分解析数
4. 失败数
5. 每条失败原因

这样后续修规则时非常高效。

## 16. 实施顺序建议

为了降低风险，建议按以下顺序推进。

### 阶段 1：打通最小闭环

目标：

从 CSV 读取 10 到 20 条记录，输出含网格、裂缝、简单标注的 DXF。

范围：

1. 只支持 CSV
2. 只支持：
   - 纵向裂缝
   - 环向裂缝
   - 斜向裂缝
   - 渗水
   - 露筋
3. 只支持单板定位
4. 只支持单行文本标注

### 阶段 2：补齐复杂病害

目标：

加入：

1. 破损
2. 网裂
3. 修补异常说明
4. 跨板病害
5. 重叠避让

### 阶段 3：补齐工程化能力

目标：

1. 支持 xlsx 直读
2. 支持配置文件
3. 支持诊断报告
4. 支持样式统一
5. 支持批量文件处理

## 17. 验证方案

建议从一开始就准备验证集，而不是做完再试。

### 17.1 单元测试

优先测：

1. 病害类型识别
2. 长度解析
3. 宽度解析
4. 面积解析
5. 板块号解析
6. 位置映射

### 17.2 样例回归

准备一组固定样例，例如：

1. `距板端5.5m，环向裂缝，长度为4m，宽度为0.30mm，修补处局部起皮脱落`
2. `1~2号板，距1号板板端5m，距拱顶中心线1m，纵向裂缝，长度为10.0m，宽度为0.50mm`
3. `距板端5m，2处露筋锈蚀，累计长度为1m`
4. `距板端0m，施工缝渗水印迹，面积为1m2`
5. `距板端4m，斜向裂缝，长度为3m，宽0.20mm`

固定这些输入，对应输出图中的位置和标注应保持一致。

### 17.3 人工 CAD 校核项

每轮验证至少检查：

1. 板块顺序是否正确
2. 病害是否落在正确分区
3. 长裂缝长度是否明显异常
4. 标注是否压线
5. 渗水和破损是否过大或过小
6. 中文是否乱码

## 18. 关键风险与应对

### 风险 1：病害描述不规范

应对：

1. 规则解析必须允许 `PARTIAL`
2. 永远保留原文
3. 输出诊断报告

### 风险 2：板长与“距板端”基准不统一

应对：

1. 板长配置化
2. 必要时按板号单独覆盖
3. 对超范围距离打告警

### 风险 3：环向裂缝和斜向裂缝缩放失真

应对：

不要依赖现有 `stretch_x` 单一模型，直接代码绘制。

### 风险 4：跨板病害难以表达

应对：

MVP 先按板缝拆分，确保结果稳定可用。

### 风险 5：中文文字乱码

应对：

1. 输入侧统一 UTF-8
2. DXF 写出前做一次中文样例验证
3. 文字样式尽量简单

## 19. 结合当前仓库的具体结论

针对当前 `auto_cad` 仓库，最合理的演进方式不是推倒重来，而是：

1. 保留 `DxfSymbolCollector + SymbolJsonWriter`
   继续维护符号资产库。

2. 保留 `symbols.json`
   继续作为渗水、破损、网裂等符号型病害的素材来源。

3. 不直接扩展现有 `JsonSymbolDxfWriter` 来承载全部业务
   因为它现在的职责是“顺序输出符号”，不是“排版完整病害展开图”。

4. 新增一条并行主链路更合理：
   - `InspectionTableReader`
   - `DefectTextParser`
   - `SlabLayoutEngine`
   - `DiseaseLayoutDxfWriter`

5. 裂缝类、露筋类优先代码绘制
   符号型病害继续复用 JSON 符号库。

这条路线改动最可控，也最符合当前工程已有资产。

## 20. 下一步建议

如果下一步开始进入实现阶段，建议按下面顺序落地：

1. 先确定一份真实的 CSV 测试样表。
2. 先确定一份 `layout_config.json`。
3. 先实现文本解析，不急着写 DXF。
4. 用解析结果先打印到控制台或 JSON，确认规则正确。
5. 再做网格和板块框架。
6. 最后把病害图元和文字落图。

这样每一步都可验证，不会把问题混在一起。
