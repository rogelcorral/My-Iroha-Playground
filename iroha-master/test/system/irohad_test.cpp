/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <soci/postgresql/soci-postgresql.h>
#include <soci/soci.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/process.hpp>
#include <boost/variant.hpp>

#include "backend/protobuf/query_responses/proto_query_response.hpp"
#include "common/bind.hpp"
#include "common/files.hpp"
#include "crypto/keys_manager_impl.hpp"
#include "framework/result_gtest_checkers.hpp"
#include "integration/acceptance/acceptance_fixture.hpp"
#include "interfaces/query_responses/roles_response.hpp"
#include "logger/logger.hpp"
#include "logger/logger_manager.hpp"
#include "main/impl/pg_connection_init.hpp"
#include "main/iroha_conf_literals.hpp"
#include "main/iroha_conf_loader.hpp"
#include "network/impl/grpc_channel_builder.hpp"
#include "torii/command_client.hpp"
#include "torii/query_client.hpp"

// workaround for redefining -WERROR problem
#undef RAPIDJSON_HAS_STDSTRING

#include "framework/config_helper.hpp"

using namespace boost::process;
using namespace boost::filesystem;
using namespace std::chrono_literals;
using namespace common_constants;
using iroha::operator|;

static logger::LoggerManagerTreePtr getIrohadTestLoggerManager() {
  static logger::LoggerManagerTreePtr irohad_test_logger_manager;
  if (!irohad_test_logger_manager) {
    irohad_test_logger_manager =
        std::make_shared<logger::LoggerManagerTree>(logger::LoggerConfig{
            logger::LogLevel::kInfo, logger::getDefaultLogPatterns()});
  }
  return irohad_test_logger_manager->getChild("IrohadTest");
}

class IrohadTest : public AcceptanceFixture {
 public:
  IrohadTest()
      : kAddress("127.0.0.1"),
        kPort(50051),
        test_data_path_(boost::filesystem::path(PATHTESTDATA)),
        keys_manager_(
            kAdminId,
            test_data_path_,
            getIrohadTestLoggerManager()->getChild("KeysManager")->getLogger()),
        log_(getIrohadTestLoggerManager()->getLogger()) {}

  void SetUp() override {
    setPaths();

    rapidjson::Document doc;
    std::ifstream ifs_iroha(path_config_.string());
    rapidjson::IStreamWrapper isw(ifs_iroha);
    doc.ParseStream(isw);
    ASSERT_FALSE(doc.HasParseError())
        << "Failed to parse irohad config at " << path_config_.string();
    db_name_ = integration_framework::getRandomDbName();
    pgopts_ = "dbname=" + db_name_ + " "
        + integration_framework::getPostgresCredsFromEnv().value_or(
              doc[config_members::PgOpt].GetString());
    // we need a separate file here in case if target environment
    // has custom database connection options set
    // via environment variables
    doc[config_members::PgOpt].SetString(pgopts_.data(), pgopts_.size());
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    std::string config_copy_string = sb.GetString();
    std::ofstream copy_file(config_copy_);
    copy_file.write(config_copy_string.data(), config_copy_string.size());
  }

  void launchIroha() {
    launchIroha(setDefaultParams());
  }

  void launchIroha(const std::string &parameters) {
    iroha_process_.emplace(irohad_executable.string() + parameters);
    auto channel = grpc::CreateChannel(kAddress + ":" + std::to_string(kPort),
                                       grpc::InsecureChannelCredentials());
    auto state = channel->GetState(true);
    auto deadline = std::chrono::system_clock::now() + kTimeout;
    while (state != grpc_connectivity_state::GRPC_CHANNEL_READY
           and deadline > std::chrono::system_clock::now()) {
      channel->WaitForStateChange(state, deadline);
      state = channel->GetState(true);
    }
    ASSERT_EQ(state, grpc_connectivity_state::GRPC_CHANNEL_READY);
    ASSERT_TRUE(iroha_process_->running());
  }

  void launchIroha(const boost::optional<std::string> &config_path,
                   const boost::optional<std::string> &genesis_block,
                   const boost::optional<std::string> &keypair_path,
                   const boost::optional<std::string> &additional_params) {
    launchIroha(
        params(config_path, genesis_block, keypair_path, additional_params));
  }

