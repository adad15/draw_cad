# 方案A：外部符号库（JSON）+ C++ 参数化生成 CAD 病害图形（从0开始）

> 目标：你已有 CAD 病害图例（DWG），希望用 C++ 自动化把 Excel 中的病害记录绘制到 CAD 展开/俯视图上。  
> 核心做法：把“病害图例”工程化为**可复用符号库**（Symbol Library），再把 Excel 记录解析为结构化参数并实例化符号。

---

## 0. 总体思路（两条流水线）

### A. 离线：构建符号库（偶尔做）
**CAD 图例 DWG → 导出 DXF → 提取块几何 → 归一化（锚点/尺寸）→ `symbols.json`**

- 只做一次，或图例更新时重跑。
- 产物：`symbols.json`（可版本管理），后续出图只依赖它。

### B. 在线：批量出图（经常做）
**Excel → 解析（类型/位置/长度/宽度/面积…）→ 坐标定位（板块+分区）→ 读取 `symbols.json` → 变换实例化 → 输出 DXF/DWG**

- 每次有新 Excel 就跑一遍。
- 产物：`out_defects.dxf`（或叠加到模板后的 `out.dwg`）。

---

## 1. 数据模型（CAD 无关的业务层）

### 1.1 病害类型（语义枚举）
建议统一为：
- 纵向裂缝 / 环向裂缝 / 斜向裂缝 / 网状裂缝  
- 破损 / 锈胀露筋 / 渗水泛碱 / 墙体变形 / 错台

### 1.2 病害参数（单位规范）
内部建议统一：
- 坐标：m
- 长度：m
- 面积：m²
- 裂缝宽度：mm（可保留，也可换算为 m）

```cpp
enum class DefectKind {
  LongitudinalCrack,
  TransverseCrack,
  DiagonalCrack,
  AlligatorCrack,
  Damage,
  ExposedRebar,
  SeepageEfflorescence,
  WallDeformation,
  StepOffset
};

struct DefectParams {
  double x = 0.0, y = 0.0;      // m：定位后落点（板块局部坐标或图纸坐标）
  double length_m = 0.0;        // m：裂缝/露筋长度
  double width_mm = 0.0;        // mm：裂缝宽度
  double area_m2  = 0.0;        // m2：渗水/破损等面状
  double angle_deg = 0.0;       // 旋转角（斜裂缝、变形等）
  std::string label;            // 标注（L=, δ=, A= 或原文）
};
```

---

## 2. CAD 符号库源文件准备（你已有 DWG 图例时最关键的一步）

### 2.1 每种病害做成一个 Block（块）
在一张 `symbols_source.dwg` 里，把每个图例做成单独块：
- `SYM_CRACK_LONG`
- `SYM_CRACK_TRANS`
- `SYM_CRACK_DIAG`
- `SYM_CRACK_ALLIGATOR`
- `SYM_DAMAGE`
- `SYM_REBAR`
- `SYM_SEEPAGE`
- `SYM_DEFORM`
- `SYM_STEP`

### 2.2 统一“默认姿态”和“锚点”
- **块基点（BLOCK base point）= 你希望定位的锚点**
  - 推荐：符号中心（适合云线/错台/网裂）
  - 或：符号左端（适合方向型符号）
- 统一默认朝向：建议**沿 +X 方向**视为 0°，后续实例化统一旋转。
- 确认 DWG 单位（m 或 mm），后续在 JSON meta 里记录。

### 2.3 降低提取难度：尽量避免 SPLINE/HATCH
推荐块内部只用：
- LINE / LWPOLYLINE / ARC / CIRCLE  
如有 SPLINE，建议在 CAD 内转成多段线（近似）。

---

## 3. 导出 DXF（作为提取中间格式）
将 `symbols_source.dwg` 导出为 `symbols_source.dxf`（建议 R2010/R2013）。

> 方案A的关键：**DXF 更适合 C++ 自动解析和再输出**。DWG 解析通常需要 ObjectARX/ODA，工程成本大。

---

## 4. `symbols.json` 设计（符号库外部存储）

