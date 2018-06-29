#include "words.hpp"

#include <mesh.pp/cryptopp_byte.hpp>

#include <cryptopp/eccrypto.h>
#include <cryptopp/ecp.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/ripemd.h>
#include <cryptopp/sha.h>

#include <mesh.pp/cryptoutility.hpp>

#include <iostream>
#include <random>
#include <string>
#include <cassert>

#define BRAIN_KEY_WORD_COUNT 16

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

//graphene mod
std::string suggest_brain_key()
{
    std::string brain_key{};

    uint8_t seed[sizeof(std::mt19937_64::result_type)];
    CryptoPP::OS_GenerateRandomBlock(true , seed, sizeof(seed));

    std::mt19937_64 mt64_engine(reinterpret_cast<std::mt19937_64::result_type>(seed));
    std::uniform_int_distribution<> uni_int_dist(0, graphene::words::word_list_size - 1);

    for( int i=0; i<BRAIN_KEY_WORD_COUNT; i++ )
    {
        if( i > 0 )
            brain_key += " ";
        brain_key += graphene::words::word_list[ uni_int_dist(mt64_engine) ];
    }

    brain_key = normalize_brain_key(brain_key);

    return brain_key;
}

std::string bk_to_sk(std::string bk_str, int sequence_number )
{
    CryptoPP::SHA512 sha512;
    CryptoPP::SHA256 sha256;

    bk_str += " ";
    bk_str += std::to_string(sequence_number);
    std::string sk_str{};

    CryptoPP::StringSource ss(bk_str, true, new CryptoPP::HashFilter(sha512, new CryptoPP::HashFilter(sha256, new CryptoPP::StringSink(sk_str))));
    return sk_str;
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


std::string sk_to_wif(const std::string & secret)
{
    CryptoPP::SHA256 sha256;
    std::string chk_str{}, wif_str{"\x80", 1};
    wif_str += secret;

    CryptoPP::StringSource ss(wif_str, true, new CryptoPP::HashFilter(sha256, new CryptoPP::HashFilter(sha256, new CryptoPP::StringSink(chk_str))));

    wif_str += std::string(chk_str.data(), 4);
    return EncodeBase58((uint8_t *)wif_str.data(), wif_str.size());
}

std::string wif_to_sk(const std::string & wif_str)
{
    CryptoPP::SHA256 sha256;
    std::vector<unsigned char> vch;
    char z{0};
    DecodeBase58(wif_str.c_str(), vch);

    if (*vch.begin() != 0x80)
        return {};

    std::string result(vch.size() - 1 - 4, z), chk_str_(4, z);
    std::copy(vch.begin() + 1, vch.end() - 4, result.begin());
    std::copy(vch.end() - 4, vch.end(), chk_str_.begin());
    std::string chk_str{};

    CryptoPP::StringSource ss(std::string("\x80") + result, true, new CryptoPP::HashFilter(sha256, new CryptoPP::HashFilter(sha256, new CryptoPP::StringSink(chk_str))));
    chk_str.resize(4);

    if(chk_str != chk_str_)
        return {};

    return result;
}

#define HASH_VER 1

std::string pk_to_base58(std::string key)
{
#if HASH_VER
    CryptoPP::RIPEMD160 hash;
#else
    CryptoPP::SHA256 hash;
#endif
    std::string chk_str{};
    CryptoPP::StringSource ss(key, true, new CryptoPP::HashFilter(hash,  new CryptoPP::StringSink(chk_str)));

    key += std::string(chk_str.data(), 4);

    return EncodeBase58((uint8_t *)key.data(), key.size());
}

std::string base58_to_pk(const std::string & b58_str)
{
#if HASH_VER
    CryptoPP::RIPEMD160 hash;
#else
    CryptoPP::SHA256 hash;
#endif
    std::vector<unsigned char> vch;
    char z{0};
    DecodeBase58(b58_str.c_str(), vch);

    std::string result(vch.size() - 4, z), chk_str_(4, z);
    std::copy(vch.begin(), vch.end() - 4, result.begin());
    std::copy(vch.end() - 4, vch.end(), chk_str_.begin());
    std::string chk_str{};

    CryptoPP::StringSource ss(result, true, new CryptoPP::HashFilter(hash,  new CryptoPP::StringSink(chk_str)));
    chk_str.resize(4);

    if(chk_str != chk_str_)
        return {};

    return result;
}


std::string ECPoint_to_zstr(const CryptoPP::OID &oid, const CryptoPP::ECP::Point & P)
{
    auto ecgp = CryptoPP::DL_GroupParameters_EC<CryptoPP::ECP>(oid);
    auto ecc = ecgp.GetCurve();

    std::string z_str(ecc.EncodedPointSize(true), '\x00');
    ecc.EncodePoint((CryptoPP::byte*)z_str.data(), P, true);

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

#define INVERT 1

int main(int argc, char **argv)
{
    std::string bk = suggest_brain_key();

    std::cout<<bk<<std::endl;
    auto sk = bk_to_sk(bk, 0);
    auto wif = sk_to_wif(sk);
    std::cout << "sk/wif b58: " << wif << std::endl;

    CryptoPP::Integer sk_i{(uint8_t*)sk.data(), sk.size()};

#if INVERT
    auto sk_str = wif_to_sk(wif);
    CryptoPP::Integer sk_{(CryptoPP::byte*)sk_str.data(), sk_str.size()};
    std::cout<< "sk hex: " << std::hex << sk_ << std::endl;
#endif

    auto secp256k1 = CryptoPP::ASN1::secp256k1();
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
    private_key.Initialize(secp256k1, sk_i);
    private_key.MakePublicKey(public_key);

    auto pk_i = public_key.GetPublicElement();
    auto pk_b58 = pk_to_base58(ECPoint_to_zstr(secp256k1, pk_i));
    std::cout << "pk b58: " << pk_b58 << std::endl;

#if INVERT
    auto pk_str = base58_to_pk(pk_b58);
    auto P = zstr_to_ECPoint(secp256k1, pk_str);
    decltype(public_key) public_key_;
    public_key_.Initialize(secp256k1, P);

    CryptoPP::AutoSeededRandomPool arng;
    std::cout<< private_key.Validate(arng, 3)<<public_key.Validate(arng, 3)<<public_key_.Validate(arng, 3)<< std::endl;
    std::cout<< "public_key_ hex: " << std::hex << public_key_.GetPublicElement().x << " " << public_key_.GetPublicElement().y << std::endl;
    std::cout<< "public_key  hex: " << std::hex << public_key.GetPublicElement().x << " " << public_key.GetPublicElement().y << std::endl;
#endif

    meshpp::random_seed m_rs(bk);
    meshpp::private_key m_pvk = m_rs.get_private_key();
    meshpp::public_key m_pbk = m_pvk.get_public_key();

    std::cout << std::endl << m_rs.get_brain_key() << std::endl;
    std::cout << m_pvk.get_base58_wif() << std::endl;
    std::cout << m_pbk.get_base58() << std::endl;

    std::string msg = "tigran";
    std::vector<char> msg_buf(msg.begin(), msg.end());

    meshpp::signature m_sgn = m_pvk.sign(msg_buf);
    std::cout << m_sgn.base64 << std::endl;

    std::cout << m_sgn.verify() << std::endl;

    return 0;
}
