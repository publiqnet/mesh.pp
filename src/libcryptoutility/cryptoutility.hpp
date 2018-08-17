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
    private_key get_private_key(uint64_t index) const;
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
    signature sign(std::string const& message) const;
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

CRYPTOUTILITYSHARED_EXPORT bool verify_signature(public_key const& pb_key,
                                                 std::string const& message,
                                                 std::string const& base58);

class CRYPTOUTILITYSHARED_EXPORT signature
{
public:
    signature(public_key const& pb_key,
              std::string const& message,
              std::string const& base58_);

    public_key pb_key;
    std::string message;
    std::string base58;
};

class CRYPTOUTILITYSHARED_EXPORT exception_private_key : public std::runtime_error
{
public:
    explicit exception_private_key(std::string const& str_private_key);

    exception_private_key(exception_private_key const&) noexcept;
    exception_private_key& operator=(exception_private_key const&) noexcept;

    virtual ~exception_private_key() noexcept;

    std::string priv_key;
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

class CRYPTOUTILITYSHARED_EXPORT exception_signature : public std::runtime_error
{
public:
    explicit exception_signature(signature const& sgn);

    exception_signature(exception_signature const&) noexcept;
    exception_signature& operator=(exception_signature const&) noexcept;

    virtual ~exception_signature() noexcept;

    signature sgn;
};

CRYPTOUTILITYSHARED_EXPORT std::string hash(const std::string &);
template <typename InputIt>
std::string hash(InputIt first, InputIt last)
{
    return hash(std::string(first, last));
}

CRYPTOUTILITYSHARED_EXPORT std::string to_hex(const std::string &);
template <typename InputIt>
std::string to_hex(InputIt first, InputIt last)
{
    return to_hex(std::string(first, last));
}

CRYPTOUTILITYSHARED_EXPORT std::string from_hex(const std::string &);
template <typename InputIt>
std::string from_hex(InputIt first, InputIt last)
{
    return from_hex(std::string(first, last));
}

CRYPTOUTILITYSHARED_EXPORT std::string to_base58(std::string const & raw_str);
template <typename InputIt>
std::string to_base58(InputIt first, InputIt last)
{
    return to_base58(std::string(first, last));
}

CRYPTOUTILITYSHARED_EXPORT std::string from_base58(std::string const & data);
template <typename InputIt>
std::string from_base58(InputIt first, InputIt last)
{
    return from_base58(std::string(first, last));
}

CRYPTOUTILITYSHARED_EXPORT uint64_t distance(std::string const& hash58_first, std::string const& hash58_second);

}
