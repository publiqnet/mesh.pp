#pragma once

#include "global.hpp"

#include <memory>
#include <string>

namespace meshpp
{
namespace detail
{
class private_key_internals;
}

class CRYPTOUTILITYSHARED_EXPORT private_key
{
public:
    private_key(std::string const& base58);
    private_key(private_key&&);
    ~private_key();
private:
    std::unique_ptr<detail::private_key_internals> m_pimpl;
};
}
