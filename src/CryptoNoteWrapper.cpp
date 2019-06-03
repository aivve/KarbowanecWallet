// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2016-2017 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CryptoNoteWrapper.h"
#include <CheckpointsData.h>
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/Checkpoints.h"
#include "NodeRpcProxy/NodeRpcProxy.h"
#include "CryptoNoteCore/DataBaseConfig.h"
#include "P2p/NetNodeConfig.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Miner.h"
#include "CryptoNoteCore/MinerConfig.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "Common/Util.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/DatabaseBlockchainCache.h"
#include "CryptoNoteCore/DatabaseBlockchainCacheFactory.h"
#include "CryptoNoteCore/DataBaseConfig.h"
#include "CryptoNoteCore/DataBaseErrors.h"
#include "CryptoNoteCore/MainChainStorage.h"
#include "CryptoNoteCore/IMainChainStorage.h"
#include "CryptoNoteCore/RocksDBWrapper.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "InProcessNode/InProcessNode.h"
#include "P2p/NetNode.h"
#include "WalletLegacy/WalletLegacy.h"
#include "Logging/LoggerManager.h"
#include "System/Dispatcher.h"
#include "CurrencyAdapter.h"
#include "Settings.h"

#include <QDebug>

namespace WalletGui {

namespace {

bool parsePaymentId(const std::string& payment_id_str, Crypto::Hash& payment_id) {
  return CryptoNote::parsePaymentId(payment_id_str, payment_id);
}

std::string convertPaymentId(const std::string& paymentIdString) {
  if (paymentIdString.empty()) {
    return "";
  }

  Crypto::Hash paymentId;
  if (!parsePaymentId(paymentIdString, paymentId)) {
    std::stringstream errorStr;
    errorStr << "Payment id has invalid format: \"" + paymentIdString + "\", expected 64-character string";
    throw std::runtime_error(errorStr.str());
  }

  std::vector<uint8_t> extra;
  CryptoNote::BinaryArray extraNonce;
  CryptoNote::setPaymentIdToTransactionExtraNonce(extraNonce, paymentId);
  if (!CryptoNote::addExtraNonceToTransactionExtra(extra, extraNonce)) {
    std::stringstream errorStr;
    errorStr << "Something went wrong with payment_id. Please check its format: \"" + paymentIdString + "\", expected 64-character string";
    throw std::runtime_error(errorStr.str());
  }

  return std::string(extra.begin(), extra.end());
}

std::string extractPaymentId(const std::string& extra) {
  std::vector<CryptoNote::TransactionExtraField> extraFields;
  std::vector<uint8_t> extraVector;
  std::copy(extra.begin(), extra.end(), std::back_inserter(extraVector));

  if (!CryptoNote::parseTransactionExtra(extraVector, extraFields)) {
    throw std::runtime_error("Can't parse extra");
  }

  std::string result;
  CryptoNote::TransactionExtraNonce extraNonce;
  if (CryptoNote::findTransactionExtraFieldByType(extraFields, extraNonce)) {
    Crypto::Hash paymentIdHash;
    if (CryptoNote::getPaymentIdFromTransactionExtraNonce(extraNonce.nonce, paymentIdHash)) {
      unsigned char* buff = reinterpret_cast<unsigned char *>(&paymentIdHash);
      for (size_t i = 0; i < sizeof(paymentIdHash); ++i) {
        result.push_back("0123456789ABCDEF"[buff[i] >> 4]);
        result.push_back("0123456789ABCDEF"[buff[i] & 15]);
      }
    }
  }

  return result;
}

inline std::string interpret_rpc_response(bool ok, const std::string& status) {
  std::string err;
  if (ok) {
    if (status == CORE_RPC_STATUS_BUSY) {
      err = "daemon is busy. Please try later";
    } else if (status != CORE_RPC_STATUS_OK) {
      err = status;
    }
  } else {
    err = "possible lost connection to daemon";
  }
  return err;
}

}

Node::~Node() {
}

class RpcNode : CryptoNote::INodeObserver, public Node {
public:
  Logging::LoggerManager& m_logManager;
  RpcNode(const CryptoNote::Currency& currency, INodeCallback& callback, Logging::LoggerManager& logManager, const std::string& nodeHost, unsigned short nodePort) :
    m_callback(callback),
    m_currency(currency),
    m_dispatcher(),
    m_logManager(logManager),
    m_node(nodeHost, nodePort, m_logManager) {
    m_node.addObserver(this);
  }

  ~RpcNode() override {
  }

