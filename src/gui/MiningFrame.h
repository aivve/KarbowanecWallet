// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Copyright (c) 2016-2017 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QFrame>
#include "qcustomplot.h"
#include "Miner.h"

class QAbstractButton;

namespace Ui {
class MiningFrame;
}

namespace WalletGui {

class MiningFrame : public QFrame {
  Q_OBJECT

public:
  MiningFrame(QWidget* _parent);
  ~MiningFrame();

  void addPoint(double x, double y);
  void plot();

protected:
  void timerEvent(QTimerEvent* _event) Q_DECL_OVERRIDE;

private:
  QScopedPointer<Ui::MiningFrame> m_ui;
  int m_soloHashRateTimerId;
  int m_minerRoutineTimerId;
  QString m_walletAddress;
  QVector<double> m_hX, m_hY;
  miner* m_miner;

  void initCpuCoreList();
  void startSolo();
  void stopSolo();

  bool m_wallet_closed = false;
  bool m_solo_mining = false;
  bool m_sychronized = false;

  void walletOpened();
  void walletClosed();
  bool isSoloRunning() const;
  quint32 getHashRate() const;
  double m_maxHr = 0.5;

  Q_SLOT void startStopSoloClicked(QAbstractButton* _button);
  Q_SLOT void enableSolo();
  Q_SLOT void setMiningThreads();
};

}