### 4.1 最小可用 JSON 结构
```json
{
  "meta": { "unit": "m", "version": 1 },
  "symbols": [
    {
      "id": "crack_long",
      "block": "SYM_CRACK_LONG",
      "kind": "LongitudinalCrack",
      "anchor": [0.0, 0.0],
      "bbox": { "min": [-0.5, -0.05], "max": [0.5, 0.05] },
      "style": { "layer": "CRACK", "linetype": "CONTINUOUS", "lineweight_mm": 0.25 },
      "params": { "scale_mode": "stretch_x", "base_length_m": 1.0 },
      "primitives": [
        { "type": "LINE", "a": [-0.5, 0.0], "b": [0.5, 0.0] }
      ]
    }
  ]
}
```

### 4.2 字段含义与工程建议
- `id`：业务侧引用的稳定 ID（不要直接用块名）
- `block`：来源块名（便于追溯/更新）
- `anchor`：归一化后的锚点（建议归一到 (0,0)）
- `bbox`：用于调试、避免重叠、推断默认尺度
- `style`：图层/线型等（出图一致性）
- `params.scale_mode`：
  - `uniform`：整体缩放（适合渗水云线、错台符号）
  - `stretch_x`：仅沿局部 X 拉伸（适合“按长度变化”的裂缝/露筋）
  - `stretch_y`：沿 Y 拉伸（少用）
- `base_length_m`：模板符号基准长度（例如块内部画的是 1m 裂缝）

---

## 5. 离线工具：`sym_extractor`（DXF → JSON）

### 5.1 功能
- 输入：`symbols_source.dxf`
- 过滤：块名 `SYM_` 前缀
- 输出：`symbols.json`

### 5.2 推荐依赖（C++）
- DXF 解析：`libdxfrw`
- JSON：`nlohmann/json`

### 5.3 MVP 支持的 DXF 实体（先够用）
- LINE
- LWPOLYLINE（多段线）
- ARC（可选：先离散成 polyline）

### 5.4 提取流程（算法）
对每个 BLOCK：
1. 遍历块内实体 → 转为内部原语（Line/Polyline/Arc）
2. 读取 BLOCK base point 作为锚点
3. 计算 bbox
4. 归一化：所有点减去 base point，使锚点变为 (0,0)
5. 输出到 `symbols.json`

---

## 6. 在线程序：批量出图主流程（Excel → DXF）

### 6.1 模块划分（建议工程目录）
```
proj/
  data/
    symbols.json
    template_grid.dxf        (可选：衬砌展开图模板/网格)
  src/
    excel_reader.cpp         (读 Excel 行)
    defect_parser.cpp        (正则解析长度/宽度/面积/方向)
    locator.cpp              (板块+分区 → x,y)
    symbol_library.cpp       (加载 symbols.json)
    placer.cpp               (实例化：缩放/拉伸/旋转/平移)
    dxf_writer.cpp           (输出 DXF：LINE/LWPOLYLINE/TEXT)
    main.cpp
```

### 6.2 关键步骤
1. **读取 Excel**
   - 读取：板块号、病害位置、检查内容、病害描述等列
2. **文本解析（regex + 单位归一）**
   - 距板端：`距板端(\d+(\.\d+)?)m`
   - 长度：`长度为?(\d+(\.\d+)?)m`
   - 宽度：`宽度为?(\d+(\.\d+)?)mm`
   - 面积：`面积为?(\d+(\.\d+)?)m2` 或 `S=(...)m2` 或 `0.05×1.5m2`
   - 方向：环向/纵向/斜向（优先用“检查内容”列）
3. **坐标定位（板块与分区）**
   - X：板块范围内按“距板端”映射：`x = slab_x0 + offset`
   - Y：按“左墙/左拱腰/拱顶/右拱腰/右墙”映射到分区中心：`y = (y_low+y_high)/2`
   - 解析失败降级：放到该板块该分区中心，并标注原文
4. **符号实例化（placer）**
   - 查 `symbols.json` 获取符号 primitives
   - 按 `scale_mode` 计算变换：
     - `uniform`：整体缩放（可按面积开方近似）
     - `stretch_x`：`sx = length / base_length`
   - 施加：缩放/拉伸 → 旋转 → 平移到 (x,y)
5. **输出 DXF**
   - 按符号 style 分图层输出
   - 追加 TEXT/MTEXT 标注：`L=...m, δ=...mm, A=...m²`

