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


int main(int argc, char **argv)
{
    meshpp::random_seed m_rs;
    meshpp::private_key m_pvk = m_rs.get_private_key();
    meshpp::public_key m_pbk = m_pvk.get_public_key();

    std::cout << std::endl << m_rs.get_brain_key() << std::endl;
    std::cout << m_pvk.get_base58_wif() << std::endl;
    std::cout << m_pbk.get_base58() << std::endl;

    std::string msg = "";
    std::vector<char> msg_buf(msg.begin(), msg.end());

    meshpp::signature m_sgn = m_pvk.sign(msg_buf);
    std::cout << m_sgn.base64 << std::endl;

    std::cout<<R"(hash256("") -> b58:GKot5hBsd81kMupNCXHaqbhv3huEbxAFMLnpcX2hniwn -> 0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)"<<std::endl;
    std::cout << meshpp::hash("") << std::endl;
    std::cout << meshpp::hash(msg) << std::endl;
    std::cout << meshpp::hash(msg_buf.begin(), msg_buf.end()) << std::endl;
    std::cout << m_sgn.verify() << std::endl;

    return 0;
}
