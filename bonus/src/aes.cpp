#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/kdf.h>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>

#define SALT_SIZE 16
#define IV_SIZE 12
#define KEY_SIZE 32
#define TAG_SIZE 16
#define PBKDF2_ITERATIONS 100000


namespace aes {
    
    void handleErrors() {
        ERR_print_errors_fp(stderr);
        abort();
    }
    
    std::vector<unsigned char> encrypt(
        const std::string& plaintext,
        const std::string& password
    ) {
        unsigned char salt[SALT_SIZE];
        unsigned char iv[IV_SIZE];
        unsigned char key[KEY_SIZE];
        unsigned char tag[TAG_SIZE];
    
        RAND_bytes(salt, SALT_SIZE);
        RAND_bytes(iv, IV_SIZE);
    
        // Derive key using PBKDF2
        if (!PKCS5_PBKDF2_HMAC(
                password.c_str(), password.length(),
                salt, SALT_SIZE,
                PBKDF2_ITERATIONS,
                EVP_sha256(),
                KEY_SIZE, key)) {
            handleErrors();
        }
    
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) handleErrors();
    
        if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
            handleErrors();
    
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL))
            handleErrors();
    
        if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv))
            handleErrors();
    
        std::vector<unsigned char> ciphertext(plaintext.size());
        int len;
    
        if (1 != EVP_EncryptUpdate(ctx,
                                   ciphertext.data(),
                                   &len,
                                   (unsigned char*)plaintext.data(),
                                   plaintext.size()))
            handleErrors();
    
        int ciphertext_len = len;
    
        if (1 != EVP_EncryptFinal_ex(ctx,
                                     ciphertext.data() + len,
                                     &len))
            handleErrors();
    
        ciphertext_len += len;
    
        if (1 != EVP_CIPHER_CTX_ctrl(ctx,
                                     EVP_CTRL_GCM_GET_TAG,
                                     TAG_SIZE,
                                     tag))
            handleErrors();
    
        EVP_CIPHER_CTX_free(ctx);
    

        std::vector<unsigned char> output;
        output.insert(output.end(), salt, salt + SALT_SIZE);
        output.insert(output.end(), iv, iv + IV_SIZE);
        output.insert(output.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
        output.insert(output.end(), tag, tag + TAG_SIZE);
    
        return output;
    }
    
    std::vector<unsigned char> decrypt(
        const std::vector<unsigned char>& encrypted_data,
        const std::string& password
    ) {
        if (encrypted_data.size() < SALT_SIZE + IV_SIZE + TAG_SIZE) {
            throw std::runtime_error("Invalid encrypted data size");
        }

        const unsigned char* data = encrypted_data.data();

        const unsigned char* salt = data;
        const unsigned char* iv   = data + SALT_SIZE;
        const unsigned char* tag  = data + encrypted_data.size() - TAG_SIZE;
        const unsigned char* ciphertext =
            data + SALT_SIZE + IV_SIZE;

        int ciphertext_len =
            static_cast<int>(encrypted_data.size() - SALT_SIZE - IV_SIZE - TAG_SIZE);

        unsigned char key[KEY_SIZE];


        if (!PKCS5_PBKDF2_HMAC(
                password.c_str(),
                static_cast<int>(password.length()),
                salt,
                SALT_SIZE,
                PBKDF2_ITERATIONS,
                EVP_sha256(),
                KEY_SIZE,
                key)) {
            handleErrors();
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) handleErrors();

        if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
            handleErrors();

        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL))
            handleErrors();

        if (1 != EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv))
            handleErrors();

        std::vector<unsigned char> plaintext(ciphertext_len);

        int len = 0;

        if (1 != EVP_DecryptUpdate(ctx,
                                plaintext.data(),
                                &len,
                                ciphertext,
                                ciphertext_len))
            handleErrors();

        int plaintext_len = len;


        if (1 != EVP_CIPHER_CTX_ctrl(ctx,
                                    EVP_CTRL_GCM_SET_TAG,
                                    TAG_SIZE,
                                    (void*)tag))
            handleErrors();

        int ret = EVP_DecryptFinal_ex(
            ctx,
            plaintext.data() + len,
            &len
        );

        EVP_CIPHER_CTX_free(ctx);

        if (ret <= 0) {
            throw std::runtime_error(
                "Decryption failed: authentication tag mismatch"
            );
        }

        plaintext_len += len;
        plaintext.resize(plaintext_len);

        return plaintext;
    }

}
