/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "main/raw_block_loader.hpp"

#include <gtest/gtest.h>
#include "framework/test_logger.hpp"
#include "interfaces/iroha_internal/block.hpp"
#include "interfaces/transaction.hpp"

using iroha::main::BlockLoader;

/**
 * @given block in json format
 * @when converting json to block using raw block loader
 * @then check that the block is correct
 */
TEST(BlockLoaderTest, BlockLoaderJsonParsing) {
  BlockLoader loader(getTestLogger("BlockLoader"));
  auto str =
      R"({
"block_v1": {
  "payload": {
    "transactions": [],
    "height": 1,
    "prev_block_hash": "0101010101010101010101010101010101010101010101010101010101010101",
    "created_time": 0
    },
  "signatures": []
}
})";

  auto block = loader.parseBlock(str);

  ASSERT_TRUE(block);
  auto b = block.value();

  ASSERT_EQ(b->transactions().size(), 0);
  ASSERT_EQ(b->height(), 1);
  ASSERT_EQ(b->createdTime(), 0);
  ASSERT_TRUE(b->signatures().empty());
  ASSERT_EQ(b->prevHash().hex(),
            "0101010101010101010101010101010101010101010101010101010101010101");
}
