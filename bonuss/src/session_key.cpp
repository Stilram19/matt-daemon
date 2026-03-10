#include <openssl/rand.h>
#include <string>
#include <stdexcept>
#include <vector>

std::string generate_session_key(size_t length) {
    if (length < 12) {
        throw std::invalid_argument("Password length should be at least 12 characters.");
    }

    const std::string charset =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!@#$%^&*()-_=+[]{}|;:,.<>?";

    const size_t charset_size = charset.size();

    std::string password;
    password.resize(length);

    std::vector<unsigned char> random_bytes(length);

    if (RAND_bytes(random_bytes.data(), length) != 1) {
        throw std::runtime_error("Secure random generation failed.");
    }

    for (size_t i = 0; i < length; ++i) {
        password[i] = charset[random_bytes[i] % charset_size];
    }

    return password;
}
