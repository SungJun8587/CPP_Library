
//***************************************************************************
// CryptoUtil.h : interface for the CCryptoUtil class.
//
//***************************************************************************

#ifndef UC_CRYPTOUTIL_H
#define UC_CRYPTOUTIL_H

#include <BaseRedefineDataType.h>
#include <BaseMacro.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

#pragma comment(lib, LIB_NAME("libcrypto"))
#pragma comment(lib, LIB_NAME("libssl"))

namespace Crypto
{
	//***************************************************************************
	// @brief 암호화, 복호화 및 해시 기능을 제공하는 유틸리티 클래스입니다.
	// @detail OpenSSL 라이브러리를 기반으로 MD5/SHA-256 해시, AES 및 SEED 대칭 암호화,
	//         AES-GCM 인증 암호화, CSPRNG 난수 생성 기능을 수행합니다.
	//***************************************************************************
	class CCryptoUtil
	{
	public:
		CCryptoUtil(const std::string& key, const std::string& iv);
		~CCryptoUtil() {};

		std::string HashMD5(const std::string& data) const;

		std::string EncryptAES(const std::string& plaintext) const;
		std::string DecryptAES(const std::string& ciphertext) const;

		std::string EncryptSEED(const std::string& plaintext) const;
		std::string DecryptSEED(const std::string& ciphertext) const;

		std::string EncryptAESGCM(const std::string& plaintext, std::string& tag) const;
		std::string DecryptAESGCM(const std::string& ciphertext, std::string& tag) const;

	public:
		//***************************************************************************
		// @brief 해시 알고리즘 지원 여부를 확인합니다.
		// @detail 지정된 해시 알고리즘 이름(MD5, SHA256, SHA512, SHA1, RIPEMD160 등)이 유효한지 확인합니다.
		// @param algorithm 확인할 해시 알고리즘 이름
		// @return 지원하는 경우 true, 그렇지 않은 경우 false
		//***************************************************************************
		static bool IsHashAlgorithmSupported(const std::string& algorithm)
		{
			const EVP_MD* md = EVP_get_digestbyname(algorithm.c_str());
			return md != nullptr;
		}

		//***************************************************************************
		// @brief 암호화 알고리즘 지원 여부를 확인합니다.
		// @detail 지정된 암호화 알고리즘 이름(AES-128-CBC, SEED-CBC, AES-256-GCM 등)이 유효한지 확인합니다.
		// @param algorithm 확인할 암호화 알고리즘 이름
		// @return 지원하는 경우 true, 그렇지 않은 경우 false
		//***************************************************************************
		static bool IsCipherAlgorithmSupported(const std::string& algorithm)
		{
			const EVP_CIPHER* cipher = EVP_get_cipherbyname(algorithm.c_str());
			return cipher != nullptr;
		}

		//***************************************************************************
		// @brief [신규] 암호학적으로 안전한 난수 바이트를 생성합니다(OpenSSL RAND_bytes).
		// @detail 세션 토큰, salt 등 보안 목적의 난수 생성 전용 — rand()/std::mt19937
		//         등 일반 난수 생성기와 절대 혼용하지 말 것.
		// @return 성공 시 true. RAND_bytes()가 실패(엔트로피 소스 문제 등)하면 false.
		//***************************************************************************
		static bool GenerateRandomBytes(unsigned char* out, size_t len)
		{
			return ::RAND_bytes(out, static_cast<int>(len)) == 1;
		}

		//***************************************************************************
		// @brief [신규] SHA-256 해시를 계산합니다.
		// @detail HashMD5()와 달리 static으로 뒀다 — SHA-256 자체는 대칭 키/IV가
		//         필요 없는 순수 해시 연산이라, 이 하나를 쓰겠다고 키/IV로
		//         인스턴스를 만드는 게 더 부자연스럽다고 판단했다(HashMD5와의
		//         이런 비대칭은 의도적 — 필요하면 HashMD5도 static으로 통일하는
		//         걸 검토해볼 수 있음).
		// @return 16진수 소문자 문자열(64자)로 인코딩된 SHA-256 해시.
		//***************************************************************************
		static std::string HashSHA256(const std::string& data)
		{
			return hashWith(data, EVP_sha256());
		}

		//***************************************************************************
		// @brief [신규] 바이너리 데이터를 16진수 문자열로 변환합니다(외부 공개용).
		//***************************************************************************
		static std::string ToHex(const unsigned char* data, size_t length)
		{
			return toHex(data, length);
		}

