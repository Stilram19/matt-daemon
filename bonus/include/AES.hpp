#pragma once

#include <vector>
#include <string>

std::string generate_session_key(size_t length);
namespace aes {
        std::vector<unsigned char> encrypt(
        const std::string& plaintext,
        const std::string& password
    );
    std::vector<unsigned char> decrypt(
        const std::vector<unsigned char>& encrypted_data,
        const std::string& password
    );
}