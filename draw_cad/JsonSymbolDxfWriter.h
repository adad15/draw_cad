#pragma once

#include "dxf_to_json.h"
#include <vector>

// JsonSymbolDxfWriter 用于读取项目生成的 symbols.json，
// 按调用方指定的符号列表筛选图形，必要时执行 stretch_x 缩放，
// 最终把结果写入一个新的 DXF 文件。
class JsonSymbolDxfWriter final {
public:
    // 一个 SymbolRequest 对应 symbols.json 中的一个符号对象。
    // `symbol_id` 用来匹配 JSON 里的 `id` 字段。
    // `stretch_x_scale` 只有在该符号的
    // params.scale_mode == "stretch_x" 时才会生效。
    struct SymbolRequest {
        std::string symbol_id;
        double stretch_x_scale = 1.0;
    };

    // 主流程：
    // 1. 解析 SymbolJsonWriter 生成的 symbols.json。
    // 2. 根据调用方给出的 id 列表找到需要输出的符号。
    // 3. 对允许 stretch_x 的符号应用对应的缩放倍数。
    // 4. 按 `symbol_gap` 从左到右依次排布这些符号。
    // 5. 将排布后的所有图元写入新的 DXF 文件。
    //
    // 参数说明：
    // - input_json: 输入的 symbols.json 路径。
    // - output_dxf: 输出 DXF 路径，会创建或覆盖该文件。
    // - requested_symbols: 需要导出的符号请求列表，顺序即输出顺序。
    // - symbol_gap: 相邻两个输出符号之间的额外水平间距。
    static ConvertResult run(
        const std::string& input_json,
        const std::string& output_dxf,
        const std::vector<SymbolRequest>& requested_symbols,
        double symbol_gap = 1.0
    );
};
