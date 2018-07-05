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
    meshpp::config::set_public_key_prefix("PBQ");

    meshpp::random_seed m_rs;
    meshpp::private_key m_pvk = m_rs.get_private_key();
    //  1)  Generate Public/Private key pair
    //
    //  go to https://kjur.github.io/jsrsasign/sample/sample-ecdsa.html
    //  choose ECC curve "secp256k1"
    //  generate EC key pair
    //  got example private key 5ada66ae5c8416d4cac1fbb3a694c6c57f4da43a53a734d366998f9c466df133
    //      and public key 0486e4e8f19a64b66f727717ad24e9f2c88b7c09ac6a8757bffa34e75c4742537586f866c3c02c34e0d15002a60ea45fa06f62bdacec60e94f5f97a9cc55a51c8d

    //  2)  Get Private Key Base58 representation
    //
    //  go to http://extranet.cryptomathic.com/hashcalc/index
    //  "enter as hex" private key prefixed with "80"
    //      805ada66ae5c8416d4cac1fbb3a694c6c57f4da43a53a734d366998f9c466df133
    //  "calculate" sha256 hash, the result will be c18cab3cc210b8bebedafb4dfd31417cabc24ff19edc39cad0604ff1f370e550
    //  again calculate the hash of the result, and the double hash will be
    //      70a030a98f51c146943d611cdf8edc6333d92aee47c9e0eaaec5c13278cf3591
    //  pick first 4 bytes (70a030a9) and append to private key
    //      805ada66ae5c8416d4cac1fbb3a694c6c57f4da43a53a734d366998f9c466df13370a030a9
    //      this will be the hex representation of the private key before base58 encoding
    //  use http://lenschulwitz.com/base58 to encode private key to base58
    //      so we get base58 representation of private key, which we can use below
    m_pvk = meshpp::private_key("5JWJJWjbeyK1B22MLJDxqumadmhRfp88ZY5oPPNwTJTs1Lvkf2p");
    meshpp::public_key m_pbk = m_pvk.get_public_key();

    //  3)  Get Public Key Base58 representation
    //  the first byte "04" means that this is an uncompressed form, we remove it
    //  and divide the rest of the public key buffer into two equal halves
    //      86e4e8f19a64b66f727717ad24e9f2c88b7c09ac6a8757bffa34e75c47425375
    //      86f866c3c02c34e0d15002a60ea45fa06f62bdacec60e94f5f97a9cc55a51c8d
    //  check if the last byte "8d" is even or odd
    //  pick the first half and prepend it with "03" if it's odd and "02" otherwise
    //      so we have the public key compressed form
    //      0386e4e8f19a64b66f727717ad24e9f2c88b7c09ac6a8757bffa34e75c47425375
    //  use it in http://extranet.cryptomathic.com/hashcalc/index to get the RIPEMD hash
    //      36b6859f2c5ab04108bf75e3e523409927dba327
    //  append first 4 bytes 36b6859f to compressed public key
    //      0386e4e8f19a64b66f727717ad24e9f2c88b7c09ac6a8757bffa34e75c4742537536b6859f
    //  finally use http://lenschulwitz.com/base58 to encode public key to base58
    //      7reDm5xbd2HPyvZiiJmknJ8YLG2eusATc7gKYM8z931krCUEoQ

    std::cout << std::endl << m_rs.get_brain_key() << std::endl;
    std::cout << m_pvk.get_base58_wif() << std::endl;
    std::cout << m_pbk.get_base58() << std::endl;
    std::cout << m_pbk.to_string() << std::endl;

    std::string msg = "";
    std::vector<char> msg_buf(msg.begin(), msg.end());

    meshpp::signature m_sgn = m_pvk.sign(msg_buf);
    std::cout << m_sgn.base64 << std::endl;


    std::cout<<R"(hash256("") -> b58:GKot5hBsd81kMupNCXHaqbhv3huEbxAFMLnpcX2hniwn -> 0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)"<<std::endl;
    std::cout << meshpp::hash("") << std::endl;
    std::cout << meshpp::hash(msg) << std::endl;
    std::cout << meshpp::hash(msg_buf.begin(), msg_buf.end()) << std::endl;
    std::cout << m_sgn.verify() << std::endl;

    std::string signed_message = "a test message 42!";
    std::vector<char> signed_message_buffer(signed_message.begin(), signed_message.end());
    meshpp::public_key pbk("PBQ7reDm5xbd2HPyvZiiJmknJ8YLG2eusATc7gKYM8z931krCUEoQ");
    meshpp::public_key pbk2 = pbk; // check that copy constructor functions ok
    meshpp::signature imported_signature(
                pbk2,
                signed_message_buffer,
                "MEUCIADINLbtQqmp5jhLdqfLYVSYPoK9vrhW5A7H8d5ctOLqAiEA5gBBmQKu+HB9nR07X75oBHy76ISqzcNpwOmq1za7/fI="
                );
    std::cout << imported_signature.verify() << std::endl;


    return 0;
}
