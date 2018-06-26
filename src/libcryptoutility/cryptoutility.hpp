#pragma once

#include "global.hpp"

#include <string>

namespace meshpp
{

class CRYPTOUTILITYSHARED_EXPORT private_key
{
public:
    private_key(std::string const& base58);
    ~private_key();
private:
    std::string base58_wif;
};
}
