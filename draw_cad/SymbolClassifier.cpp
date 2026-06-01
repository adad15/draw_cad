#include "SymbolClassifier.h"
#include "mytool.h"

std::string SymbolClassifier::BlockName_to_Kind(const std::string& block_name) {
	const std::string upper_name = mytool::toUpperAscii(block_name);
	if (upper_name.find("CRACK_LONG") != std::string::npos) return "LongitudinalCrack";
	if (upper_name.find("CRACK_TRANS") != std::string::npos) return "TransverseCrack";
	if (upper_name.find("CRACK_DIAG") != std::string::npos) return "DiagonalCrack";
	if (upper_name.find("CRACK_ALLIGATOR") != std::string::npos) return "AlligatorCrack";
	if (upper_name.find("DAMAGE") != std::string::npos) return "Damage";
	if (upper_name.find("REBAR") != std::string::npos) return "ExposedRebar";
	if (upper_name.find("SEEPAGE") != std::string::npos) return "SeepageEfflorescence";
	if (upper_name.find("DEFORM") != std::string::npos) return "WallDeformation";
	if (upper_name.find("STEP") != std::string::npos) return "StepOffset";
	return "Unknown";
}

std::string SymbolClassifier::inferScaleMode(const std::string& kind) {
	if (kind == "LongitudinalCrack" || kind == "TransverseCrack" || kind == "ExposedRebar") {
		return "stretch_x";
	}
	return "uniform";
}
