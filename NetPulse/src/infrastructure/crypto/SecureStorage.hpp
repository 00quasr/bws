#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::infra {

class SecureStorage {
public:
    explicit SecureStorage(const std::filesystem::path& keyPath);
    ~SecureStorage();

    SecureStorage(const SecureStorage&) = delete;
    SecureStorage& operator=(const SecureStorage&) = delete;

    std::string encrypt(const std::string& plaintext);
    std::optional<std::string> decrypt(const std::string& ciphertext);

    static bool isInitialized();
    static std::string generateRandomKey();

private:
    bool loadOrGenerateKey();
    bool saveKey();

    std::filesystem::path keyPath_;
    std::vector<unsigned char> key_;
    bool initialized_{false};
};

// Utility functions for Base64 encoding/decoding
std::string base64Encode(const std::vector<unsigned char>& data);
std::vector<unsigned char> base64Decode(const std::string& encoded);

} // namespace netpulse::infra
