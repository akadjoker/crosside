#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace crosside::model {

struct PlatformBlock {
    std::vector<std::string> src;
    std::vector<std::string> include;
    std::vector<std::string> cppArgs;
    std::vector<std::string> ccArgs;
    std::vector<std::string> ldArgs;
    std::string shellTemplate;
};

struct BuildArgs {
    std::vector<std::string> cpp;
    std::vector<std::string> cc;
    std::vector<std::string> ld;
};

struct ModuleSpec {
    std::string name;
    std::filesystem::path dir;
    bool staticLib = true;
    std::vector<std::string> depends;
    std::vector<std::string> systems;

    PlatformBlock main;
    PlatformBlock desktop;
    PlatformBlock android;
    PlatformBlock web;
};

struct ProjectSpec {
    std::string name;
    std::filesystem::path root;
    std::filesystem::path filePath;

    std::vector<std::string> modules;
    std::vector<std::filesystem::path> src;
    std::vector<std::filesystem::path> include;

    BuildArgs main;
    BuildArgs desktop;
    BuildArgs android;
    BuildArgs web;

    std::string androidPackage;
    std::string androidActivity;
    std::string androidLabel;
    std::string androidManifestMode;
    std::vector<std::filesystem::path> androidJavaSources;
    std::filesystem::path androidIcon;
    std::unordered_map<std::string, std::filesystem::path> androidIcons;
    std::filesystem::path androidRoundIcon;
    std::unordered_map<std::string, std::filesystem::path> androidRoundIcons;
    std::filesystem::path androidAdaptiveForeground;
    std::filesystem::path androidAdaptiveMonochrome;
    std::filesystem::path androidAdaptiveBackgroundImage;
    std::string androidAdaptiveBackgroundColor;
    bool androidAdaptiveRound = true;
    std::filesystem::path androidManifestTemplate;
    std::unordered_map<std::string, std::string> androidManifestVars;
    std::string webShell;
};

using ModuleMap = std::unordered_map<std::string, ModuleSpec>;

} // namespace crosside::model
