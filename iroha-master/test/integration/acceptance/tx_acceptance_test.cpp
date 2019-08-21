/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <boost/variant.hpp>
#include "framework/integration_framework/integration_test_framework.hpp"
#include "integration/acceptance/acceptance_fixture.hpp"

using namespace common_constants;

class AcceptanceTest : public AcceptanceFixture {
 public:
  const std::function<void(const shared_model::proto::TransactionResponse &)>
      checkStatelessValidStatus = [](auto &status) {
        ASSERT_NO_THROW(
            boost::get<const shared_model::interface::StatelessValidTxResponse
                           &>(status.get()))
            << status.toString();
      };
  const std::function<void(
      const std::shared_ptr<const shared_model::interface::Proposal> &)>
      checkProposal =
          [](auto &proposal) { ASSERT_EQ(proposal->transactions().size(), 1); };
  const std::function<void(
      const std::shared_ptr<const shared_model::interface::Block> &)>
      checkStatefulValid =
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); };

  template <typename Builder = TestUnsignedTransactionBuilder>
  auto baseTx() {
    return Builder()
        .createdTime(getUniqueTime())
        .creatorAccountId(kAdminId)
        .addAssetQuantity(kAssetId, "1.0")
        .quorum(1);
  }
};

/**
 * TODO mboldyrev 18.01.2019 IR-227 convert to a separate status test
 * and a SFV integration test of non existing tx creator account
 * (seems not covered in postgres_executor_test or transaction_processor_test)
 *
 * @given non existent user
 * @when sending  transaction to the ledger
 * @then receive ENOUGH_SIGNATURES_COLLECTED status
 *       @and verified proposal is empty for that transaction
 */
