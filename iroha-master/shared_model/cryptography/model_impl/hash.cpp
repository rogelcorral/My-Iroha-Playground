/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cryptography/hash.hpp"

#include <boost/functional/hash.hpp>

#include "common/byteutils.hpp"
namespace shared_model {
  namespace crypto {

    Hash::Hash() : Blob() {}

    Hash::Hash(const std::string &hash) : Blob(hash) {}

    Hash::Hash(const Blob &blob) : Blob(blob) {}

    Hash Hash::fromHexString(const std::string &hex) {
      return Hash(Blob::fromHexString(hex));
    }

    std::string Hash::toString() const {
      return detail::PrettyStringBuilder()
          .init("Hash")
          .append(Blob::hex())
          .finalize();
    }

    std::size_t Hash::Hasher::operator()(const Hash &h) const {
      using boost::hash_combine;
      using boost::hash_value;

      std::size_t seed = 0;
      hash_combine(seed, hash_value(h.blob()));

      return seed;
    }
  }  // namespace crypto
}  // namespace shared_model