  void init(const std::function<void(std::error_code)>& callback) override {
    m_node.init(callback);
  }

  void deinit() override {
  }

  uint64_t getSpeed() override {
    // return zero - go look in daemon
    return 0;
  }

  void startMining(const std::string& address, size_t threads_count) override {
    CryptoNote::COMMAND_RPC_START_MINING::request req;
    CryptoNote::COMMAND_RPC_START_MINING::response res;

    req.miner_address = address;
    req.threads_count = threads_count;

    try {
        //CryptoNote::HttpClient httpClient(m_dispatcher, "127.0.0.1", Settings::instance().getCurrentLocalDaemonPort());
        CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);

        CryptoNote::invokeJsonCommand(httpClient, "/start_mining", req, res);

        std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          qDebug() << "Mining started in daemon";
        else
          qDebug() << "Mining has NOT been started: " << QString::fromStdString(err);

      } catch (const CryptoNote::ConnectException&) {
        qDebug() << "Wallet failed to connect to daemon.";
      } catch (const std::exception& e) {
        qDebug() << "Failed to invoke rpc method: " << e.what();
      }
  }

  void stopMining() override {
      CryptoNote::COMMAND_RPC_STOP_MINING::request req;
      CryptoNote::COMMAND_RPC_STOP_MINING::response res;

      try {
         //CryptoNote::HttpClient httpClient(m_dispatcher, "127.0.0.1", Settings::instance().getCurrentLocalDaemonPort());
         CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);

          CryptoNote::invokeJsonCommand(httpClient, "/stop_mining", req, res);
          std::string err = interpret_rpc_response(true, res.status);
          if (err.empty())
            qDebug() << "Mining stopped in daemon";
          else
            qDebug() << "Mining has NOT been stopped: " << QString::fromStdString(err);
        } catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
        }
  }

  std::string convertPaymentId(const std::string& paymentIdString) override {
    return WalletGui::convertPaymentId(paymentIdString);
  }

  std::string extractPaymentId(const std::string& extra) override {
    return WalletGui::extractPaymentId(extra);
  }

  uint64_t getLastKnownBlockHeight() const override {
    return m_node.getLastKnownBlockHeight();
  }

  uint64_t getLastLocalBlockHeight() const override {
    return m_node.getLastLocalBlockHeight();
  }

  uint64_t getLastLocalBlockTimestamp() const override {
    return m_node.getLastLocalBlockTimestamp();
  }

  uint64_t getPeerCount() {
    return m_node.getPeerCount();
  }

  uint64_t getMinimalFee() {
    return m_node.getMinimalFee();
  }

  uint64_t getDifficulty() {
    try {
        CryptoNote::COMMAND_RPC_GET_INFO::request req;
        CryptoNote::COMMAND_RPC_GET_INFO::response res;
        CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
        CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
        std::string err = interpret_rpc_response(true, res.status);
      if (err.empty())
        return res.difficulty;
      else {
        qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
        return 0;
      }}
        catch (const CryptoNote::ConnectException&) {
        qDebug() << "Wallet failed to connect to daemon.";
        return 0;
      } catch (const std::exception& e) {
        qDebug() << "Failed to invoke rpc method: " << e.what();
        return 0;
      }
  }

  uint64_t getTxCount() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.tx_count;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  uint64_t getTxPoolSize() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.tx_pool_size;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  uint64_t getAltBlocksCount() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.alt_blocks_count;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  uint64_t getConnectionsCount() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.outgoing_connections_count + res.incoming_connections_count;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  uint64_t getOutgoingConnectionsCount() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.outgoing_connections_count;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  uint64_t getIncomingConnectionsCount() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.incoming_connections_count;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  uint64_t getWhitePeerlistSize() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.white_peerlist_size;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  uint64_t getGreyPeerlistSize() {
      try {
          CryptoNote::COMMAND_RPC_GET_INFO::request req;
          CryptoNote::COMMAND_RPC_GET_INFO::response res;
          CryptoNote::HttpClient httpClient(m_dispatcher, m_node.m_nodeHost, m_node.m_nodePort);
          CryptoNote::invokeJsonCommand(httpClient, "/getinfo", req, res);
          std::string err = interpret_rpc_response(true, res.status);
        if (err.empty())
          return res.grey_peerlist_size;
        else {
          qDebug() << "Failed to invoke request: " << QString::fromStdString(err);
          return 0;
        }}
          catch (const CryptoNote::ConnectException&) {
          qDebug() << "Wallet failed to connect to daemon.";
          return 0;
        } catch (const std::exception& e) {
          qDebug() << "Failed to invoke rpc method: " << e.what();
          return 0;
        }
  }

  CryptoNote::BlockHeaderInfo getLastLocalBlockHeaderInfo() {
    return m_node.getLastLocalBlockHeaderInfo();
  }

  uint8_t getCurrentBlockMajorVersion() {
    return getLastLocalBlockHeaderInfo().majorVersion;
  }

  CryptoNote::IWalletLegacy* createWallet() override {
    return new CryptoNote::WalletLegacy(m_currency, m_node, m_logManager);
  }

