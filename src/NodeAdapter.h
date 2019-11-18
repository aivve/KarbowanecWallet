// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2016-2017 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QObject>
#include <QThread>

#include <INode.h>
#include <IWalletLegacy.h>
#include "CryptoNoteWrapper.h"

namespace CryptoNote {

class Currency;

}

namespace Logging {
  class LoggerManager;
}

namespace WalletGui {

class InProcessNodeInitializer;

class NodeAdapter : public QObject, public INodeCallback {
  Q_OBJECT
  Q_DISABLE_COPY(NodeAdapter)

public:
  static NodeAdapter& instance();

  quintptr getPeerCount();
  std::string convertPaymentId(const QString& _payment_id_string) const;
  QString extractPaymentId(const std::string& _extra) const;
  CryptoNote::IWalletLegacy* createWallet() const;

  bool init();
  void deinit();
  quint64 getLastKnownBlockHeight() const;
  quint64 getLastLocalBlockHeight() const;
  QDateTime getLastLocalBlockTimestamp() const;
  quint64 getDifficulty();
  quint64 getTxCount();
  quint64 getTxPoolSize();
  quint64 getAltBlocksCount();
  quint64 getConnectionsCount();
  quint64 getOutgoingConnectionsCount();
  quint64 getIncomingConnectionsCount();
  quint64 getWhitePeerlistSize();
  quint64 getGreyPeerlistSize();
  quint64 getMinimalFee() const;
  uint8_t getCurrentBlockMajorVersion();
  CryptoNote::BlockHeaderInfo getLastLocalBlockHeaderInfo();
  void peerCountUpdated(Node& _node, size_t _count) Q_DECL_OVERRIDE;
  void localBlockchainUpdated(Node& _node, uint64_t _height) Q_DECL_OVERRIDE;
  void lastKnownBlockHeightUpdated(Node& _node, uint64_t _height) Q_DECL_OVERRIDE;
  void startSoloMining(QString _address, size_t _threads_count);
  void stopSoloMining();
  quint64 getSpeed() const;

  bool getStake(uint8_t blockMajorVersion, uint64_t fee, uint32_t& height, CryptoNote::difficulty_type& next_diff, size_t& medianSize, uint64_t& alreadyGeneratedCoins, size_t currentBlockSize, uint64_t& stake, uint64_t& blockReward);
  bool prepareBlockTemplate(CryptoNote::Block& b, uint64_t& fee, const CryptoNote::AccountPublicAddress& adr, CryptoNote::difficulty_type& diffic, uint32_t& height, const CryptoNote::BinaryArray& ex_nonce, size_t& median_size, size_t& txs_size, uint64_t& already_generated_coins);
  bool handleBlockFound(CryptoNote::Block& b);

private:
  Node* m_node;
  QThread m_nodeInitializerThread;
  InProcessNodeInitializer* m_nodeInitializer;

  NodeAdapter();
  ~NodeAdapter();

  bool initInProcessNode();
  CryptoNote::CoreConfig makeCoreConfig() const;
  CryptoNote::NetNodeConfig makeNetNodeConfig() const;

Q_SIGNALS:
  void localBlockchainUpdatedSignal(quint64 _height);
  void lastKnownBlockHeightUpdatedSignal(quint64 _height);
  void nodeInitCompletedSignal();
  void peerCountUpdatedSignal(quintptr _count);
  void initNodeSignal(Node** _node, const CryptoNote::Currency* currency, INodeCallback* _callback, Logging::LoggerManager* _loggerManager,
    const CryptoNote::CoreConfig& _coreConfig, const CryptoNote::NetNodeConfig& _netNodeConfig);
  void deinitNodeSignal(Node** _node);
  void connectionFailedSignal();
};

}
