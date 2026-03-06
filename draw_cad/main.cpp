#include "dxf_to_json.h"

#include <exception>
#include <iostream>
#include <string>

namespace {
	void printUsage(const char* exeName) {
		std::cout
			<< "Usage:\n"
			<< "  " << exeName << " [input.dxf] [output.json] [unit] [block_prefix]\n\n"
			<< "Example:\n"
			<< "  " << exeName << " Drawing1.dxf symbols.json m SYM_\n";
	}
}

int main(int argc, char* argv[]) {
	try {
		if (argc > 1) {
			const std::string arg1 = argv[1] ? argv[1] : "";
			if (arg1 == "-h" || arg1 == "--help") {
				printUsage(argv[0] ? argv[0] : "draw_cad");
				return 0;
			}
		}

		MyConfig config; // 使用默认值

		// 按位置参数覆盖默认配置
		if (argc > 1 && argv[1]) config.input_dxf = argv[1];
		if (argc > 2 && argv[2]) config.output_json = argv[2];
		if (argc > 3 && argv[3]) config.unit = argv[3];
		if (argc > 4 && argv[4]) config.block_prefix = argv[4];

		dxf_to_json app(config);
		const ConvertResult result = app.runExtractor();

		if (!result.ok()) {
			std::cerr << "[ERROR] code=" << result.code << ", message=" << result.message << "\n";
			return result.code;
		}

		std::cout << "[OK] " << result.message << "\n";
		return 0;
	}
	catch (const std::exception& ex) {
		std::cerr << "[EXCEPTION] " << ex.what() << "\n";
		return 100;
	}
	catch (...) {
		std::cerr << "[EXCEPTION] unknown exception\n";
		return 101;
	}
}