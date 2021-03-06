#include "cryptoutility.hpp"

#include "words.hpp"

#include <mesh.pp/cryptopp_byte.hpp>

#include <cryptopp/eccrypto.h>
#include <cryptopp/ecp.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/ripemd.h>
#include <cryptopp/sha.h>
#include <cryptopp/base64.h>
#include <cryptopp/dsa.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

#include <string>
#include <cassert>
#include <vector>
#include <random>
#include <cstring>

using std::string;
using std::vector;
using std::runtime_error;

string g_public_key_prefix = string();

namespace meshpp
{

namespace detail
{
bool wif_to_sk(const std::string & wif_str, CryptoPP::Integer& sk);
std::string normalize_brain_key(const std::string & bk);
std::string suggest_brain_key();
string bk_to_wif_sk(string const& bk_str, int sequence_number);
string pk_to_base58(std::string const& key);
bool base58_to_pk_hex(string const& b58_str, string& pk);
bool base58_to_pk(string const& b58_str, CryptoPP::ECP::Point &pk);
string ECPoint_to_zstr(const CryptoPP::OID &oid, const CryptoPP::ECP::Point &P, bool compressed = true);
CryptoPP::ECP::Point zstr_to_ECPoint(const CryptoPP::OID &oid, const std::string& z_str);
std::string compress_pk(const CryptoPP::OID &oid, std::string str);
std::string decompress_pk(const CryptoPP::OID &oid, std::string str);
string hash(const string & message);
vector<unsigned char> from_base58(std::string const & data);
}

void config::set_public_key_prefix(std::string const& prefix)
{
    g_public_key_prefix = prefix;
}

std::string config::public_key_prefix()
{
    return g_public_key_prefix;
}

random_seed::random_seed(string const& brain_key_)
    : brain_key(brain_key_)
{
    if (brain_key.empty())
        brain_key = detail::suggest_brain_key();
    else
        brain_key = detail::normalize_brain_key(brain_key);
}

random_seed::~random_seed()
{}

string random_seed::get_brain_key() const
{
    return brain_key;
}

private_key random_seed::get_private_key(uint64_t index) const
{
    return private_key(detail::bk_to_wif_sk(brain_key, int(index)));
}

private_key::private_key(string const& base58_wif_)
    : base58_wif(base58_wif_)
{
    CryptoPP::Integer sk;
    if (false == detail::wif_to_sk(base58_wif, sk))
        throw exception_private_key(base58_wif);
}

private_key::private_key(private_key const& other)
    : base58_wif(other.base58_wif)
{}

private_key::~private_key()
{}

string private_key::get_base58_wif() const
{
    return base58_wif;
}

public_key private_key::get_public_key() const
{
    CryptoPP::Integer sk;
    detail::wif_to_sk(base58_wif, sk);

    auto secp256k1 = CryptoPP::ASN1::secp256k1();
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey pv_key;
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey pb_key;
    pv_key.Initialize(secp256k1, sk);
    pv_key.MakePublicKey(pb_key);

    auto pk_i = pb_key.GetPublicElement();

    auto pk_b58 = detail::pk_to_base58(detail::ECPoint_to_zstr(secp256k1, pk_i));

    return public_key(g_public_key_prefix + pk_b58);
}

signature private_key::sign(std::string const& message) const
{
    CryptoPP::Integer sk;
    detail::wif_to_sk(base58_wif, sk);

    auto secp256k1 = CryptoPP::ASN1::secp256k1();
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey pv_key;
    pv_key.Initialize(secp256k1, sk);

    // sign message

    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Signer signer(pv_key);

    CryptoPP::AutoSeededRandomPool rng;

    std::string message_str(message.begin(), message.end());
    std::string signature_raw;
    CryptoPP::StringSource ss(message_str, true,
                              new CryptoPP::SignerFilter(rng, signer, new CryptoPP::StringSink(signature_raw)));

    string signature_der(signature_raw.size()*2, '\x00');
    auto sz_ = CryptoPP::DSAConvertSignatureFormat(
                (CryptoPP::byte*) signature_der.data(), signature_der.size(), CryptoPP::DSA_DER,
                (const CryptoPP::byte*)signature_raw.data(), signature_raw.size(), CryptoPP::DSA_P1363
                );
    signature_der.resize(sz_);

    string signature_der_b58 = to_base58(signature_der);

    return signature(get_public_key(), message, signature_der_b58);
}

std::string private_key::decrypt(std::string msg) const
{
    static const size_t HMAC_KEY_SIZE = 32;
    static const size_t COMPRESSED_PUBLIC_KEY_SIZE = 33;
    static const size_t HMAC_SHA256_SIZE = 32;
    static const size_t AES_KEY_SIZE = 32;
    static const size_t AES_BLOCK_SIZE = 16;

    if (msg.size() < COMPRESSED_PUBLIC_KEY_SIZE + HMAC_SHA256_SIZE + AES_BLOCK_SIZE)
        throw std::runtime_error("Wrong encrypted message");

    auto secp256k1 = CryptoPP::ASN1::secp256k1();
    auto ecgp = CryptoPP::DL_GroupParameters_EC<CryptoPP::ECP>(secp256k1);

    CryptoPP::ECDH<CryptoPP::ECP, CryptoPP::NoCofactorMultiplication>::Domain dh(ecgp);
    
    CryptoPP::SecByteBlock sk(dh.PrivateKeyLength());
    CryptoPP::SecByteBlock shared_value(dh.AgreedValueLength());
    CryptoPP::Integer sk_i;

    detail::wif_to_sk(base58_wif, sk_i);
    sk_i.Encode(sk, sk.size());

    auto pk_str = detail::decompress_pk(secp256k1, std::string(msg.begin(), msg.begin() + COMPRESSED_PUBLIC_KEY_SIZE));

    if (! dh.Agree(shared_value, sk, (CryptoPP::byte*)pk_str.data(), true))
        throw std::runtime_error("ECDH stage failed");

    char i1[] = {0, 0, 0, 1} , i2[] = {0, 0, 0, 2}, i3[] = {0, 0, 0, 3};
    std::string _i1(i1, 4), _i2(i2, 4), _i3(i3, 4);

    auto shared_value_str = std::string(reinterpret_cast<std::string::value_type *>(&shared_value[0]), shared_value.size());
    auto derived_key = detail::hash(shared_value_str + _i1) + detail::hash(shared_value_str + _i2) + detail::hash(shared_value_str + _i3);

    auto enc_key = derived_key.substr(0, AES_KEY_SIZE);
    auto enc_iv = derived_key.substr(AES_KEY_SIZE, AES_BLOCK_SIZE);
    auto mac_key = derived_key.substr(AES_KEY_SIZE + AES_BLOCK_SIZE, HMAC_KEY_SIZE);

    CryptoPP::CBC_Mode< CryptoPP::AES >::Decryption aes_dec;

    std::string cipher(msg.begin() + COMPRESSED_PUBLIC_KEY_SIZE + HMAC_SHA256_SIZE, msg.end());
    
    CryptoPP::HMAC<CryptoPP::SHA256> hmac((CryptoPP::byte*)mac_key.data(), mac_key.size());
    const int flags = CryptoPP::HashVerificationFilter::THROW_EXCEPTION | CryptoPP::HashVerificationFilter::HASH_AT_BEGIN; 
    
    CryptoPP::StringSource(msg.substr(COMPRESSED_PUBLIC_KEY_SIZE), true, new CryptoPP::HashVerificationFilter(hmac, NULL, flags)); 

    aes_dec.SetKeyWithIV((CryptoPP::byte*)enc_key.data(), enc_key.size(), (CryptoPP::byte*)enc_iv.data(), enc_iv.size());
 
    std::string result;
    CryptoPP::StringSource(cipher, true, new CryptoPP::StreamTransformationFilter(aes_dec, new CryptoPP::StringSink(result)));

    return result;    
}

std::string public_key::encrypt(std::string msg) const
{
    static const size_t HMAC_KEY_SIZE = 32;
    static const size_t AES_KEY_SIZE = 32;
    static const size_t AES_BLOCK_SIZE = 16;

    CryptoPP::AutoSeededRandomPool rng;
    auto secp256k1 = CryptoPP::ASN1::secp256k1();
    auto ecgp = CryptoPP::DL_GroupParameters_EC<CryptoPP::ECP>(secp256k1);

    CryptoPP::ECDH<CryptoPP::ECP, CryptoPP::NoCofactorMultiplication>::Domain dh(ecgp);
    
    CryptoPP::SecByteBlock e_sk(dh.PrivateKeyLength());
    CryptoPP::SecByteBlock e_pk(dh.PublicKeyLength());
    CryptoPP::SecByteBlock shared_value(dh.AgreedValueLength());
    dh.GenerateKeyPair(rng, e_sk, e_pk);

    auto pk_v = detail::from_base58(str_base58);
    auto pk_str = detail::decompress_pk(secp256k1, std::string(pk_v.begin(), pk_v.end()-4));

    if (! dh.Agree(shared_value, e_sk, (CryptoPP::byte*)pk_str.data(), true))
        throw std::runtime_error("ECDH stage failed");

    char i1[] = {0, 0, 0, 1} , i2[] = {0, 0, 0, 2}, i3[] = {0, 0, 0, 3};
    std::string _i1(i1, 4), _i2(i2, 4), _i3(i3, 4);

    auto shared_value_str = std::string(reinterpret_cast<std::string::value_type *>(&shared_value[0]), shared_value.size());
    auto derived_key = detail::hash(shared_value_str + _i1) + detail::hash(shared_value_str + _i2) + detail::hash(shared_value_str + _i3);

    auto enc_key = derived_key.substr(0, AES_KEY_SIZE);
    auto enc_iv = derived_key.substr(AES_KEY_SIZE, AES_BLOCK_SIZE);
    auto mac_key = derived_key.substr(AES_KEY_SIZE + AES_BLOCK_SIZE, HMAC_KEY_SIZE);

    CryptoPP::CBC_Mode< CryptoPP::AES >::Encryption aec_enc;
    std::string cipher, mac;

    aec_enc.SetKeyWithIV((CryptoPP::byte*)enc_key.data(), enc_key.size(), (CryptoPP::byte*)enc_iv.data(), enc_iv.size());
 
    CryptoPP::StringSource(msg, true, new CryptoPP::StreamTransformationFilter(aec_enc, new CryptoPP::StringSink(cipher)));

    CryptoPP::HMAC<CryptoPP::SHA256> hmac((CryptoPP::byte*)mac_key.data(), mac_key.size());
    CryptoPP::StringSource(cipher, true, new CryptoPP::HashFilter(hmac, new CryptoPP::StringSink(mac)));

    std::string result(e_pk.begin(), e_pk.end());

    result = detail::compress_pk(secp256k1, result);
    result += mac;
    result += cipher;

    return result;    
}

public_key::public_key(string const& str_base58_)
    : str_base58(str_base58_)
    //, raw_base58()
{
    if (false == g_public_key_prefix.empty() &&
        0 != str_base58.find(g_public_key_prefix))
        throw exception_public_key(str_base58);

    str_base58.erase(0, g_public_key_prefix.length());

    CryptoPP::ECP::Point pk;
    if (false == detail::base58_to_pk(str_base58, pk))
        throw exception_public_key(g_public_key_prefix + str_base58);

    auto secp256k1 = CryptoPP::ASN1::secp256k1();

    if (detail::pk_to_base58(detail::ECPoint_to_zstr(secp256k1, pk)) != str_base58)
        throw exception_public_key(g_public_key_prefix + str_base58);
}

public_key::public_key(public_key const& other)
    : str_base58(other.str_base58)
    //, raw_base58(other.raw_base58)
{

}

public_key::~public_key() {}

string public_key::to_string() const
{
    return g_public_key_prefix + str_base58;
}

string public_key::get_base58() const
{
    return str_base58;
}

bool verify_signature(public_key const& pb_key,
                      std::string const& message,
                      std::string const& signature_b58)
{
    try {
        auto secp256k1 = CryptoPP::ASN1::secp256k1();

        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey pub_key;
        string pub_key_hex;

        // decode base58 signature
        auto signature_der = detail::from_base58(signature_b58);

        // verify message
        detail::base58_to_pk_hex(pb_key.get_base58(), pub_key_hex);

        const CryptoPP::ECP::Point P = detail::zstr_to_ECPoint(secp256k1, pub_key_hex);
        pub_key.Initialize(secp256k1, P);

        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Verifier verifier(pub_key);

        string signature_raw(verifier.MaxSignatureLength(), '\x00');
        CryptoPP::DSAConvertSignatureFormat(
                    (CryptoPP::byte*) signature_raw.data(), signature_raw.size(), CryptoPP::DSA_P1363,
                    (const CryptoPP::byte*)signature_der.data(), signature_der.size(), CryptoPP::DSA_DER
                    );

        bool result = false;
        result = verifier.VerifyMessage((CryptoPP::byte*) message.data(), message.size(),
                                        (CryptoPP::byte*) signature_raw.data(), signature_raw.size());

        return result;
    } catch (...) {
        return false;
    }
}

signature::signature(public_key const& pb_key_,
                     std::string const& message_,
                     std::string const& signature_b58_)
    : pb_key(pb_key_)
    , message(message_)
    , base58(signature_b58_)
{
    if (false == verify_signature(pb_key, message, base58))
        throw exception_signature(*this);
}

exception_private_key::exception_private_key(string const& str_private_key)
    : runtime_error("invalid private key: \"" + str_private_key + "\"")
    , priv_key(str_private_key)
{}

exception_private_key::exception_private_key(exception_private_key const& other) noexcept
    : runtime_error(other)
    , priv_key(other.priv_key)
{}

exception_private_key& exception_private_key::operator=(exception_private_key const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    priv_key = other.priv_key;
    return *this;
}

exception_private_key::~exception_private_key() noexcept
{}

exception_public_key::exception_public_key(string const& str_public_key)
    : runtime_error("invalid public key: \"" + str_public_key + "\"")
    , pub_key(str_public_key)
{}

exception_public_key::exception_public_key(exception_public_key const& other) noexcept
    : runtime_error(other)
    , pub_key(other.pub_key)
{}

exception_public_key& exception_public_key::operator=(exception_public_key const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    pub_key = other.pub_key;
    return *this;
}

exception_public_key::~exception_public_key() noexcept
{}

exception_signature::exception_signature(signature const& sgn_)
    : runtime_error("invalid signature: "
                    "\"" + sgn_.pb_key.to_string() + "\", "
                    "\"" + std::string(sgn_.message.begin(), sgn_.message.end()) + "\", "
                    "\"" + sgn_.base58 + "\"")
    , sgn(sgn_)
{}

exception_signature::exception_signature(exception_signature const& other) noexcept
    : runtime_error(other)
    , sgn(other.sgn)
{}

exception_signature& exception_signature::operator=(exception_signature const& other) noexcept
{
    dynamic_cast<runtime_error*>(this)->operator =(other);
    sgn = other.sgn;
    return *this;
}

exception_signature::~exception_signature() noexcept
{}

string hash(const string & message)
{
    auto _hash = detail::hash(message);

    return to_base58(_hash);
}

string to_hex(const string & message)
{
    std::string _hash_hex;
    CryptoPP::StringSource ss(message, true,
                              new CryptoPP::HexEncoder(
                              new CryptoPP::StringSink(_hash_hex)
                            ));
    return _hash_hex;
}

string from_hex(const string & hex_str)
{
    std::string _raw_str;
    CryptoPP::StringSource ss(hex_str, true,
                              new CryptoPP::HexDecoder(
                              new CryptoPP::StringSink(_raw_str)
                            ));
    return string(_raw_str.begin(), _raw_str.end());
}

string from_base58(string const& data)
{
    auto vec = detail::from_base58(data);
    return string(vec.begin(), vec.end());
}

string aes_encrypt(string const& key, string const& plaintext)
{
    // DEFAULT_KEYLENGTH=16 bytes, that is 128 bit AES key
    CryptoPP::byte aes_key[CryptoPP::AES::DEFAULT_KEYLENGTH];
    CryptoPP::byte iv[CryptoPP::AES::BLOCKSIZE];
    memset(aes_key, 0x00, CryptoPP::AES::DEFAULT_KEYLENGTH);
    memcpy(aes_key, key.c_str(), std::min(uint64_t(key.length()), uint64_t(CryptoPP::AES::DEFAULT_KEYLENGTH)));
    memset(iv, 0x00, CryptoPP::AES::BLOCKSIZE);

    string ciphertext;

    CryptoPP::AES::Encryption aesEncryption(aes_key, CryptoPP::AES::DEFAULT_KEYLENGTH);
    CryptoPP::CBC_Mode_ExternalCipher::Encryption cbcEncryption(aesEncryption, iv);

    CryptoPP::StreamTransformationFilter stfEncryptor(cbcEncryption, new CryptoPP::StringSink(ciphertext));
    stfEncryptor.Put(reinterpret_cast<const unsigned char*>(plaintext.c_str()), plaintext.length());
    stfEncryptor.MessageEnd();

    return ciphertext;
}

string aes_decrypt(string const& key, string const& ciphertext)
{
    // DEFAULT_KEYLENGTH=16 bytes, that is 128 bit AES key
    CryptoPP::byte aes_key[CryptoPP::AES::DEFAULT_KEYLENGTH];
    CryptoPP::byte iv[CryptoPP::AES::BLOCKSIZE];
    memset(aes_key, 0x00, CryptoPP::AES::DEFAULT_KEYLENGTH);
    memcpy(aes_key, key.c_str(), std::min(uint64_t(key.length()), uint64_t(CryptoPP::AES::DEFAULT_KEYLENGTH)));
    memset(iv, 0x00, CryptoPP::AES::BLOCKSIZE);

    string decryptedtext;

    CryptoPP::AES::Decryption aesDecryption(aes_key, CryptoPP::AES::DEFAULT_KEYLENGTH);
    CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, iv);

