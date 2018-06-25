#include "cryptoutility.hpp"

#include <cryptopp/sha.h>

#include <string>

using std::string;

namespace meshpp
{
namespace detail
{
class private_key_internals
{
public:
    string base58;
};
}

private_key::private_key(string const& base58)
{

}
private_key::private_key(private_key&&) = default;

private_key::~private_key()
{}
}
