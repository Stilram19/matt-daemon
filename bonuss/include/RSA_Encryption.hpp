#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <vector>
#include <string>
#include <stdexcept>


namespace rsa {
    
    class RSA_Encryption {
    private:
        EVP_PKEY* pkey = nullptr;
        EVP_PKEY* publicKey = nullptr;
    
    public:
        RSA_Encryption() {}
        RSA_Encryption(const RSA_Encryption &) = delete;
        RSA_Encryption &operator=(const RSA_Encryption &) = delete;
    
        ~RSA_Encryption() {
            if (pkey)
                EVP_PKEY_free(pkey);
            if (publicKey)
                EVP_PKEY_free(publicKey);
        }
    
        void generateKeys(int keySize) {
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
            if (!ctx) throw std::runtime_error("Context creation failed");
    
            if (EVP_PKEY_keygen_init(ctx) <= 0)
                throw std::runtime_error("Keygen init failed");
    
            if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, keySize) <= 0)
                throw std::runtime_error("Setting key size failed");
    
            if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
                throw std::runtime_error("Key generation failed");
    
            EVP_PKEY_CTX_free(ctx);
        }
    
        std::string getPublicKeyPEM() const {
            BIO* bio = BIO_new(BIO_s_mem());
            PEM_write_bio_PUBKEY(bio, pkey);
    
            char* data;
            long len = BIO_get_mem_data(bio, &data);
            std::string key(data, len);
    
            BIO_free(bio);
            return key;
        }
    
        std::vector<unsigned char> decrypt(const std::vector<unsigned char>& ciphertext) {
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
            if (!ctx) throw std::runtime_error("Decrypt context failed");

            if (EVP_PKEY_decrypt_init(ctx) <= 0)
                throw std::runtime_error("Decrypt init failed");

            if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
                throw std::runtime_error("Padding failed");

            size_t outLen;

            if (EVP_PKEY_decrypt(ctx, nullptr, &outLen,
                                ciphertext.data(), ciphertext.size()) <= 0)
                throw std::runtime_error("Decrypt size calc failed");

            std::vector<unsigned char> plaintext(outLen);

            if (EVP_PKEY_decrypt(ctx, plaintext.data(), &outLen,
                                ciphertext.data(), ciphertext.size()) <= 0)
                throw std::runtime_error("Decrypt failed");

            EVP_PKEY_CTX_free(ctx);

            plaintext.resize(outLen);   

            return plaintext;
        }
    
    
        void loadPublicKey(const std::string& publicKeyPEM) {
            BIO* bio = BIO_new_mem_buf(publicKeyPEM.data(), publicKeyPEM.size());
            if (!bio)
                throw std::runtime_error("BIO creation failed");
    
            publicKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
    
            if (!publicKey)
                throw std::runtime_error("Public key loading failed");
        }
    
        std::vector<unsigned char> encrypt(const std::string& plaintext) {
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(publicKey, nullptr);
            if (!ctx)
                throw std::runtime_error("Encrypt context failed");
    
            if (EVP_PKEY_encrypt_init(ctx) <= 0)
                throw std::runtime_error("Encrypt init failed");
    
            if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
                throw std::runtime_error("Padding failed");
    
            size_t outLen;
    
            if (EVP_PKEY_encrypt(ctx, nullptr, &outLen,
                                 reinterpret_cast<const unsigned char*>(plaintext.data()),
                                 plaintext.size()) <= 0)
                throw std::runtime_error("Encrypt size calc failed");
    
            std::vector<unsigned char> ciphertext(outLen);
    
            if (EVP_PKEY_encrypt(ctx, ciphertext.data(), &outLen,
                                 reinterpret_cast<const unsigned char*>(plaintext.data()),
                                 plaintext.size()) <= 0)
                throw std::runtime_error("Encryption failed");
    
            EVP_PKEY_CTX_free(ctx);
    
            ciphertext.resize(outLen);
            return ciphertext;
        }
    
    };

}



