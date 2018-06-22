//(
//  main.cpp
//  Test1
//
//  Created by GorMkrtchyan on 6/14/18.
//  Copyright © 2018 GorMkrtchyan. All rights reserved.
//

#include <iostream>

#include <cryptopp/hex.h>
#include <cryptopp/osrng.h>
#include <cryptopp/pssr.h>
#include <cryptopp/rsa.h>
#include <cryptopp/whrlpool.h>

#include <string>

using namespace std;

struct KeyPairHex
{
    string publicKey;
    string privateKey;
};

using Signer   = CryptoPP::RSASS<CryptoPP::PSSR, CryptoPP::Whirlpool>::Signer;
using Verifier = CryptoPP::RSASS<CryptoPP::PSSR, CryptoPP::Whirlpool>::Verifier;

//==============================================================================
inline KeyPairHex RsaGenerateHexKeyPair(unsigned int aKeySize)
{
    KeyPairHex keyPair;

    // PGP Random Pool-like generator
    CryptoPP::AutoSeededRandomPool rng;

    // generate keys
    CryptoPP::RSA::PrivateKey privateKey;
    privateKey.GenerateRandomWithKeySize(rng, aKeySize);
    CryptoPP::RSA::PublicKey publicKey(privateKey);

    // save keys
    publicKey.Save( CryptoPP::HexEncoder(
                                         new CryptoPP::StringSink(keyPair.publicKey)).Ref());
    privateKey.Save(CryptoPP::HexEncoder(
                                         new CryptoPP::StringSink(keyPair.privateKey)).Ref());

    return keyPair;
}

//==============================================================================
inline std::string RsaSignString(const std::string &aPrivateKeyStrHex,
                                 const std::string &aMessage) {

    // decode and load private key (using pipeline)
    CryptoPP::RSA::PrivateKey privateKey;
    privateKey.Load(CryptoPP::StringSource(aPrivateKeyStrHex, true,
                                           new CryptoPP::HexDecoder()).Ref());

    // sign message
    std::string signature;
    Signer signer(privateKey);
    CryptoPP::AutoSeededRandomPool rng;

    CryptoPP::StringSource ss(aMessage, true,
                              new CryptoPP::SignerFilter(rng, signer,
                                        new CryptoPP::HexEncoder(
                                                    new CryptoPP::StringSink(signature))));

    return signature;
}

//==============================================================================
inline bool RsaVerifyString(const std::string &aPublicKeyStrHex,
                            const std::string &aMessage,
                            const std::string &aSignatureStrHex) {

    // decode and load public key (using pipeline)
    CryptoPP::RSA::PublicKey publicKey;
    publicKey.Load(CryptoPP::StringSource(aPublicKeyStrHex, true,
                                          new CryptoPP::HexDecoder()).Ref());

    // decode signature
    std::string decodedSignature;
    CryptoPP::StringSource ss(aSignatureStrHex, true,
                              new CryptoPP::HexDecoder(
                                    new CryptoPP::StringSink(decodedSignature)));

    // verify message
    bool result = false;
    Verifier verifier(publicKey);
    CryptoPP::StringSource ss2(decodedSignature + aMessage, true,
                               new CryptoPP::SignatureVerificationFilter(verifier,
                                        new CryptoPP::ArraySink((CryptoPP::byte*)&result,
                                                                                                 sizeof(result))));

    return result;
}



int main(int argc, const char * argv[])
{
    // insert code here..

    {
        auto keys = RsaGenerateHexKeyPair(3072);
        std::cout << "Private key: " << std::endl << keys.privateKey << "\n" << std::endl;

        std::cout << "Public key: " << std::endl << keys.publicKey << "\n" << std::endl;

        std::string message("secret message");
        std::cout << "Message:" << std::endl;
        std::cout << message << "\n" << std::endl;

        // generate a signature for the message
        auto signature(RsaSignString(keys.privateKey, message));
        std::cout << "Signature:" << std::endl;
        std::cout << signature << "\n" << std::endl;

        // verify signature against public key
        if (RsaVerifyString(keys.publicKey, message, signature)) {
            std::cout << "Signatue valid." << std::endl;
        } else {
            std::cout << "Signatue invalid." << std::endl;
        }
    }

    cout << "Hello, World C++11!\n";
    return 0;
}
