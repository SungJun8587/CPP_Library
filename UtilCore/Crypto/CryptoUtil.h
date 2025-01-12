
//***************************************************************************
// CryptoUtil.h : interface for the CCryptoUtil class.
//
//***************************************************************************

#ifndef __CRYPTOUTIL_H__
#define __CRYPTOUTIL_H__

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

#ifndef	__BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

namespace Crypto
{
	class CCryptoUtil
	{
		public:
			CCryptoUtil(const std::string& key, const std::string& iv);
			~CCryptoUtil() {};

			bool IsSupported(string name);

			std::string HashMD5(const std::string& data) const;

			std::string EncryptAES(const std::string& plaintext) const;
			std::string DecryptAES(const std::string& ciphertext) const;

			std::string EncryptSEED(const std::string& plaintext) const;
			std::string DecryptSEED(const std::string& ciphertext) const;

			std::string EncryptAESGCM(const std::string& plaintext, std::string& tag) const;
			std::string DecryptAESGCM(const std::string& ciphertext, std::string& tag) const;

		public:
			//***************************************************************************
			// 해시 알고리즘 지원 여부 확인(MD5, SHA256, SHA512, SHA1, RIPEMD160
			static bool IsHashAlgorithmSupported(const std::string& algorithm)
			{
				const EVP_MD* md = EVP_get_digestbyname(algorithm.c_str());
				return md != nullptr;
			}

			//***************************************************************************
			// 암호화 알고리즘 지원 여부 확인(AES-128-CBC, AES-192-CBC, AES-256-CBC, SEED-CBC, AES-128-GCM, AES-256-GCM)
			static bool IsCipherAlgorithmSupported(const std::string& algorithm)
			{
				const EVP_CIPHER* cipher = EVP_get_cipherbyname(algorithm.c_str());
				return cipher != nullptr;
			}

		private:
			//***************************************************************************
			// EVP 기반 암호화 함수
			static std::string encryptEVP(const std::string& plaintext, const std::string& key, const std::string& iv, const EVP_CIPHER* cipher)
			{
				std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
				int len = 0, ciphertext_len = 0;

				EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
				if( !ctx ) throw std::runtime_error("Failed to create EVP_CIPHER_CTX");

				if( EVP_EncryptInit_ex(ctx, cipher, nullptr,
					reinterpret_cast<const unsigned char*>(key.data()),
					reinterpret_cast<const unsigned char*>(iv.data())) != 1 )
				{
					EVP_CIPHER_CTX_free(ctx);
					throw std::runtime_error("Failed to initialize encryption");
				}

				if( EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
					reinterpret_cast<const unsigned char*>(plaintext.data()), 
					static_cast<int>(plaintext.size())) != 1 )
				{
					EVP_CIPHER_CTX_free(ctx);
					throw std::runtime_error("Failed to encrypt data");
				}
				ciphertext_len = len;

				if( EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1 )
				{
					EVP_CIPHER_CTX_free(ctx);
					throw std::runtime_error("Failed to finalize encryption");
				}
				ciphertext_len += len;

				EVP_CIPHER_CTX_free(ctx);
				return std::string(ciphertext.begin(), ciphertext.begin() + ciphertext_len);
			}

			//***************************************************************************
			// EVP 기반 복호화 함수
			static std::string decryptEVP(const std::string& ciphertext, const std::string& key, const std::string& iv, const EVP_CIPHER* cipher)
			{
				std::vector<unsigned char> plaintext(ciphertext.size());
				int len = 0, plaintext_len = 0;

				EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
				if( !ctx ) throw std::runtime_error("Failed to create EVP_CIPHER_CTX");

				if( EVP_DecryptInit_ex(ctx, cipher, nullptr,
					reinterpret_cast<const unsigned char*>(key.data()),
					reinterpret_cast<const unsigned char*>(iv.data())) != 1 )
				{
					EVP_CIPHER_CTX_free(ctx);
					throw std::runtime_error("Failed to initialize decryption");
				}

				if( EVP_DecryptUpdate(ctx, plaintext.data(), &len,
					reinterpret_cast<const unsigned char*>(ciphertext.data()), 
					static_cast<int>(ciphertext.size())) != 1 )
				{
					EVP_CIPHER_CTX_free(ctx);
					throw std::runtime_error("Failed to decrypt data");
				}
				plaintext_len = len;

				if( EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1 )
				{
					EVP_CIPHER_CTX_free(ctx);
					throw std::runtime_error("Failed to finalize decryption");
				}
				plaintext_len += len;

				EVP_CIPHER_CTX_free(ctx);
				return std::string(plaintext.begin(), plaintext.begin() + plaintext_len);
			}

			//***************************************************************************
			// AES-GCM 암호화/복호화 함수
			static std::string encryptEVP_AESGCM(const std::string& input, const std::string& key, const std::string& iv, std::string& tag, bool encrypt)
			{
				std::vector<unsigned char> output(input.size() + EVP_MAX_BLOCK_LENGTH);
				int len = 0;
				int output_len = 0;

				EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
				if( !ctx ) throw std::runtime_error("Failed to create EVP_CIPHER_CTX");

				if( encrypt )
				{
					if( EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
						reinterpret_cast<const unsigned char*>(key.data()),
						reinterpret_cast<const unsigned char*>(iv.data())) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to initialize encryption");
					}

					if( EVP_EncryptUpdate(ctx, output.data(), &len,
						reinterpret_cast<const unsigned char*>(input.data()), 
						static_cast<int>(input.size())) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to encrypt data");
					}
					output_len = len;

					if( EVP_EncryptFinal_ex(ctx, output.data() + len, &len) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to finalize encryption");
					}
					output_len += len;

					std::vector<unsigned char> tag_buf(16);
					if( EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag_buf.data()) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to get GCM tag");
					}
					tag.assign(reinterpret_cast<char*>(tag_buf.data()), tag_buf.size());
				}
				else
				{
					if( EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
						reinterpret_cast<const unsigned char*>(key.data()),
						reinterpret_cast<const unsigned char*>(iv.data())) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to initialize decryption");
					}

					if( EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
						const_cast<char*>(tag.data())) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to set GCM tag");
					}

					if( EVP_DecryptUpdate(ctx, output.data(), &len,
						reinterpret_cast<const unsigned char*>(input.data()), 
						static_cast<int>(input.size())) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to decrypt data");
					}
					output_len = len;

					if( EVP_DecryptFinal_ex(ctx, output.data() + len, &len) != 1 )
					{
						EVP_CIPHER_CTX_free(ctx);
						throw std::runtime_error("Failed to finalize decryption");
					}
					output_len += len;
				}

				EVP_CIPHER_CTX_free(ctx);
				return std::string(output.begin(), output.begin() + output_len);
			}

			//***************************************************************************
			// 바이너리 데이터를 16진수 문자열로 변환
			static std::string toHex(const unsigned char* data, size_t length)
			{
				std::string result;
				for( size_t i = 0; i < length; ++i )
				{
					char buf[3];
					snprintf(buf, sizeof(buf), "%02x", data[i]);
					result += buf;
				}
				return result;
			}

		private:
			std::string _key;  // AES, SEED, AES-GCM 키
			std::string _iv;   // AES, SEED, AES-GCM IV
	};
}

#endif // ndef __CRYPTOUTIL_H__
