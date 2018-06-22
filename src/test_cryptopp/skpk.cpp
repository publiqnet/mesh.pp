#include "words.hpp"

#include "cryptopp/eccrypto.h"
#include "cryptopp/ecp.h"
#include <cryptopp/hex.h>
#include "cryptopp/oids.h"
#include "cryptopp/osrng.h"
#include "cryptopp/ripemd.h"
#include "cryptopp/sha.h"

#include <iostream>
#include <random>
#include <string>

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
    int size = sz * 138 / 100 + 1; // log(256) / log(58), rounded up.
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


std::string sk_to_wif(const std::string & secret)
{
    CryptoPP::SHA256 sha256;
    std::string chk_str{}, wif_str{"\x80", 1};
    wif_str += secret;

    CryptoPP::StringSource ss(wif_str, true, new CryptoPP::HashFilter(sha256, new CryptoPP::HashFilter(sha256, new CryptoPP::StringSink(chk_str))));

    wif_str += std::string(chk_str.data(), 4);
    return EncodeBase58((uint8_t *)wif_str.data(), wif_str.size());
}


std::string pk_to_base58(std::string key)
{
#if 1
    CryptoPP::RIPEMD160 hash;
#else
    CryptoPP::SHA256 hash;
#endif
    std::string chk_str{};
    CryptoPP::StringSource ss(key, true, new CryptoPP::HashFilter(hash,  new CryptoPP::StringSink(chk_str)));

    key += std::string(chk_str.data(), 4);

    return EncodeBase58((uint8_t *)key.data(), key.size());
}

std::string ECPoint_to_zstr(const CryptoPP::ECP::Point &P)
{
    char z{0};
    std::string Px(P.x.ByteCount() + 1, z);
    P.x.Encode((uint8_t*)Px.data() + 1, Px.size() - 1);

    Px[0] = (P.y.IsOdd()) ? 0x03 : 0x02;

    return Px;
}

int main(int argc, char **argv)
{
    auto bk = suggest_brain_key();

    std::cout<<bk<<std::endl;
    auto sk = bk_to_sk(bk, 0);
    std::cout << "sk/wif b58: " << sk_to_wif(sk) << std::endl;

    CryptoPP::Integer sk_i{(uint8_t*)sk.data(), sk.size()};

#if 0
    std::string hex_str{};
    CryptoPP::StringSource ss(sk, true, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hex_str)));
    std::cout<<hex_str<<std::endl;
#endif

    auto secp256k1 = CryptoPP::ASN1::secp256k1();
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey private_key;
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey public_key;
    private_key.Initialize(secp256k1, sk_i);
    private_key.MakePublicKey(public_key);

    auto pk_i = public_key.GetPublicElement();

    std::cout << "pk b58: " << pk_to_base58(ECPoint_to_zstr(pk_i)) << std::endl;

    return 0;
}