    CryptoPP::StreamTransformationFilter stfDecryptor(cbcDecryption, new CryptoPP::StringSink(decryptedtext));
    stfDecryptor.Put(reinterpret_cast<const unsigned char*>(ciphertext.c_str()), ciphertext.size());
    stfDecryptor.MessageEnd();

    return decryptedtext;
}

string ECIES_encrypt(string const& key, string const& plain_b64_msg)
{

    B_UNUSED(key);
    B_UNUSED(plain_b64_msg);

    string cipher_b64_msg;

    B_UNUSED(cipher_b64_msg);

    return cipher_b64_msg;
}

string ECIES_decrypt(string const& key, string const& cipher_b64_msg)
{

    B_UNUSED(key);
    B_UNUSED(cipher_b64_msg);

    string plain_b64_msg;

    B_UNUSED(plain_b64_msg);

    return plain_b64_msg;
}

uint64_t distance(string const& hash58_first, string const& hash58_second)
{
    vector<unsigned char> vec_first, vec_second;
    vec_first = detail::from_base58(hash58_first.c_str());
    vec_second = detail::from_base58(hash58_second.c_str());
    if (vec_first.empty())
        throw runtime_error("invalid base58 string: " + hash58_first);
    if (vec_second.empty())
        throw runtime_error("invalid base58 string: " + hash58_second);

    if (vec_first.size() != 32)
        throw runtime_error("not a valid hash: " + hash58_first);
    if (vec_second.size() != 32)
        throw runtime_error("not a valid hash: " + hash58_second);

    auto setbitcount = [](unsigned char ch)
    {
        size_t res = 0;
        unsigned char test = 1;
        for (size_t index = 0; index < 8; ++index)
        {
            unsigned char temp = test & ch;
            res += (0 != temp);
            test *= 2;
        }

        return res;
    };

    uint32_t compress_int = 0;
    for (uint32_t index = 0; index < 32; ++index)
    {
        unsigned char ch = vec_first[index] ^ vec_second[index];
        uint32_t k = 6 - index / 8;
        uint32_t temp = (k <= setbitcount(ch));

        compress_int *= 2;
        compress_int |= temp;
    }

    return compress_int;
}


