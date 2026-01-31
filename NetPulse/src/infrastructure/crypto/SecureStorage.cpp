#include "infrastructure/crypto/SecureStorage.hpp"

#include <sodium.h>
#include <spdlog/spdlog.h>

#include <fstream>

namespace netpulse::infra {

namespace {

constexpr size_t KEY_SIZE = crypto_secretbox_KEYBYTES;
constexpr size_t NONCE_SIZE = crypto_secretbox_NONCEBYTES;
constexpr size_t MAC_SIZE = crypto_secretbox_MACBYTES;

} // namespace

SecureStorage::SecureStorage(const std::filesystem::path& keyPath) : keyPath_(keyPath) {
    if (sodium_init() < 0) {
        spdlog::error("Failed to initialize libsodium");
        return;
    }

    key_.resize(KEY_SIZE);
    initialized_ = loadOrGenerateKey();
}

SecureStorage::~SecureStorage() {
    if (!key_.empty()) {
        sodium_memzero(key_.data(), key_.size());
    }
}

bool SecureStorage::loadOrGenerateKey() {
    if (std::filesystem::exists(keyPath_)) {
        std::ifstream file(keyPath_, std::ios::binary);
        if (file) {
            file.read(reinterpret_cast<char*>(key_.data()), static_cast<std::streamsize>(KEY_SIZE));
            if (file.gcount() == static_cast<std::streamsize>(KEY_SIZE)) {
                spdlog::debug("Loaded encryption key from {}", keyPath_.string());
                return true;
            }
        }
        spdlog::warn("Failed to load encryption key, generating new one");
    }

    // Generate new key
    randombytes_buf(key_.data(), KEY_SIZE);
    return saveKey();
}

bool SecureStorage::saveKey() {
    // Create parent directory if needed
    auto parent = keyPath_.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(keyPath_, std::ios::binary);
    if (!file) {
        spdlog::error("Failed to create key file: {}", keyPath_.string());
        return false;
    }

    file.write(reinterpret_cast<const char*>(key_.data()), static_cast<std::streamsize>(KEY_SIZE));
    file.close();

    // Set restrictive permissions (owner read/write only)
#ifndef _WIN32
    std::filesystem::permissions(keyPath_, std::filesystem::perms::owner_read |
                                               std::filesystem::perms::owner_write);
#endif

    spdlog::info("Generated and saved new encryption key to {}", keyPath_.string());
    return true;
}

std::string SecureStorage::encrypt(const std::string& plaintext) {
    if (!initialized_) {
        spdlog::error("SecureStorage not initialized");
        return {};
    }

    std::vector<unsigned char> nonce(NONCE_SIZE);
    randombytes_buf(nonce.data(), NONCE_SIZE);

    std::vector<unsigned char> ciphertext(plaintext.size() + MAC_SIZE);

    if (crypto_secretbox_easy(ciphertext.data(),
                              reinterpret_cast<const unsigned char*>(plaintext.data()),
                              plaintext.size(), nonce.data(), key_.data()) != 0) {
        spdlog::error("Encryption failed");
        return {};
    }

    // Combine nonce + ciphertext
    std::vector<unsigned char> combined;
    combined.reserve(NONCE_SIZE + ciphertext.size());
    combined.insert(combined.end(), nonce.begin(), nonce.end());
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());

    return base64Encode(combined);
}

std::optional<std::string> SecureStorage::decrypt(const std::string& ciphertext) {
    if (!initialized_) {
        spdlog::error("SecureStorage not initialized");
        return std::nullopt;
    }

    auto combined = base64Decode(ciphertext);
    if (combined.size() < NONCE_SIZE + MAC_SIZE) {
        spdlog::error("Ciphertext too short");
        return std::nullopt;
    }

    std::vector<unsigned char> nonce(combined.begin(), combined.begin() + NONCE_SIZE);
    std::vector<unsigned char> encrypted(combined.begin() + NONCE_SIZE, combined.end());

    std::vector<unsigned char> plaintext(encrypted.size() - MAC_SIZE);

    if (crypto_secretbox_open_easy(plaintext.data(), encrypted.data(), encrypted.size(),
                                   nonce.data(), key_.data()) != 0) {
        spdlog::error("Decryption failed");
        return std::nullopt;
    }

    return std::string(plaintext.begin(), plaintext.end());
}

bool SecureStorage::isInitialized() {
    return sodium_init() >= 0;
}

std::string SecureStorage::generateRandomKey() {
    if (sodium_init() < 0) {
        return {};
    }

    std::vector<unsigned char> key(KEY_SIZE);
    randombytes_buf(key.data(), KEY_SIZE);
    return base64Encode(key);
}

// Base64 encoding/decoding using libsodium's implementation
std::string base64Encode(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return {};
    }

    size_t encodedLen = sodium_base64_encoded_len(data.size(), sodium_base64_VARIANT_ORIGINAL);
    std::string encoded(encodedLen, '\0');

    sodium_bin2base64(encoded.data(), encodedLen, data.data(), data.size(),
                      sodium_base64_VARIANT_ORIGINAL);

    // Remove null terminator
    while (!encoded.empty() && encoded.back() == '\0') {
        encoded.pop_back();
    }

    return encoded;
}

std::vector<unsigned char> base64Decode(const std::string& encoded) {
    if (encoded.empty()) {
        return {};
    }

    std::vector<unsigned char> decoded(encoded.size());
    size_t decodedLen = 0;

    if (sodium_base642bin(decoded.data(), decoded.size(), encoded.c_str(), encoded.size(),
                          nullptr, &decodedLen, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0) {
        return {};
    }

    decoded.resize(decodedLen);
    return decoded;
}

} // namespace netpulse::infra
