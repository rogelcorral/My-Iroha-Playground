/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AMETSUCHI_INDEXER_HPP
#define AMETSUCHI_INDEXER_HPP

#include <string>

#include "common/result.hpp"
#include "interfaces/common_objects/types.hpp"

namespace iroha {
  namespace ametsuchi {

    /** Stores transaction data in WSV.
     * \attention The effect of any change only gets into WSV storage after \see
     * Indexer::flush() is called!
     */
    class Indexer {
     public:
      virtual ~Indexer() = default;

      /// Position of a transaction in the ledger.
      struct TxPosition {
        shared_model::interface::types::HeightType
            height;    ///< the height of block containing this transaction
        size_t index;  ///< the number of this transaction in the block
      };

      /// Index tx position by its hash.
      virtual void txHashPosition(
          const shared_model::interface::types::HashType &hash,
          TxPosition position) = 0;

      /// Store a committed tx hash.
      virtual void committedTxHash(
          const shared_model::interface::types::HashType
              &committed_tx_hash) = 0;

      /// Store a rejected tx hash.
      virtual void rejectedTxHash(
          const shared_model::interface::types::HashType &rejected_tx_hash) = 0;

      /// Index tx position by creator account.
      virtual void txPositionByCreator(
          const shared_model::interface::types::AccountIdType creator,
          TxPosition position) = 0;

      /// Index account asset tx position by involved account and asset.
      virtual void accountAssetTxPosition(
          const shared_model::interface::types::AccountIdType &account_id,
          const shared_model::interface::types::AssetIdType &asset_id,
          TxPosition position) = 0;

      /**
       * Flush the indices to storage.
       * Makes the effects of new indices (that were created before this call)
       * visible to other components.
       * @return Void Value on success, string Error on failure.
       */
      virtual iroha::expected::Result<void, std::string> flush() = 0;
    };

  }  // namespace ametsuchi
}  // namespace iroha

#endif /* AMETSUCHI_INDEXER_HPP */