//string base64_to_hex(const string & b64_str)
//{
//    string hex_str;
//    CryptoPP::StringSource ss(b64_str, true,
//                              new CryptoPP::Base64Decoder(
//                              new CryptoPP::HexEncoder(
//                              new CryptoPP::StringSink(hex_str)
//    )));
//    return hex_str;
//}

//bitcoin
std::string to_base58(const std::string & raw_str)
{
    const unsigned char* pbegin = (const unsigned char*)raw_str.data();
    const size_t sz = raw_str.size();
    static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    const unsigned char* pend = pbegin + sz;
    // Skip & count leading zeroes.
    int zeroes = 0;
    int length = 0;
    for ( ; pbegin != pend && *pbegin == 0; pbegin++)
        zeroes++;

    // Allocate enough space in big-endian base58 representation.
    size_t size = sz * 138 / 100 + 1; // log(256) / log(58), rounded up.
    vector<unsigned char> b58(size);
    // Process the bytes.
    for ( ; pbegin != pend; pbegin++)
    {
        int carry = *pbegin;
        int i = 0;
        // Apply "b58 = b58 * 256 + ch".
        for (vector<unsigned char>::reverse_iterator it = b58.rbegin(); (carry != 0 || i < length) && (it != b58.rend()); it++, i++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }

        assert(carry == 0);
        length = i;
    }
    // Skip leading zeroes in base58 result.
    vector<unsigned char>::iterator it = b58.begin() + (size - length);
    while (it != b58.end() && *it == 0)
        it++;
    // Translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end())
        str += pszBase58[*(it++)];
    return str;
}