  int getBlockCount() {
    int block_count = 0;
    auto sql =
        std::make_unique<soci::session>(*soci::factory_postgresql(), pgopts_);
    *sql << "SELECT COUNT(*) FROM blocks;", soci::into(block_count);
    return block_count;
  }

  void TearDown() override {
    if (iroha_process_) {
      iroha_process_->terminate();
    }

    framework::expected::assertResultValue(
        iroha::ametsuchi::PgConnectionInit::dropWorkingDatabase(
            iroha::ametsuchi::PostgresOptions{pgopts_, db_name_, log_}));
    boost::filesystem::remove(config_copy_);
  }

  std::string params(const boost::optional<std::string> &config_path,
                     const boost::optional<std::string> &genesis_block,
                     const boost::optional<std::string> &keypair_path,
                     const boost::optional<std::string> &additional_params) {
    std::string res;
    config_path | [&res](auto &&s) { res += " --config " + s; };
    genesis_block | [&res](auto &&s) { res += " --genesis_block " + s; };
    keypair_path | [&res](auto &&s) { res += " --keypair_name " + s; };
    additional_params | [&res](auto &&s) { res += " " + s; };
    return res;
  }

  std::string setDefaultParams() {
    return params(
        config_copy_, path_genesis_.string(), path_keypair_.string(), {});
  }

  /**
   * Send default transaction with given key pair.
   * Method will wait until transaction reach COMMITTED status
   * OR until limit of attempts is exceeded.
   * @param key_pair Key pair for signing transaction
   * @return Response object from Torii
   */
  iroha::protocol::ToriiResponse sendDefaultTx(
      const shared_model::crypto::Keypair &key_pair) {
    iroha::protocol::TxStatusRequest tx_request;
    iroha::protocol::ToriiResponse torii_response;

    auto tx =
        complete(baseTx(kAdminId).setAccountQuorum(kAdminId, 1), key_pair);
    tx_request.set_tx_hash(tx.hash().hex());

    torii::CommandSyncClient client(
        iroha::network::createClient<iroha::protocol::CommandService_v1>(
            kAddress + ":" + std::to_string(kPort)),
        getIrohadTestLoggerManager()->getChild("CommandClient")->getLogger());
    client.Torii(tx.getTransport());

    auto resub_counter(resubscribe_attempts);
    constexpr auto committed_status = iroha::protocol::TxStatus::COMMITTED;
    do {
      std::this_thread::sleep_for(resubscribe_timeout);
      client.Status(tx_request, torii_response);
    } while (torii_response.tx_status() != committed_status
             and --resub_counter);

    return torii_response;
  }

  /**
   * Sending default transaction and assert that it was finished with
   * COMMITED status.
   * Method will wait until transaction reach COMMITTED status
   * OR until limit of attempts is exceeded.
   * @param key_pair Key pair for signing transaction
   */
  void sendDefaultTxAndCheck(const shared_model::crypto::Keypair &key_pair) {
    iroha::protocol::ToriiResponse torii_response;
    torii_response = sendDefaultTx(key_pair);
    ASSERT_EQ(torii_response.tx_status(), iroha::protocol::TxStatus::COMMITTED);
  }

 private:
  void setPaths() {
    path_irohad_ = boost::filesystem::path(PATHIROHAD);
    irohad_executable = path_irohad_ / "irohad";
    path_config_ = test_data_path_ / "config.sample";
    path_genesis_ = test_data_path_ / "genesis.block";
    path_keypair_ = test_data_path_ / "node0";
    config_copy_ = path_config_.string() + std::string(".copy");
  }

 public:
  boost::filesystem::path irohad_executable;
  const std::chrono::milliseconds kTimeout = 30s;
  const std::string kAddress;
  const uint16_t kPort;

  boost::optional<child> iroha_process_;

  /**
   * Command client resubscription settings
   *
   * The do-while loop imitates client resubscription to the stream. Stream
   * "expiration" is a valid designed case (see pr #1615 for the details).
   *
   * The number of attempts (5) is a magic constant here. The idea behind this
   * number is the following: five resubscription with 3 seconds timeout is
   * usually enough to pass the test; if not - most likely there is another bug.
   */
  const uint32_t resubscribe_attempts = 5;
  const std::chrono::seconds resubscribe_timeout = std::chrono::seconds(3);

