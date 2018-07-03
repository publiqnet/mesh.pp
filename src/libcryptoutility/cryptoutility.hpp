#pragma once

#include "global.hpp"

#include <string>
#include <vector>

namespace meshpp
{
class private_key;
class public_key;
class signature;

class CRYPTOUTILITYSHARED_EXPORT random_seed
{
public:
    random_seed(std::string const& brain_key = std::string());
    ~random_seed();

    std::string get_brain_key() const;
    private_key get_private_key() const;
private:
    std::string brain_key;
};

class CRYPTOUTILITYSHARED_EXPORT private_key
{
public:
    private_key(std::string const& base58);
    private_key(private_key const& other);
    ~private_key();

    std::string get_base58_wif() const;
    public_key get_public_key() const;
    signature sign(std::vector<char> message) const;
private:
    std::string base58_wif;
};

class CRYPTOUTILITYSHARED_EXPORT public_key
{
public:
    public_key(std::string const& base58);
    public_key(public_key const& other);
    ~public_key();

    std::string get_base58() const;
private:
    std::string base58;
};

class CRYPTOUTILITYSHARED_EXPORT signature
{
public:
    signature(public_key const& pb_key_,
              std::vector<char> const& message_,
              std::string const& base64_)
        : pb_key(pb_key_)
        , message(message_)
        , base64(base64_) {}

    bool verify() const;

    public_key pb_key;
    std::vector<char> message;
    std::string base64;
};

CRYPTOUTILITYSHARED_EXPORT std::string hash(const std::string &);
template <typename InputIt>
std::string hash(InputIt first, InputIt last)
{
    return hash(std::string(first, last));
}


}