string from_base64(string const& data)
{
    using namespace CryptoPP;
    string result;
    StringSource ss(data, true, new Base64Decoder(new StringSink(result)));
    return result;
}

string to_base64(string const& data, bool insertLineBreaks)
{
    using namespace CryptoPP;
    string result;
    StringSource ss(data, true, new Base64Encoder(new StringSink(result), insertLineBreaks));
    return result;
}

//bitcoin
namespace detail
{
vector<unsigned char> from_base58(string const & str_b58)
{
    const char* psz = str_b58.c_str();
    vector<unsigned char> vch;
    static const int8_t mapBase58[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6,  7, 8,-1,-1,-1,-1,-1,-1,
        -1, 9,10,11,12,13,14,15, 16,-1,17,18,19,20,21,-1,
        22,23,24,25,26,27,28,29, 30,31,32,-1,-1,-1,-1,-1,
        -1,33,34,35,36,37,38,39, 40,41,42,43,-1,44,45,46,
        47,48,49,50,51,52,53,54, 55,56,57,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    };
    // Skip leading spaces.
    while (*psz && isspace(*psz))
        psz++;
    // Skip and count leading '1's.
    int zeroes = 0;
    int length = 0;
    for ( ; *psz == '1'; psz++)
        zeroes++;

    // Allocate enough space in big-endian base256 representation.
    size_t size = strlen(psz) * 733 /1000 + 1; // log(58) / log(256), rounded up.
    vector<unsigned char> b256(size);
    // Process the characters.
    static_assert(sizeof(mapBase58)/sizeof(mapBase58[0]) == 256, "mapBase58.size() should be 256"); // guarantee not out of range
    for ( ; *psz && !isspace(*psz); psz++)
    {
        // Decode base58 character
        int carry = mapBase58[(uint8_t)*psz];
        if (carry == -1)  // Invalid b58 character
            return vch;
        int i = 0;
        for (vector<unsigned char>::reverse_iterator it = b256.rbegin(); (carry != 0 || i < length) && (it != b256.rend()); ++it, ++i) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        assert(carry == 0);
        length = i;
    }
    // Skip trailing spaces.
    while (isspace(*psz))
        psz++;
    if (*psz != 0)
        return vch;
    // Skip leading zeroes in b256.
    vector<unsigned char>::iterator it = b256.begin() + (size - length);
    while (it != b256.end() && *it == 0)
        it++;
    // Copy result into output vector.
    vch.reserve(zeroes + (b256.end() - it));
    vch.assign(zeroes, 0x00);
    while (it != b256.end())
        vch.push_back(*(it++));
    return vch;
}
}

