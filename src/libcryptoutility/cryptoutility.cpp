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

#include <string>
#include <cassert>
#include <vector>
#include <exception>
#include <random>

using std::string;
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
string EncodeBase58(const unsigned char* pbegin, const size_t sz);
bool DecodeBase58(const char* psz, std::vector<unsigned char>& vch);
}

void config::set_public_key_prefix(std::string const& prefix)
{
    g_public_key_prefix = prefix;
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
        throw std::runtime_error("invalid private key: \"" + base58_wif + "\"");
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

signature private_key::sign(std::vector<char> const& message) const
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

    string signature_der_b64;
    CryptoPP::StringSource ss2(signature_der, true, new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(signature_der_b64), false));
    return signature(get_public_key(), message, signature_der_b64);
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

    /*auto secp256k1 = CryptoPP::ASN1::secp256k1();
    auto raw_ = detail::ECPoint_to_zstr(secp256k1, pk, false);
    raw_base58 = detail::EncodeBase58((CryptoPP::byte*)raw_.data(), raw_.size());*/
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

signature::signature(public_key const& pb_key_,
                     std::vector<char> const& message_,
                     std::string const& base64_)
    : pb_key(pb_key_)
    , message(message_)
    , base64(base64_)
{}

bool signature::verify() const
{
    auto secp256k1 = CryptoPP::ASN1::secp256k1();

    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey pub_key;
    string pub_key_hex;

    // decode base64 signature
    string signature_der;

    CryptoPP::StringSource ss(base64, true,
                              new CryptoPP::Base64Decoder(
                                    new CryptoPP::StringSink(signature_der)));


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
}

void signature::check() const
{
    if (false == verify())
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
                    "\"" + sgn_.base64 + "\"")
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
    CryptoPP::SHA256 sha256;
    string _hash;

    CryptoPP::StringSource ss(message, true,
                              new CryptoPP::HashFilter(sha256,
                              new CryptoPP::StringSink(_hash)
                            ));
    return detail::EncodeBase58((CryptoPP::byte*)_hash.data(), _hash.size());
}


string base64_to_hex(const string & b64_str)
{
    string hex_str;
    CryptoPP::StringSource ss(b64_str, true,
                              new CryptoPP::Base64Decoder(
                              new CryptoPP::HexEncoder(
                              new CryptoPP::StringSink(hex_str)
    )));
    return hex_str;
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

string EncodeBase58(const unsigned char* pbegin, const size_t sz);
bool DecodeBase58(const char* psz, std::vector<unsigned char>& vch);

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

    wif_str += std::string(chk_str.data(), 4);
    return EncodeBase58((uint8_t *)wif_str.data(), wif_str.size());
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

    std::vector<unsigned char> vch;
    char z{0};
    code = DecodeBase58(wif_str.c_str(), vch);

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

//graphene
std::string normalize_brain_key(const std::string & bk)
{
   size_t i = 0, n = bk.length();
   std::string result;
   char c;
   result.reserve( n );

   bool preceded_by_whitespace = false;
   bool non_empty = false;
   while( i < n )
   {
      c = bk[i++];
      switch( c )
      {
      case ' ':  case '\t': case '\r': case '\n': case '\v': case '\f':
         preceded_by_whitespace = true;
         continue;

      case 'a': c = 'A'; break;
      case 'b': c = 'B'; break;
      case 'c': c = 'C'; break;
      case 'd': c = 'D'; break;
      case 'e': c = 'E'; break;
      case 'f': c = 'F'; break;
      case 'g': c = 'G'; break;
      case 'h': c = 'H'; break;
      case 'i': c = 'I'; break;
      case 'j': c = 'J'; break;
      case 'k': c = 'K'; break;
      case 'l': c = 'L'; break;
      case 'm': c = 'M'; break;
      case 'n': c = 'N'; break;
      case 'o': c = 'O'; break;
      case 'p': c = 'P'; break;
      case 'q': c = 'Q'; break;
      case 'r': c = 'R'; break;
      case 's': c = 'S'; break;
      case 't': c = 'T'; break;
      case 'u': c = 'U'; break;
      case 'v': c = 'V'; break;
      case 'w': c = 'W'; break;
      case 'x': c = 'X'; break;
      case 'y': c = 'Y'; break;
      case 'z': c = 'Z'; break;

      default:
         break;
      }
      if( preceded_by_whitespace && non_empty )
         result.push_back(' ');
      result.push_back(c);
      preceded_by_whitespace = false;
      non_empty = true;
   }
   return result;
}

//bitcoin
std::string EncodeBase58(const unsigned char* pbegin, const size_t sz)
{
    static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    const unsigned char* pend = pbegin + sz;
    // Skip & count leading zeroes.
    int zeroes = 0;
    int length = 0;
    for ( ; pbegin != pend && *pbegin == 0; pbegin++)
        zeroes++;

    // Allocate enough space in big-endian base58 representation.
    size_t size = sz * 138 / 100 + 1; // log(256) / log(58), rounded up.
    std::vector<unsigned char> b58(size);
    // Process the bytes.
    for ( ; pbegin != pend; pbegin++)
    {
        int carry = *pbegin;
        int i = 0;
        // Apply "b58 = b58 * 256 + ch".
        for (std::vector<unsigned char>::reverse_iterator it = b58.rbegin(); (carry != 0 || i < length) && (it != b58.rend()); it++, i++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }

        assert(carry == 0);
        length = i;
    }
    // Skip leading zeroes in base58 result.
    std::vector<unsigned char>::iterator it = b58.begin() + (size - length);
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

//bitcoin
bool DecodeBase58(const char* psz, std::vector<unsigned char>& vch)
{
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
    std::vector<unsigned char> b256(size);
    // Process the characters.
    static_assert(sizeof(mapBase58)/sizeof(mapBase58[0]) == 256, "mapBase58.size() should be 256"); // guarantee not out of range
    for ( ; *psz && !isspace(*psz); psz++)
    {
        // Decode base58 character
        int carry = mapBase58[(uint8_t)*psz];
        if (carry == -1)  // Invalid b58 character
            return false;
        int i = 0;
        for (std::vector<unsigned char>::reverse_iterator it = b256.rbegin(); (carry != 0 || i < length) && (it != b256.rend()); ++it, ++i) {
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
        return false;
    // Skip leading zeroes in b256.
    std::vector<unsigned char>::iterator it = b256.begin() + (size - length);
    while (it != b256.end() && *it == 0)
        it++;
    // Copy result into output vector.
    vch.reserve(zeroes + (b256.end() - it));
    vch.assign(zeroes, 0x00);
    while (it != b256.end())
        vch.push_back(*(it++));
    return true;
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

    return EncodeBase58((uint8_t *)key_temp.data(), key_temp.size());
}

bool base58_to_pk_hex(string const& b58_str, string& pk)
{
    bool code = true;

#if HASH_VER
    CryptoPP::RIPEMD160 hash;
#else
    CryptoPP::SHA256 hash;
#endif
    std::vector<unsigned char> vch;
    code = DecodeBase58(b58_str.c_str(), vch);

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

}   //  end detail
}