private:
  INodeCallback& m_callback;
  const CryptoNote::Currency& m_currency;
  CryptoNote::NodeRpcProxy m_node;
  System::Dispatcher m_dispatcher;

  void peerCountUpdated(size_t count) {
    m_callback.peerCountUpdated(*this, count);
  }

  void localBlockchainUpdated(uint64_t height) {
    m_callback.localBlockchainUpdated(*this, height);
  }

  void lastKnownBlockHeightUpdated(uint64_t height) {
    m_callback.lastKnownBlockHeightUpdated(*this, height);
  }
};


class InprocessNode : CryptoNote::INodeObserver, public Node {
public:
  InprocessNode(const CryptoNote::Currency& currency, 
    Logging::LoggerManager& logManager,
    CryptoNote::Checkpoints& checkpoints,
    const CryptoNote::NetNodeConfig& netNodeConfig,
    INodeCallback& callback) :
    m_currency(currency),
    m_dispatcher(),
    m_callback(callback),
    m_logManager(logManager),
    m_checkpoints(checkpoints),
    m_dbConfig(),
    m_database(logManager),
    m_netNodeConfig(netNodeConfig),
    m_core(currency, logManager, std::move(m_checkpoints), m_dispatcher,
      std::unique_ptr<CryptoNote::IBlockchainCacheFactory>(new CryptoNote::DatabaseBlockchainCacheFactory(m_database, m_logManager)),
      CryptoNote::createSwappedMainChainStorage(std::string(Settings::instance().getDataDir().absolutePath().toLocal8Bit().data()), currency)),
    m_protocolHandler(currency, m_dispatcher, m_core, nullptr, logManager),
    m_nodeServer(m_dispatcher, m_protocolHandler, logManager),
    m_node(m_core, m_protocolHandler, m_dispatcher) {

    //TODO: move to settings?
    m_dbConfig.setConfigFolderDefaulted(true);
    m_dbConfig.setDataDir(std::string(Settings::instance().getDataDir().absolutePath().toLocal8Bit().data()));
    m_dbConfig.setMaxOpenFiles(100);
    m_dbConfig.setReadCacheSize(128 * 1024 * 1024);
    m_dbConfig.setWriteBufferSize(128 * 1024 * 1024);
    m_dbConfig.setTestnet(false);
    m_dbConfig.setBackgroundThreadsCount(4);

    try {
      m_database.init(m_dbConfig);
      if (!CryptoNote::DatabaseBlockchainCache::checkDBSchemeVersion(m_database, logManager))
      {
        m_database.shutdown();
        m_database.destoy(m_dbConfig);
        m_database.init(m_dbConfig);
      }
    }
    catch (const std::system_error& _error) {
      if (_error.code().value() == static_cast<int>(CryptoNote::error::DataBaseErrorCodes::IO_ERROR)) {
        throw std::runtime_error("IO error");
      }
      throw std::runtime_error("Database in usage");
    }
    catch (const std::exception& _error) {
      throw std::runtime_error("Database initialization failed");
    }

    m_core.load();

    m_protocolHandler.set_p2p_endpoint(&m_nodeServer);

  }

  ~InprocessNode() override {

  }

  void init(const std::function<void(std::error_code)>& callback) override {
    try {
      if (!m_nodeServer.init(m_netNodeConfig)) {
        callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
        return;
      }
    } catch (std::runtime_error& _err) {
      callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
      return;
    }

    m_node.init([this, callback](std::error_code ec) {
      m_node.addObserver(this);
      callback(ec);
    });

    m_nodeServer.run();
    m_nodeServer.deinit();
    m_database.shutdown();
    m_database.destoy(m_dbConfig);
    m_node.shutdown();
  }

  void deinit() override {
    m_nodeServer.sendStopSignal();
  }

  void startMining(const std::string& address, size_t threads_count) override {
    //m_core.get_miner().start(CurrencyAdapter::instance().internalAddress(QString::fromStdString(address)), threads_count);
  }

