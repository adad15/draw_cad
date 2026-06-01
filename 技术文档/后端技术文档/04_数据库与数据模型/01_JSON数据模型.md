# JSON 数据模型

更新时间：2026-06-01

## 1. 数据存储形态

`auto_cad` 当前没有数据库。数据主要来自 Excel、DXF 和运行时生成的中间 JSON 文件。

| 数据类型 | 来源 | 用途 |
| --- | --- | --- |
| 病害数据 | 外观病害 Excel | 绘制病害位置、类型、尺寸和标注 |
| 板长数据 | 板长 Excel | 计算纵向排布、环号和里程 |
| 病害符号 | 图例 DXF | 复用病害符号几何 |
| 中间 JSON | 后端运行过程 | 调试、批处理或绘图阶段传递 |
| 输出 DXF | 后端生成 | CAD 成果文件 |

## 2. `symbols.json`

用途：保存或调试病害符号映射。

典型内容：

```json
{
  "crack": {
    "name": "裂缝",
    "layer": "disease",
    "block": "CRACK_SYMBOL"
  }
}
```

关键字段：

| 字段 | 说明 |
| --- | --- |
| `name` | 病害中文名称 |
| `layer` | 输出图层 |
| `block` | 图块或符号标识 |

## 3. `disease_report.json`

用途：保存病害 Excel 解析后的结构化结果。

典型内容：

```json
{
  "items": [
    {
      "mileage": "K1+120",
      "ring": 32,
      "type": "裂缝",
      "width": 0.2,
      "length": 1.5
    }
  ]
}
```

关键字段：

| 字段 | 说明 |
| --- | --- |
| `mileage` | 里程文本 |
| `ring` | 环号 |
| `type` | 病害类型 |
| `width`、`length` | 病害尺寸 |

## 4. `board_lengths.json`

用途：保存板长 Excel 解析后的结构化结果。

典型内容：

```json
{
  "boards": [
    {
      "startMileage": "K1+000",
      "endMileage": "K1+012",
      "length": 12.0
    }
  ]
}
```

关键字段：

| 字段 | 说明 |
| --- | --- |
| `startMileage` | 起始里程 |
| `endMileage` | 结束里程 |
| `length` | 板长 |

## 5. 维护规则

1. JSON 字段调整后，同步检查读取、绘制和测试脚本。
2. 中间 JSON 不作为长期数据库设计，字段应服务于 CAD 生成链路。
3. 标书/AI 配置 JSON 已迁出到独立 `tender_ai_backend` 项目维护。
