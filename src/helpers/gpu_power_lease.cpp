// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

namespace
{
std::string readText(const std::filesystem::path &path)
{
    std::ifstream file(path);
    std::string value;
    if (!file || !std::getline(file, value))
        return {};

    while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.erase(value.begin());
    return value;
}

bool writeText(const std::filesystem::path &path, const std::string &value)
{
    std::ofstream file(path);
    if (!file)
        return false;
    file << value << '\n';
    file.flush();
    return file.good();
}

void restorePowerControl(const std::filesystem::path &path, const std::string &value)
{
    if (!writeText(path, value))
        std::cerr << "Could not restore GPU runtime power setting.\n";
}
} // namespace

int main(int argc, char **argv)
{
    static const std::regex pciAddressPattern(R"(^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]$)");
    if (argc != 2 || !std::regex_match(argv[1], pciAddressPattern))
    {
        std::cerr << "Expected one PCI address in domain:bus:device.function format.\n";
        return 2;
    }

    const std::filesystem::path devicePath = std::filesystem::path("/sys/bus/pci/devices") / argv[1];
    if (readText(devicePath / "vendor") != "0x1002")
    {
        std::cerr << "The selected PCI device is not an AMD GPU.\n";
        return 2;
    }

    std::error_code error;
    const std::filesystem::path driverPath = std::filesystem::canonical(devicePath / "driver", error);
    if (error || driverPath.filename() != "amdgpu")
    {
        std::cerr << "The selected PCI device is not bound to amdgpu.\n";
        return 2;
    }

    const std::filesystem::path controlPath = devicePath / "power/control";
    const std::string previous = readText(controlPath);
    if (previous != "auto" && previous != "on")
    {
        std::cerr << "Could not read the GPU runtime power setting.\n";
        return 1;
    }

    if (previous != "on" && !writeText(controlPath, "on"))
    {
        std::cerr << "Could not resume the selected GPU.\n";
        return 1;
    }

    std::cout << "READY" << std::endl;
    std::string command;
    while (std::getline(std::cin, command))
    {
        if (command == "release")
            break;
    }

    restorePowerControl(controlPath, previous);
    return 0;
}