  void stopMining() override {
    //m_core.get_miner().stop();
  }

  uint64_t getSpeed() override {
    //return m_core.get_miner().get_speed();
    return 0;
  }

  std::string convertPaymentId(const std::string& paymentIdString) override {
    return WalletGui::convertPaymentId(paymentIdString);
  }

  std::string extractPaymentId(const std::string& extra) override {
    return WalletGui::extractPaymentId(extra);
  }

  uint64_t getLastKnownBlockHeight() const override {
    return m_node.getLastKnownBlockHeight();
  }

  uint64_t getLastLocalBlockHeight() const override {
    return m_node.getLastLocalBlockHeight();
  }

  uint64_t getLastLocalBlockTimestamp() const override {
    return m_node.getLastLocalBlockTimestamp();
  }

  uint64_t getPeerCount() {
    return m_nodeServer.get_connections_count();
  }

  uint64_t getDifficulty() {
    return m_core.getDifficultyForNextBlock();
  }

  uint64_t getTxCount() {
    return m_core.getBlockchainTransactionCount() - m_core.get_current_blockchain_height();
  }

  uint64_t getTxPoolSize() {
    return m_core.getPoolTransactionCount();
  }

  uint64_t getAltBlocksCount() {
    return m_core.getAlternativeBlockCount();
  }

  uint64_t getConnectionsCount() {
    return m_nodeServer.get_connections_count();
  }

  uint64_t getOutgoingConnectionsCount() {
    return m_nodeServer.get_outgoing_connections_count();
  }

  uint64_t getIncomingConnectionsCount() {
    return m_nodeServer.get_connections_count() - m_nodeServer.get_outgoing_connections_count();
  }

  uint64_t getWhitePeerlistSize() {
    return m_nodeServer.getPeerlistManager().get_white_peers_count();
  }

  uint64_t getGreyPeerlistSize() {
    return m_nodeServer.getPeerlistManager().get_gray_peers_count();
  }

  uint64_t getMinimalFee() {
    return m_core.getMinimalFee();
  }

  CryptoNote::BlockHeaderInfo getLastLocalBlockHeaderInfo() {
    return m_node.getLastLocalBlockHeaderInfo();
  }

  uint8_t getCurrentBlockMajorVersion() {
    return getLastLocalBlockHeaderInfo().majorVersion;
  }

  CryptoNote::IWalletLegacy* createWallet() override {
    return new CryptoNote::WalletLegacy(m_currency, m_node, m_logManager);
  }

private:
  INodeCallback& m_callback;
  const CryptoNote::Currency& m_currency;
  System::Dispatcher m_dispatcher;
  Logging::LoggerManager& m_logManager;
  CryptoNote::Checkpoints& m_checkpoints;
  CryptoNote::RocksDBWrapper m_database;
  CryptoNote::DataBaseConfig m_dbConfig;
  CryptoNote::NetNodeConfig m_netNodeConfig;
  CryptoNote::Core m_core;
  CryptoNote::CryptoNoteProtocolHandler m_protocolHandler;
  CryptoNote::NodeServer m_nodeServer;
  CryptoNote::InProcessNode m_node;
  std::future<bool> m_nodeServerFuture;

  void peerCountUpdated(size_t count) {
    m_callback.peerCountUpdated(*this, count);
    //m_callback.peerCountUpdated(*this, m_nodeServer.get_connections_count() - 1);
  }

  void localBlockchainUpdated(uint64_t height) {
    m_callback.localBlockchainUpdated(*this, height);
  }

  void lastKnownBlockHeightUpdated(uint64_t height) {
    m_callback.lastKnownBlockHeightUpdated(*this, height);
  }
};

Node* createRpcNode(const CryptoNote::Currency& currency, INodeCallback& callback, Logging::LoggerManager& logManager,  const std::string& nodeHost, unsigned short nodePort) {
  return new RpcNode(currency, callback, logManager, nodeHost, nodePort);
}

Node* createInprocessNode(const CryptoNote::Currency& currency, Logging::LoggerManager& logManager,
  const CryptoNote::NetNodeConfig& netNodeConfig, INodeCallback& callback) {

  CryptoNote::Checkpoints checkpoints(logManager);
  for (const CryptoNote::CheckpointData& checkpoint : CryptoNote::CHECKPOINTS) {
    checkpoints.addCheckpoint(checkpoint.index, checkpoint.blockId);
  }
  checkpoints.loadCheckpointsFromDns();

  return new InprocessNode(currency, logManager, checkpoints, netNodeConfig, callback);
}

}