		//***************************************************************************
		// @brief [신규] 16진수 문자열을 바이너리 데이터로 되돌립니다.
		// @param outLen out에 쓸 바이트 수 — hex.size()가 outLen*2가 아니거나
		//        16진수가 아닌 문자가 섞여 있으면 false(이 경우 out은 사용하지 말 것).
		//***************************************************************************
		static bool FromHex(const std::string& hex, unsigned char* out, size_t outLen)
		{
			if( hex.size() != outLen * 2 )
				return false;

			auto hexVal = [](char c) -> int
				{
					if( c >= '0' && c <= '9' ) return c - '0';
					if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
					if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
					return -1;
				};

			for( size_t i = 0; i < outLen; ++i )
			{
				const int hi = hexVal(hex[i * 2]);
				const int lo = hexVal(hex[i * 2 + 1]);
				if( hi < 0 || lo < 0 )
					return false;

				out[i] = static_cast<unsigned char>((hi << 4) | lo);
			}
			return true;
		}

		//***************************************************************************
		// @brief [신규] 타이밍 사이드채널을 피하기 위한 상수시간 바이트 비교.
		// @detail 불일치를 찾는 즉시 반환하지 않고 항상 len바이트 전부를
		//         XOR-누적한 뒤 마지막에 판정 — 토큰/해시 비교 등 보안이
		//         중요한 값 비교에 사용할 것(문자열 비교 연산자 사용 금지).
		//***************************************************************************
		static bool ConstantTimeEquals(const unsigned char* a, const unsigned char* b, size_t len)
		{
			unsigned char diff = 0;
			for( size_t i = 0; i < len; ++i )
				diff |= static_cast<unsigned char>(a[i] ^ b[i]);
			return diff == 0;
		}

	private:
		//***************************************************************************
		// @brief [신규] EVP_MD_CTX 기반 범용 해시 계산 — HashMD5()/HashSHA256()이
		//        공유하는 내부 구현. 기존 HashMD5()의 인라인 구현을 그대로
		//        옮겨온 것으로, HashMD5()의 동작/시그니처는 전혀 바뀌지 않는다.
		//***************************************************************************
		static std::string hashWith(const std::string& data, const EVP_MD* md)
		{
			EVP_MD_CTX* ctx = EVP_MD_CTX_new();
			if( !ctx )
			{
				throw std::runtime_error("Failed to create EVP_MD_CTX");
			}

			unsigned char hash[EVP_MAX_MD_SIZE];
			unsigned int hash_len = 0;

			if( EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
				EVP_DigestUpdate(ctx, data.c_str(), data.size()) != 1 ||
				EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1 )
			{
				EVP_MD_CTX_free(ctx);
				throw std::runtime_error("Failed to compute hash");
			}

			EVP_MD_CTX_free(ctx);
			return toHex(hash, hash_len);
		}

		//***************************************************************************
		// @brief EVP 인터페이스를 기반으로 데이터를 암호화합니다.
		// @detail 제공된 대칭 키, IV 및 암호화 방식을 사용하여 평문을 암호문으로 변환합니다.
		// @param plaintext 암호화할 평문 데이터
		// @param key 암호화 키
		// @param iv 초기화 벡터 (IV)
		// @param cipher 사용할 EVP_CIPHER 알고리즘 객체
		// @return 암호화된 문자열 데이터
		//***************************************************************************
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
		// @brief EVP 인터페이스를 기반으로 데이터를 복호화합니다.
		// @detail 제공된 대칭 키, IV 및 암호화 방식을 사용하여 암호문을 평문으로 변환합니다.
		// @param ciphertext 복호화할 암호문 데이터
		// @param key 복호화 키
		// @param iv 초기화 벡터 (IV)
		// @param cipher 사용할 EVP_CIPHER 알고리즘 객체
		// @return 복호화된 평문 문자열 데이터
		//***************************************************************************
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
		// @brief AES-GCM 방식을 이용해 데이터를 암호화 또는 복호화합니다.
		// @detail 인증 암호화(AEAD) 모드인 AES-GCM을 사용하여 데이터를 처리하고 인증 태그(Tag)를 생성하거나 검증합니다.
		// @param input 처리할 입력 데이터 (평문 또는 암호문)
		// @param key 암호화/복호화 키
		// @param iv 초기화 벡터 (IV)
		// @param tag 인증 태그 (참조자, 암호화 시 생성되고 복호화 시 사용됨)
		// @param encrypt 암호화 수행 여부 (true: 암호화, false: 복호화)
		// @return 처리된 결과 문자열 데이터
		//***************************************************************************
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
		// @brief 바이너리 데이터를 16진수 문자열로 변환합니다.
		// @detail 입력받은 바이트 배열을 두 자리 16진수 형식의 문자열로 변환합니다.
		// @param data 변환할 바이너리 데이터 포인터
		// @param length 데이터의 길이
		// @return 변환된 16진수 문자열
		//***************************************************************************
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
		std::string _key;  // 암호화/복호화에 사용되는 대칭 키 (AES, SEED, AES-GCM)
		std::string _iv;   // 암호화/복호화에 사용되는 초기화 벡터 (AES, SEED, AES-GCM)
	};
}

#endif // ndef UC_CRYPTOUTIL_H