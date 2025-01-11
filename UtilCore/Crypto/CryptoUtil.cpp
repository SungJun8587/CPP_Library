
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

	CCryptoUtil::CCryptoUtil(const std::string& key, const std::string& iv)
		: _key(key), _iv(iv)
	{
		if( _key.empty() || _iv.empty() )
		{
			throw std::invalid_argument("Key and IV must not be empty");
		}
	}

	//***************************************************************************
	// MD5 해싱
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
	// AES-256-CBC 암호화
	std::string CCryptoUtil::EncryptAES(const std::string& plaintext) const
	{
		return encryptEVP(plaintext, _key, _iv, EVP_aes_256_cbc());
	}

	//***************************************************************************
	// AES-256-CBC 복호화
	std::string CCryptoUtil::DecryptAES(const std::string& ciphertext) const
	{
		return decryptEVP(ciphertext, _key, _iv, EVP_aes_256_cbc());
	}

	//***************************************************************************
	// SEED 암호화
	std::string CCryptoUtil::EncryptSEED(const std::string& plaintext) const
	{
		return encryptEVP(plaintext, _key, _iv, EVP_seed_cbc());
	}

	//***************************************************************************
	// SEED 복호화
	std::string CCryptoUtil::DecryptSEED(const std::string& ciphertext) const
	{
		return decryptEVP(ciphertext, _key, _iv, EVP_seed_cbc());
	}

	//***************************************************************************
	// AES-GCM 암호화
	std::string CCryptoUtil::EncryptAESGCM(const std::string& plaintext, std::string& tag) const
	{
		return encryptEVP_AESGCM(plaintext, _key, _iv, tag, true);
	}

	//***************************************************************************
	// AES-GCM 복호화
	std::string CCryptoUtil::DecryptAESGCM(const std::string& ciphertext, std::string& tag) const
	{
		return encryptEVP_AESGCM(ciphertext, _key, _iv, tag, false);
	}
}