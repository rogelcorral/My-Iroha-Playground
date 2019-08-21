/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_CRYPTO_HPP
#define IROHA_CRYPTO_HPP

#include <string>

#include "crypto/keypair.hpp"

namespace iroha {

  /**
   * Sign message with ed25519 crypto algorithm
   * @param msg
   * @param msgsize
   * @param pub
   * @param priv
   * @return
   */
  sig_t sign(const uint8_t *msg,
             size_t msgsize,
             const pubkey_t &pub,
             const privkey_t &priv);

  sig_t sign(const std::string &msg,
             const pubkey_t &pub,
             const privkey_t &priv);

  /**
   * Verify signature of ed25519 crypto algorithm
   * @param msg
   * @param msgsize
   * @param pub
   * @param sig
   * @return true if signature is valid, false otherwise
   */
  bool verify(const uint8_t *msg,
              size_t msgsize,
              const pubkey_t &pub,
              const sig_t &sig);

  bool verify(const std::string &msg, const pubkey_t &pub, const sig_t &sig);

  /**
   * Generate random seed reading from /dev/urandom
   */
  blob_t<32> create_seed();

  /**
   * Generate random seed as sha3_256(passphrase)
   * @param passphrase
   * @return
   */
  blob_t<32> create_seed(std::string passphrase);

  /**
   * Create new keypair
   * @param seed
   * @return
   */
  keypair_t create_keypair(blob_t<32> seed);

  /**
   * Create new keypair with a default seed (by create_seed())
   * @return
   */
  keypair_t create_keypair();

}  // namespace iroha
#endif  // IROHA_CRYPTO_HPP
