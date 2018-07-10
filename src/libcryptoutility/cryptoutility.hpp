#pragma once

#include "global.hpp"

#include <string>
#include <vector>
#include <exception>

namespace meshpp
{
class private_key;
class public_key;
class signature;

class CRYPTOUTILITYSHARED_EXPORT config
{
public:
    static void set_public_key_prefix(std::string const& prefix);
};

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
    signature sign(std::vector<char> const& message) const;
private:
    std::string base58_wif;
};

class CRYPTOUTILITYSHARED_EXPORT public_key
{
public:
    public_key(std::string const& str_base58);
    public_key(public_key const& other);
    ~public_key();

    std::string to_string() const;
    std::string get_base58() const;
private:
    std::string str_base58;
    //std::string raw_base58;
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

    bool verify(/*bool encoded = false*/) const;

    public_key pb_key;
    std::vector<char> message;
    std::string base64;
};

class CRYPTOUTILITYSHARED_EXPORT exception_public_key : public std::runtime_error
{
public:
    explicit exception_public_key(std::string const& str_public_key);

    exception_public_key(exception_public_key const&) noexcept;
    exception_public_key& operator=(exception_public_key const&) noexcept;

    virtual ~exception_public_key() noexcept;

    std::string pub_key;
};

CRYPTOUTILITYSHARED_EXPORT std::string hash(const std::string &);
template <typename InputIt>
std::string hash(InputIt first, InputIt last)
{
    return hash(std::string(first, last));
}

CRYPTOUTILITYSHARED_EXPORT std::string base64_to_hex(const std::string & b64_str);

}