 protected:
  boost::filesystem::path path_irohad_;
  boost::filesystem::path test_data_path_;
  boost::filesystem::path path_config_;
  boost::filesystem::path path_genesis_;
  boost::filesystem::path path_keypair_;
  std::string db_name_;
  std::string pgopts_;
  std::string config_copy_;
  iroha::KeysManagerImpl keys_manager_;

  logger::LoggerPtr log_;
};

/**
 * @given path to irohad executable and paths to files irohad is needed to be
 * run (config, genesis block, keypair)
 * @when run irohad with all parameters it needs to operate as a full node
 * @then irohad should be started and running until timeout expired
 */
TEST_F(IrohadTest, RunIrohad) {
  launchIroha();
}

/**
 * Test verifies that a transaction can be sent to running iroha and committed
 * @given running Iroha
 * @when a client sends a transaction to Iroha
 * @then the transaction is committed
 */
TEST_F(IrohadTest, SendTx) {
  launchIroha();

  auto key_pair = keys_manager_.loadKeys();
  ASSERT_TRUE(key_pair);

  SCOPED_TRACE("From send transaction test");
  sendDefaultTxAndCheck(key_pair.get());
}

/**
 * Test verifies that a query can be sent to and served by running Iroha
 * @given running Iroha
 * @when a client sends a query to Iroha
 * @then the query is served and query response is received
 */
TEST_F(IrohadTest, SendQuery) {
  launchIroha();

  auto key_pair = keys_manager_.loadKeys();
  ASSERT_TRUE(key_pair);

  iroha::protocol::QueryResponse response;
  auto query = complete(baseQry(kAdminId).getRoles(), key_pair.get());
  auto client = torii_utils::QuerySyncClient(kAddress, kPort);
  client.Find(query.getTransport(), response);
  shared_model::proto::QueryResponse resp{std::move(response)};

  ASSERT_NO_THROW(
      boost::get<const shared_model::interface::RolesResponse &>(resp.get()));
}

/**
 * Test verifies that after restarting with --overwrite-ledger flag Iroha
 * contain single genesis block in storage and Iroha can accept and serve
 * transactions
 * @given an Iroha with some transactions commited ontop of the genesis
 * block
 * @when the Iroha is restarted with --overwrite-ledger flag
 * @then the Iroha started with single genesis block in storage
 *  AND the Iroha accepts and able to commit new transactions
 */
TEST_F(IrohadTest, RestartWithOverwriteLedger) {
  launchIroha();

  auto key_pair = keys_manager_.loadKeys();
  ASSERT_TRUE(key_pair);

  SCOPED_TRACE("From restart with --overwrite-ledger flag test");
  sendDefaultTxAndCheck(key_pair.get());

  iroha_process_->terminate();

  launchIroha(config_copy_,
              path_genesis_.string(),
              path_keypair_.string(),
              std::string("--overwrite-ledger"));

  ASSERT_EQ(getBlockCount(), 1);

  SCOPED_TRACE("From restart with --overwrite-ledger flag test");
  sendDefaultTxAndCheck(key_pair.get());
}

/**
 * Test verifies that Iroha can accept and serve transactions after usual
 * restart
 * @given an Iroha with some transactions commited ontop of the genesis
 * block
 * @when the Iroha is restarted without --overwrite-ledger flag
 * @then the state is successfully restored
 *  AND the Iroha accepts and able to commit new transactions
 */
TEST_F(IrohadTest, RestartWithoutResetting) {
  launchIroha();

  auto key_pair = keys_manager_.loadKeys();
  ASSERT_TRUE(key_pair);

  SCOPED_TRACE("From restart without resetting test");
  sendDefaultTxAndCheck(key_pair.get());

  int height = getBlockCount();

  iroha_process_->terminate();

  launchIroha(config_copy_, {}, path_keypair_.string(), {});

  ASSERT_EQ(getBlockCount(), height);

  SCOPED_TRACE("From restart without resetting test");
  sendDefaultTxAndCheck(key_pair.get());
}