---

## 7. 核心代码骨架（概念级）

### 7.1 SymbolLibrary
```cpp
struct Style { std::string layer, linetype; double lineweight_mm; };

struct SymbolParams {
  std::string scale_mode;    // uniform / stretch_x / stretch_y
  double base_length_m = 1.0;
};

struct LinePrim { double ax, ay, bx, by; };
struct PolyPrim { std::vector<std::pair<double,double>> pts; bool closed; };
struct TextPrim { double x,y,h; std::string text; };

using Primitive = std::variant<LinePrim, PolyPrim, TextPrim>;

struct Symbol {
  std::string id;
  std::string kind;
  Style style;
  SymbolParams params;
  std::vector<Primitive> primitives;
};

class SymbolLibrary {
public:
  void load(const std::string& jsonPath);
  const Symbol& getById(const std::string& id) const;
};
```

### 7.2 Placer（变换实例化）
```cpp
struct Pt { double x,y; };

struct Mat2 { double a,b,c,d; };     // [a b; c d]
Pt mul(const Mat2& M, Pt p) { return {M.a*p.x + M.b*p.y, M.c*p.x + M.d*p.y}; }

Mat2 rot(double deg);
Mat2 scale(double sx, double sy);

std::vector<Primitive> instantiate(const Symbol& sym, const DefectParams& p) {
  double sx=1, sy=1;
  if (sym.params.scale_mode == "stretch_x" && sym.params.base_length_m>0 && p.length_m>0)
    sx = std::max(0.1, p.length_m / sym.params.base_length_m);
  // uniform: sx=sy=...

  Mat2 S = scale(sx, sy);
  Mat2 R = rot(p.angle_deg);
  // M = R*S
  Mat2 M{ R.a*S.a + R.b*S.c, R.a*S.b + R.b*S.d,
          R.c*S.a + R.d*S.c, R.c*S.b + R.d*S.d };

  // 对每个 primitive 的点做：p' = M*p + (p.x,p.y)
  // ...（略）
  return {};
}
```

---

## 8. 实施优先级（保证最快闭环）

### MVP 顺序（推荐）
1. **裂缝类（纵/环/斜）先用代码直接画线**
   - 长度严格来自 Excel
   - 宽度作为文字标注
2. **渗水/错台/露筋/破损/变形**先走“块几何提取 + uniform 缩放”
3. 做好“解析失败降级”：
   - 放到板块分区中心 + 标注原文，避免漏绘

---

## 9. 常见坑与规避

1. **“距板端”是左端还是右端？**
   - 先规定统一方向：板块左→右为里程增大
   - 可加启发式：若 offset > 0.8*slab_len，则认为从右端计：`x = x1 - offset`

2. **同一板块同一分区多条病害重叠**
   - 给 y 或 x 加小抖动（例如 ±0.1m）或按序号偏移

3. **符号块内部有 SPLINE/HATCH 导致提取困难**
   - CAD 侧先转 polyline；或提取器做离散化

---

## 10. 里程碑（建议工期）

- M0（0.5天）：整理 DWG 图例为 `SYM_*` 块 + 导出 DXF  
- M1（1天）：实现 `sym_extractor`（LINE/LWPOLYLINE）→ `symbols.json`  
- M2（1天）：实现主程序：加载 symbols + 手写测试病害 → 输出 DXF  
- M3（1~2天）：接入 Excel 解析 + 板块分区定位  
- M4（后续）：完善样式、标注、复杂符号（网裂/云线更像）

---

## 11. 建议交付物清单

- `symbols_source.dwg`（你维护的图例块库）
- `symbols_source.dxf`（从 DWG 导出）
- `symbols.json`（程序运行时读取）
- `sym_extractor.exe`（离线生成 JSON 的工具）
- `defect_drawer.exe`（在线：Excel → out.dxf）

---

## 12. 下一步你可以直接做什么（最小动作）
1. 把现有图例 DWG 中每种病害做成 `SYM_*` 块（统一基点）
2. 导出 `symbols_source.dxf`
3. 写/跑 `sym_extractor` 得到 `symbols.json`
4. 用主程序把 3~5 条测试病害实例化输出 `out.dxf`，在 CAD 打开验证