TEST_F(AcceptanceTest, NonExistentCreatorAccountId) {
  const std::string kNonUser = "nonuser@test";
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(complete(baseTx<>().creatorAccountId(kNonUser), kAdminKeypair),
              checkStatelessValidStatus)
      .checkProposal(checkProposal)
      .checkVerifiedProposal(
          [](auto &proposal) { ASSERT_EQ(proposal->transactions().size(), 0); })
      .checkBlock(
          [](auto block) { ASSERT_EQ(block->transactions().size(), 0); });
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 remove, covered by field validator test
 *
 * @given some user
 * @when sending transactions with an 1 hour old UNIX time
 * @then receive ENOUGH_SIGNATURES_COLLECTED status
 *       AND STATEFUL_VALIDATION_SUCCESS on that tx
 */
TEST_F(AcceptanceTest, Transaction1HourOld) {
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(complete(baseTx<>().createdTime(
                           iroha::time::now(std::chrono::hours(-1))),
                       kAdminKeypair),
              checkStatelessValidStatus)
      .skipProposal()
      .skipVerifiedProposal()
      .checkBlock(checkStatefulValid);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 remove, covered by field validator test
 *
 * @given some user
 * @when sending transactions with an less than 24 hour old UNIX time
 * @then receive ENOUGH_SIGNATURES_COLLECTED status
 *       AND STATEFUL_VALIDATION_SUCCESS on that tx
 */
TEST_F(AcceptanceTest, DISABLED_TransactionLess24HourOld) {
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(complete(baseTx<>().createdTime(iroha::time::now(
                           std::chrono::hours(24) - std::chrono::minutes(1))),
                       kAdminKeypair),
              checkStatelessValidStatus)
      .skipProposal()
      .skipVerifiedProposal()
      .checkBlock(checkStatefulValid);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 remove, covered by field validator test
 *
 * @given some user
 * @when sending transactions with an more than 24 hour old UNIX time
 * @then receive STATELESS_VALIDATION_FAILED status
 */
TEST_F(AcceptanceTest, TransactionMore24HourOld) {
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(complete(baseTx<>().createdTime(iroha::time::now(
                           std::chrono::hours(24) + std::chrono::minutes(1))),
                       kAdminKeypair),
              CHECK_STATELESS_INVALID);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 remove, covered by field validator test
 *
 * @given some user
 * @when sending transactions with an less that 5 minutes from future UNIX time
 * @then receive ENOUGH_SIGNATURES_COLLECTED status
 *       AND STATEFUL_VALIDATION_SUCCESS on that tx
 */
TEST_F(AcceptanceTest, Transaction5MinutesFromFuture) {
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(complete(baseTx<>().createdTime(iroha::time::now(
                           std::chrono::minutes(5) - std::chrono::seconds(10))),
                       kAdminKeypair),
              checkStatelessValidStatus)
      .skipProposal()
      .skipVerifiedProposal()
      .checkBlock(checkStatefulValid);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 remove, covered by field validator test
 *
 * @given some user
 * @when sending transactions with an 10 minutes from future UNIX time
 * @then receive STATELESS_VALIDATION_FAILED status
 */
TEST_F(AcceptanceTest, Transaction10MinutesFromFuture) {
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(complete(baseTx<>().createdTime(
                           iroha::time::now(std::chrono::minutes(10))),
                       kAdminKeypair),
              CHECK_STATELESS_INVALID);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 remove, covered by field validator test
 *
 * @given some user
 * @when sending transactions with an empty public Key
 * @then receive STATELESS_VALIDATION_FAILED status
 */
TEST_F(AcceptanceTest, TransactionEmptyPubKey) {
  shared_model::proto::Transaction tx =
      baseTx<TestTransactionBuilder>().build();

  auto signedBlob = shared_model::crypto::CryptoSigner<>::sign(
      shared_model::crypto::Blob(tx.payload()), kAdminKeypair);
  tx.addSignature(signedBlob, shared_model::crypto::PublicKey(""));
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(tx, CHECK_STATELESS_INVALID);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 convert to a crypto provider unit test.
 * Also make a single SVL integration test including SignableModelValidator or
 * even whole torii::CommandServiceTransportGrpc and the crypto provider
 *
 * @given some user
 * @when sending transactions with an empty signedBlob
 * @then receive STATELESS_VALIDATION_FAILED status
 */
TEST_F(AcceptanceTest, TransactionEmptySignedblob) {
  shared_model::proto::Transaction tx =
      baseTx<TestTransactionBuilder>().build();
  tx.addSignature(shared_model::crypto::Signed(""), kAdminKeypair.publicKey());
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(tx, CHECK_STATELESS_INVALID);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 convert to a crypto provider unit test
 *
 * @given some user
 * @when sending transactions with correctly formed invalid PublicKey
 * @then receive STATELESS_VALIDATION_FAILED status
 */
TEST_F(AcceptanceTest, TransactionInvalidPublicKey) {
  shared_model::proto::Transaction tx =
      baseTx<TestTransactionBuilder>().build();
  auto signedBlob = shared_model::crypto::CryptoSigner<>::sign(
      shared_model::crypto::Blob(tx.payload()), kAdminKeypair);
  tx.addSignature(
      signedBlob,
      shared_model::crypto::PublicKey(std::string(
          shared_model::crypto::DefaultCryptoAlgorithmType::kPublicKeyLength,
          'a')));
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(tx, CHECK_STATELESS_INVALID);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 convert to a crypto provider unit test
 *
 * @given some user
 * @when sending transactions with Invalid SignedBlock
 * @then receive STATELESS_VALIDATION_FAILED status
 */
TEST_F(AcceptanceTest, TransactionInvalidSignedBlob) {
  shared_model::proto::Transaction tx =
      baseTx<TestTransactionBuilder>().build();

  auto signedBlob = shared_model::crypto::CryptoSigner<>::sign(
      shared_model::crypto::Blob(tx.payload()), kAdminKeypair);
  auto raw = signedBlob.blob();
  raw[0] = (raw[0] == std::numeric_limits<uint8_t>::max() ? 0 : raw[0] + 1);
  auto wrongBlob = shared_model::crypto::Signed(raw);

  tx.addSignature(wrongBlob, kAdminKeypair.publicKey());

  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(tx, CHECK_STATELESS_INVALID);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 remove, successful case covered by
 * higher-level tests
 *
 * @given some user
 * @when sending transactions with valid signature
 * @then receive ENOUGH_SIGNATURES_COLLECTED status
 *       AND STATEFUL_VALIDATION_SUCCESS on that tx
 */
TEST_F(AcceptanceTest, TransactionValidSignedBlob) {
  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(complete(baseTx<>(), kAdminKeypair), checkStatelessValidStatus)
      .skipProposal()
      .skipVerifiedProposal()
      .checkBlock(checkStatefulValid);
}

/**
 * TODO mboldyrev 18.01.2019 IR-227 convert to a SignableModelValidator test
 *
 * @given some user
 * @when sending transaction without any signature
 * @then the response is STATELESS_VALIDATION_FAILED
 */
TEST_F(AcceptanceTest, EmptySignatures) {
  auto proto_tx = baseTx<TestTransactionBuilder>().build().getTransport();
  proto_tx.clear_signatures();
  auto tx = shared_model::proto::Transaction(proto_tx);

  integration_framework::IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(tx, CHECK_STATELESS_INVALID);
}