#define HASH_VER 1
#define BRAIN_KEY_WORD_COUNT 16

namespace detail
{
//graphene mod
std::string suggest_brain_key()
{
    std::string brain_key{};

    uint8_t seed[sizeof(std::mt19937_64::result_type)];
    CryptoPP::OS_GenerateRandomBlock(true , seed, sizeof(seed));

    std::mt19937_64::result_type mt64_seed;
    memcpy(&mt64_seed, seed, sizeof(seed));

    std::mt19937_64 mt64_engine(mt64_seed);
    std::uniform_int_distribution<> uni_int_dist(0, words::word_list_size - 1);

    for( int i=0; i<BRAIN_KEY_WORD_COUNT; i++ )
    {
        if( i > 0 )
            brain_key += " ";
        brain_key += words::word_list[ uni_int_dist(mt64_engine) ];
    }

    brain_key = normalize_brain_key(brain_key);

    return brain_key;
}

std::string bk_to_sk(std::string const& bk_str, int sequence_number )
{
    CryptoPP::SHA256 sha256;
    CryptoPP::SHA512 sha512;

    std::string sk_str{};

    CryptoPP::StringSource ss(bk_str + " " + std::to_string(sequence_number),
                              true, new CryptoPP::HashFilter(sha512, new CryptoPP::HashFilter(sha256, new CryptoPP::StringSink(sk_str))));
    return sk_str;
}

std::string sk_to_wif(const std::string & secret)
{
    CryptoPP::SHA256 sha256;
    std::string chk_str{}, wif_str{"\x80", 1};
    wif_str += secret;

    CryptoPP::StringSource ss(wif_str, true, new CryptoPP::HashFilter(sha256, new CryptoPP::HashFilter(sha256, new CryptoPP::StringSink(chk_str))));

    wif_str += chk_str.substr(0, 4);
    return to_base58(wif_str);
}

string bk_to_wif_sk(string const& bk_str, int sequence_number)
{
    auto sk = bk_to_sk(bk_str, sequence_number);
    return sk_to_wif(sk);
}

bool wif_to_sk(const std::string & wif_str, CryptoPP::Integer& sk)
{
    CryptoPP::SHA256 sha256;
    bool code = true;

    vector<unsigned char> vch = from_base58(wif_str);
    char z{0};

    if (code &&
        false == vch.empty() &&
        *vch.begin() != 0x80)
        code = false;
    else if (vch.size() != 37)
        code = false;
    else
    {
        std::string result(vch.size() - 1 - 4, z), chk_str_(4, z);
        std::copy(vch.begin() + 1, vch.end() - 4, result.begin());
        std::copy(vch.end() - 4, vch.end(), chk_str_.begin());
        std::string chk_str{};

        CryptoPP::StringSource ss(std::string("\x80") + result, true, new CryptoPP::HashFilter(sha256, new CryptoPP::HashFilter(sha256, new CryptoPP::StringSink(chk_str))));
        chk_str.resize(4);

        if(chk_str != chk_str_)
            code = false;
        else
            sk = CryptoPP::Integer{(CryptoPP::byte*)result.data(), result.size()};
    }

    return code;
}

std::string normalize_brain_key(const std::string & bk)
{
    return bk;
}

string pk_to_base58(std::string const& key)
{

#if HASH_VER
    CryptoPP::RIPEMD160 hash;
#else
    CryptoPP::SHA256 hash;
#endif
    std::string chk_str{};
    CryptoPP::StringSource ss(key, true, new CryptoPP::HashFilter(hash,  new CryptoPP::StringSink(chk_str)));

    string key_temp = key + std::string(chk_str.data(), 4);

    return to_base58(key_temp);
}

bool base58_to_pk_hex(string const& b58_str, string& pk)
{
    bool code = true;

#if HASH_VER
    CryptoPP::RIPEMD160 hash;
#else
    CryptoPP::SHA256 hash;
#endif
    vector<unsigned char> vch = from_base58(b58_str);

    if (code &&
        vch.size() > 4)
    {
        std::string result(vch.begin(), vch.end() - 4);
        std::string chk_str_(vch.end() - 4, vch.end());
        std::string chk_str{};

        CryptoPP::StringSource ss(result, true,
                                  new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(chk_str)));
        chk_str.resize(4);

        if(chk_str != chk_str_)
            code = false;
        else
            pk = result;
    }

