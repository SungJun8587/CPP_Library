
//***************************************************************************
// CryptoUtil.cpp: implementation of the CCryptoUtil class.
//
//***************************************************************************

#include "pch.h"
#include "CryptoUtil.h"

namespace Crypto
{
	//***************************************************************************
	// Construction/Destruction 
	//***************************************************************************

	//***************************************************************************
	// @brief CCryptoUtil 클래스의 생성자입니다.
	// @detail 전달받은 대칭 키와 초기화 벡터(IV)를 검증하고 멤버 변수로 초기화합니다.
	// @param key 암호화/복호화에 사용할 대칭 키
	// @param iv 초기화 벡터 (IV)
	//***************************************************************************
	CCryptoUtil::CCryptoUtil(const std::string& key, const std::string& iv)
		: _key(key), _iv(iv)
	{
		if( _key.empty() || _iv.empty() )
		{
			throw std::invalid_argument("Key and IV must not be empty");
		}
	}

	//***************************************************************************
	// @brief 입력된 데이터에 대한 MD5 해시 값을 계산합니다.
	// @detail OpenSSL의 EVP 인터페이스를 사용하여 MD5 해시를 생성하고, 결과를 16진수 문자열로 반환합니다.
	// @param data 해시를 생성할 원본 문자열 데이터
	// @return 생성된 MD5 해시의 16진수 문자열
	//***************************************************************************
	std::string CCryptoUtil::HashMD5(const std::string& data) const
	{
		EVP_MD_CTX* ctx = EVP_MD_CTX_new();
		if( !ctx )
		{
			throw std::runtime_error("Failed to create EVP_MD_CTX");
		}

		unsigned char hash[EVP_MAX_MD_SIZE];
		unsigned int hash_len = 0;

		if( EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1 ||
			EVP_DigestUpdate(ctx, data.c_str(), data.size()) != 1 ||
			EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1 )
		{
			EVP_MD_CTX_free(ctx);
			throw std::runtime_error("Failed to compute MD5 hash");
		}

		EVP_MD_CTX_free(ctx);
		return toHex(hash, hash_len);
	}

	//***************************************************************************
	// @brief AES-256-CBC 방식을 이용해 평문을 암호화합니다.
	// @detail 내부적으로 정의된 키와 IV, 그리고 AES-256-CBC 알고리즘을 사용하여 평문을 암호화합니다.
	// @param plaintext 암호화할 평문 문자열
	// @return 암호화된 문자열 데이터
	//***************************************************************************
	std::string CCryptoUtil::EncryptAES(const std::string& plaintext) const
	{
		return encryptEVP(plaintext, _key, _iv, EVP_aes_256_cbc());
	}

	//***************************************************************************
	// @brief AES-256-CBC 방식을 이용해 암호문을 복호화합니다.
	// @detail 내부적으로 정의된 키와 IV, 그리고 AES-256-CBC 알고리즘을 사용하여 암호문을 평문으로 되돌립니다.
	// @param ciphertext 복호화할 암호문 문자열
	// @return 복호화된 평문 문자열 데이터
	//***************************************************************************
	std::string CCryptoUtil::DecryptAES(const std::string& ciphertext) const
	{
		return decryptEVP(ciphertext, _key, _iv, EVP_aes_256_cbc());
	}

	//***************************************************************************
	// @brief SEED 방식을 이용해 평문을 암호화합니다.
	// @detail 내부적으로 정의된 키와 IV, 그리고 SEED-CBC 알고리즘을 사용하여 평문을 암호화합니다.
	// @param plaintext 암호화할 평문 문자열
	// @return 암호화된 문자열 데이터
	//***************************************************************************
	std::string CCryptoUtil::EncryptSEED(const std::string& plaintext) const
	{
		return encryptEVP(plaintext, _key, _iv, EVP_seed_cbc());
	}

	//***************************************************************************
	// @brief SEED 방식을 이용해 암호문을 복호화합니다.
	// @detail 내부적으로 정의된 키와 IV, 그리고 SEED-CBC 알고리즘을 사용하여 암호문을 평문으로 되돌립니다.
	// @param ciphertext 복호화할 암호문 문자열
	// @return 복호화된 평문 문자열 데이터
	//***************************************************************************
	std::string CCryptoUtil::DecryptSEED(const std::string& ciphertext) const
	{
		return decryptEVP(ciphertext, _key, _iv, EVP_seed_cbc());
	}

	//***************************************************************************
	// @brief AES-GCM 방식을 이용해 평문을 암호화합니다.
	// @detail 인증 암호화(AEAD) 모드인 AES-GCM을 사용하여 평문을 암호화하고 인증 태그(Tag)를 생성합니다.
	// @param plaintext 암호화할 평문 문자열
	// @param tag [out] 암호화 과정에서 생성되는 GCM 인증 태그 (참조자)
	// @return 암호화된 문자열 데이터
	//***************************************************************************
	std::string CCryptoUtil::EncryptAESGCM(const std::string& plaintext, std::string& tag) const
	{
		return encryptEVP_AESGCM(plaintext, _key, _iv, tag, true);
	}

	//***************************************************************************
	// @brief AES-GCM 방식을 이용해 암호문을 복호화합니다.
	// @detail 인증 암호화(AEAD) 모드인 AES-GCM을 사용하여 인증 태그(Tag)를 검증하고 암호문을 복호화합니다.
	// @param ciphertext 복호화할 암호문 문자열
	// @param tag [in/out] 복호화 시 검증에 사용할 GCM 인증 태그 (참조자)
	// @return 복호화된 평문 문자열 데이터
	//***************************************************************************
	std::string CCryptoUtil::DecryptAESGCM(const std::string& ciphertext, std::string& tag) const
	{
		return encryptEVP_AESGCM(ciphertext, _key, _iv, tag, false);
	}
}