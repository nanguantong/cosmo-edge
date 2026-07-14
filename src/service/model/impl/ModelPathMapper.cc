// clang-format off
#include "service/detail/ServiceRegistry.h"
#include "service/model/impl/ModelPathMapper.h"
// clang-format on

#include <filesystem>
#include <fstream>
#include <vector>

#include "nlohmann/json.hpp"
#include "service/system/IAppInfoService.h"
#include "util/Log.h"
#include "util/NnBackendConstants.h"
#include "util/PathUtil.h"

namespace cosmo::service {

namespace {

    /// Pipeline config is always config.json inside the model directory.
    bool ResolvePipelineCfgPath(const std::string& directory, std::string& cfgPath) {
        const std::filesystem::path p = std::filesystem::path(directory) / "config.json";
        if (!std::filesystem::is_regular_file(p)) {
            LOG_WARN("Missing config.json. Directory:{}", directory);
            return false;
        }
        cfgPath = p.string();
        return true;
    }

    std::vector<std::string> GetFileWithExtension(const std::string& directory,
                                                  const std::string& extension) {
        std::vector<std::string> files;
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            return files;
        }
        for (auto& entry : std::filesystem::directory_iterator(directory)) {
            if (std::filesystem::is_regular_file(entry.path()) && entry.path().extension() == extension) {
                files.push_back(entry.path().filename().string());
            }
        }
        return files;
    }

    bool ResolveOcrCharacterTable(const std::string& directory, const std::string& cfg_path,
                                  std::string& word_dict_path) {
        std::ifstream stream(cfg_path);
        if (!stream.is_open()) {
            LOG_WARN("Cannot open OCR config: {}", cfg_path);
            return false;
        }

        nlohmann::json config;
        try {
            stream >> config;
        } catch (const std::exception& e) {
            LOG_WARN("Cannot parse OCR config {}: {}", cfg_path, e.what());
            return false;
        }

        if (!config.contains("models") || !config["models"].is_array() || config["models"].empty()) {
            LOG_WARN("OCR config has no model definition: {}", cfg_path);
            return false;
        }
        const auto& model = config["models"][0];
        if (!model.contains("params") || !model["params"].is_object() ||
            !model["params"].contains("character_table_file") ||
            !model["params"]["character_table_file"].is_string()) {
            LOG_WARN("OCR config has no character table binding: {}", cfg_path);
            return false;
        }

        const std::filesystem::path table_name = model["params"]["character_table_file"].get<std::string>();
        if (table_name.empty() || table_name.has_parent_path() || table_name.filename() != table_name ||
            table_name.extension() != ".txt") {
            LOG_WARN("OCR character table must be a .txt file in the package root: {}", table_name.string());
            return false;
        }

        const std::filesystem::path table_path = std::filesystem::path(directory) / table_name;
        if (!std::filesystem::is_regular_file(table_path)) {
            LOG_WARN("OCR character table is missing: {}", table_path.string());
            return false;
        }
        word_dict_path = table_path.string();
        return true;
    }

}  // namespace

void ModelPathMapper::Set(const std::string& algCode, const std::string& modelPath) {
    if (algCode.empty() || modelPath.empty()) {
        LOG_INFO("AlgCode:{} Or ModelPath:{} Empty", algCode, modelPath);
        return;
    }
    LOG_INFO("AlgCode:{} Set ModelPath:{}", algCode, modelPath);
    std::lock_guard<std::shared_mutex> lock(mtx_);
    map_[algCode] = modelPath;
}

std::string ModelPathMapper::Get(const std::string& algCode) {
    if (algCode.empty()) {
        LOG_INFO("AlgCode:{} Empty", algCode);
        return "";
    }
    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = map_.find(algCode);
    return it != map_.end() ? it->second : "";
}

bool ModelPathMapper::GetModelCfg(const std::string& algCode, std::string& cfgPath, std::string& modelPath) {
    if (algCode.empty()) {
        LOG_WARN("AlgCode:{} Empty", algCode);
        return false;
    }

    if (service::ServiceRegistry::Instance().Get<service::IAppInfoService>().GetModelDebug()) {
        std::string path = cosmo::path::GetResourcePath();
        cfgPath          = (std::filesystem::path(path) / (algCode + ".json")).string();
        modelPath        = (std::filesystem::path(path) / (algCode + cosmo::util::kModelFileExt)).string();
        return true;
    }

    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = map_.find(algCode);
    if (it == map_.end() || it->second.empty()) {
        LOG_WARN("Model Directory Is Empty. AlgCode:{}", algCode);
        return false;
    }
    std::string directory = it->second;

    auto model_res = GetFileWithExtension(directory, cosmo::util::kModelFileExt);
    if (model_res.size() < 1) {
        LOG_WARN("No Model File. Directory:{}", directory);
        return false;
    }

    if (!ResolvePipelineCfgPath(directory, cfgPath)) {
        return false;
    }

    modelPath = model_res.size() > 1 ? directory : (std::filesystem::path(directory) / model_res[0]).string();
    return true;
}

bool ModelPathMapper::GetModelCfg(const std::string& algCode, std::string& cfgPath, std::string& modelPath,
                                  std::string& wordDictPath) {
    if (algCode.empty()) {
        LOG_INFO("AlgCode:{} Empty", algCode);
        return false;
    }

    if (service::ServiceRegistry::Instance().Get<service::IAppInfoService>().GetModelDebug()) {
        std::string path = cosmo::path::GetResourcePath();
        cfgPath          = (std::filesystem::path(path) / (algCode + ".json")).string();
        modelPath        = (std::filesystem::path(path) / (algCode + cosmo::util::kModelFileExt)).string();
        return ResolveOcrCharacterTable(path, cfgPath, wordDictPath);
    }

    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = map_.find(algCode);
    if (it == map_.end() || it->second.empty()) {
        LOG_INFO("Model Directory Is Empty. AlgCode:{}", algCode);
        return false;
    }
    std::string directory = it->second;

    auto model_res = GetFileWithExtension(directory, cosmo::util::kModelFileExt);
    if (model_res.size() < 1) {
        LOG_INFO("No Model File. Directory:{}", directory);
        return false;
    }

    if (!ResolvePipelineCfgPath(directory, cfgPath)) {
        return false;
    }

    if (!ResolveOcrCharacterTable(directory, cfgPath, wordDictPath)) {
        return false;
    }

    modelPath = model_res.size() > 1 ? directory : (std::filesystem::path(directory) / model_res[0]).string();
    LOG_INFO("algCode:{}, modelPath:{}, cfgPath:{}, wordDictPath:{}", algCode, modelPath, cfgPath,
             wordDictPath);
    return true;
}

}  // namespace cosmo::service
