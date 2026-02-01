#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Secure storage for encrypting sensitive data using libsodium.
 *
 * Provides symmetric encryption using libsodium's secret-box primitives.
 * Automatically generates and persists encryption keys. Used for storing
 * sensitive configuration values like API keys and credentials.
 *
 * @note This class is non-copyable.
 */
class SecureStorage {
public:
    /**
     * @brief Constructs a SecureStorage with the specified key path.
     * @param keyPath Path to the encryption key file.
     */
    explicit SecureStorage(const std::filesystem::path& keyPath);

    /**
     * @brief Destructor. Securely zeroes key material.
     */
    ~SecureStorage();

    SecureStorage(const SecureStorage&) = delete;
    SecureStorage& operator=(const SecureStorage&) = delete;

    /**
     * @brief Encrypts plaintext data.
     * @param plaintext Data to encrypt.
     * @return Base64-encoded ciphertext with nonce prepended.
     */
    std::string encrypt(const std::string& plaintext);

    /**
     * @brief Decrypts ciphertext data.
     * @param ciphertext Base64-encoded ciphertext with nonce prepended.
     * @return Decrypted plaintext, or nullopt if decryption fails.
     */
    std::optional<std::string> decrypt(const std::string& ciphertext);

    /**
     * @brief Checks if libsodium has been initialized.
     * @return True if initialized, false otherwise.
     */
    static bool isInitialized();

    /**
     * @brief Generates a random encryption key.
     * @return Base64-encoded random key suitable for symmetric encryption.
     */
    static std::string generateRandomKey();

private:
    bool loadOrGenerateKey();
    bool saveKey();

    std::filesystem::path keyPath_;
    std::vector<unsigned char> key_;
    bool initialized_{false};
};

/**
 * @brief Encodes binary data to Base64 string.
 * @param data Binary data to encode.
 * @return Base64-encoded string.
 */
std::string base64Encode(const std::vector<unsigned char>& data);

/**
 * @brief Decodes a Base64 string to binary data.
 * @param encoded Base64-encoded string.
 * @return Decoded binary data.
 */
std::vector<unsigned char> base64Decode(const std::string& encoded);

} // namespace netpulse::infra
