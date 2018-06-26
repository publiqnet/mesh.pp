#include "cryptoutility.hpp"

#include <mesh.pp/cryptopp_byte.hpp>

#include <cryptopp/eccrypto.h>
#include <cryptopp/ecp.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/ripemd.h>
#include <cryptopp/sha.h>

#include <string>
#include <cassert>
#include <vector>
#include <exception>

using std::string;

namespace meshpp
{

namespace detail
{
bool wif_to_sk(const std::string & wif_str, CryptoPP::Integer& sk);
}

private_key::private_key(string const& base58_wif_)
    : base58_wif(base58_wif_)
{
    CryptoPP::Integer sk;
    if (false == detail::wif_to_sk(base58_wif, sk))
        throw std::runtime_error("invalid private key: " + base58_wif);
}

private_key::~private_key()
{}

namespace detail
{
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
    int size = strlen(psz) * 733 /1000 + 1; // log(58) / log(256), rounded up.
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

bool wif_to_sk(const std::string & wif_str, CryptoPP::Integer& sk)
{
    bool code = true;

    CryptoPP::SHA256 sha256;
    std::vector<unsigned char> vch;
    char z{0};
    code = DecodeBase58(wif_str.c_str(), vch);

    if (*vch.begin() != 0x80)
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
}
}