    return code;
}

bool base58_to_pk(string const& b58_str, CryptoPP::ECP::Point& pk)
{
    string result;
    bool code = base58_to_pk_hex(b58_str, result);
    auto secp256k1 = CryptoPP::ASN1::secp256k1();
    if (code)
        pk = detail::zstr_to_ECPoint(secp256k1, result);

    return code;
}

std::string ECPoint_to_zstr(const CryptoPP::OID &oid, const CryptoPP::ECP::Point & P, bool compressed)
{
    auto ecgp = CryptoPP::DL_GroupParameters_EC<CryptoPP::ECP>(oid);
    auto ecc = ecgp.GetCurve();

    std::string z_str(ecc.EncodedPointSize(compressed), '\x00');
    ecc.EncodePoint((CryptoPP::byte*)z_str.data(), P, compressed);

    return z_str;
}

CryptoPP::ECP::Point zstr_to_ECPoint(const CryptoPP::OID &oid, const std::string& z_str)
{
    CryptoPP::ECP::Point P;

    auto ecgp = CryptoPP::DL_GroupParameters_EC<CryptoPP::ECP>(oid);
    auto ecc = ecgp.GetCurve();

    ecc.DecodePoint(P, (CryptoPP::byte*)z_str.data(), z_str.size());
    return P;
}

std::string decompress_pk(const CryptoPP::OID &oid, std::string str)
{
    auto P = zstr_to_ECPoint(oid, str);
    return ECPoint_to_zstr(oid, P, false);
}

std::string compress_pk(const CryptoPP::OID &oid, std::string str)
{
    auto P = zstr_to_ECPoint(oid, str);
    return ECPoint_to_zstr(oid, P, true);
}

string hash(const string & message)
{
    CryptoPP::SHA256 sha256;
    string _hash;

    CryptoPP::StringSource ss(message, true,
                              new CryptoPP::HashFilter(sha256,
                              new CryptoPP::StringSink(_hash)
                            ));
    return _hash;
}

}   //  end detail
}
