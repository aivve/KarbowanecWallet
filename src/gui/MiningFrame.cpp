// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Copyright (c) 2016-2017 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QThread>
#include <QUrl>

#include "MiningFrame.h"
#include "MainWindow.h"
#include "Settings.h"
#include "WalletAdapter.h"
#include "NodeAdapter.h"
#include "CryptoNoteWrapper.h"
#include "CurrencyAdapter.h"
#include "Settings.h"

#include "ui_miningframe.h"

namespace WalletGui {

const quint32 HASHRATE_TIMER_INTERVAL = 1000;
const quint32 MINER_ROUTINE_TIMER_INTERVAL = 60000;

MiningFrame::MiningFrame(QWidget* _parent) : QFrame(_parent), m_ui(new Ui::MiningFrame), m_miner(new Miner(this)), m_soloHashRateTimerId(-1) {
  m_ui->setupUi(this);
  initCpuCoreList();

  QString connection = Settings::instance().getConnection();
  if (connection.compare("remote") == 0) {
    m_ui->m_startSolo->setDisabled(true);
  }

  m_ui->m_startSolo->setEnabled(false);

  m_ui->m_hashRateChart->addGraph();
  m_ui->m_hashRateChart->graph(0)->setScatterStyle(QCPScatterStyle::ssDot);
  m_ui->m_hashRateChart->graph(0)->setLineStyle(QCPGraph::lsLine);
  m_ui->m_hashRateChart->graph()->setPen(QPen(QRgb(0x34496d)));
  m_ui->m_hashRateChart->graph()->setBrush(QBrush(QRgb(0xcbdef7)));

  QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
  dateTicker->setDateTimeFormat("hh:mm:ss");
  m_ui->m_hashRateChart->xAxis->setTicker(dateTicker);

  m_ui->m_hashRateChart->yAxis->setRange(0, m_maxHr);
  m_ui->m_hashRateChart->yAxis->setLabel("Hashrate");

  // make top and right axes visible but without ticks and labels
  m_ui->m_hashRateChart->xAxis2->setVisible(true);
  m_ui->m_hashRateChart->yAxis2->setVisible(true);
  m_ui->m_hashRateChart->xAxis2->setTicks(false);
  m_ui->m_hashRateChart->yAxis2->setTicks(false);
  m_ui->m_hashRateChart->xAxis2->setTickLabels(false);
  m_ui->m_hashRateChart->yAxis2->setTickLabels(false);

  m_ui->m_hashRateChart->setBackground(QBrush(QRgb(0xeef5fc)));

  addPoint(QDateTime::currentDateTime().toTime_t(), 0);
  plot();

  m_ui->m_stakeMixinSlider->setValue(3);
  stakeMixinChanged(m_ui->m_stakeMixinSlider->value());

  connect(&WalletAdapter::instance(), &WalletAdapter::walletCloseCompletedSignal, this, &MiningFrame::walletClosed, Qt::QueuedConnection);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletInitCompletedSignal, this, &MiningFrame::walletOpened, Qt::QueuedConnection);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletSynchronizationCompletedSignal, this, &MiningFrame::enableSolo, Qt::QueuedConnection);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletActualBalanceUpdatedSignal, this, &MiningFrame::updateBalance, Qt::QueuedConnection);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletPendingBalanceUpdatedSignal, this, &MiningFrame::updatePendingBalance, Qt::QueuedConnection);
  connect(&NodeAdapter::instance(), &NodeAdapter::localBlockchainUpdatedSignal, this, &MiningFrame::onBlockHeightUpdated, Qt::QueuedConnection);
  connect(&*m_miner, &Miner::minerMessageSignal, this, &MiningFrame::updateMinerLog, Qt::QueuedConnection);
}

MiningFrame::~MiningFrame() {
  stopSolo();
}

void MiningFrame::addPoint(double x, double y)
{
  m_hX.append(x);
  m_hY.append(y);

  // scroll plot
  if (m_hX.size() > 1000 || m_hY.size() > 1000) {
    m_hX.pop_front();
    m_hY.pop_front();
  }
}

void MiningFrame::plot()
{
  m_maxHr = std::max<double>(m_maxHr, m_hY.at(m_hY.size()-1));
  m_ui->m_hashRateChart->graph(0)->setData(m_hX, m_hY);
  m_ui->m_hashRateChart->xAxis->setRange(m_hX.at(0), m_hX.at(m_hX.size()-1));
  m_ui->m_hashRateChart->yAxis->setRange(0, m_maxHr);
  m_ui->m_hashRateChart->replot();
  m_ui->m_hashRateChart->update();
}

void MiningFrame::enableSolo() {
  m_sychronized = true;
  if (!m_solo_mining) {
    m_ui->m_startSolo->setEnabled(true);
    if(Settings::instance().isMiningOnLaunchEnabled() && m_sychronized) {
      if (!m_solo_mining) {
        startSolo();
        m_ui->m_startSolo->setChecked(true);
      }
    }
  }
}

void MiningFrame::timerEvent(QTimerEvent* _event) {
  if (_event->timerId() == m_soloHashRateTimerId) {
    m_miner->merge_hr();
    quint32 soloHashRate = m_miner->get_speed();
    if (soloHashRate == 0) {
      return;
    }
    double kHashRate = soloHashRate / 1000.0;
    m_ui->m_soloLabel->setText(tr("Mining solo. Hashrate: %1 kH/s").arg(kHashRate));
    addPoint(QDateTime::currentDateTime().toTime_t(), kHashRate);
    plot();

    return;
  }
  if (_event->timerId() == m_minerRoutineTimerId) {
    m_miner->on_idle();
  }

  QFrame::timerEvent(_event);
}

void MiningFrame::initCpuCoreList() {
  quint16 threads = Settings::instance().getMiningThreads();
  int cpuCoreCount = QThread::idealThreadCount();
  if (cpuCoreCount == -1) {
      cpuCoreCount = 2;
  }

  for (int i = 0; i < cpuCoreCount; ++i) {
    m_ui->m_cpuCombo->addItem(QString::number(i + 1), i + 1);
  }

  if (threads > 0) {
    m_ui->m_cpuCombo->setCurrentIndex(m_ui->m_cpuCombo->findData(threads, Qt::DisplayRole));
  } else {
    m_ui->m_cpuCombo->setCurrentIndex((cpuCoreCount - 1) / 2);
  }
}

void MiningFrame::walletOpened() {
  if(m_solo_mining)
    stopSolo();

  m_wallet_closed = false;

  quint64 stake = NodeAdapter::instance().getStake();
  quint64 actualBalance = WalletAdapter::instance().getActualBalance();
  quint64 lockedBalance = WalletAdapter::instance().getPendingBalance();
  m_ui->m_stakeLabel->setText(CurrencyAdapter::instance().formatAmount(stake).remove(',') + ' ' + CurrencyAdapter::instance().getCurrencyTicker().toUpper());
  m_ui->m_availableLabel->setText(CurrencyAdapter::instance().formatAmount(actualBalance).remove(',') + ' ' + CurrencyAdapter::instance().getCurrencyTicker().toUpper());
  m_ui->m_lockedLabel->setText(CurrencyAdapter::instance().formatAmount(lockedBalance).remove(',') + ' ' + CurrencyAdapter::instance().getCurrencyTicker().toUpper());

  if(m_sychronized) {
    if (actualBalance > stake) {
      m_ui->m_stopSolo->isChecked();
      m_ui->m_stopSolo->setEnabled(false);
      m_ui->m_startSolo->setEnabled(true);
    }
  }

  m_walletAddress = WalletAdapter::instance().getAddress();

  if(Settings::instance().isMiningOnLaunchEnabled() && m_sychronized) {
    if (!m_solo_mining) {
      startSolo();
      m_ui->m_startSolo->setChecked(true);
    }
  }
}

void MiningFrame::walletClosed() {
  // allow mining after wallet is closed to it's address
  // but mining can't be started if there's no open wallet
  // stopSolo();
  m_wallet_closed = true;
  m_ui->m_startSolo->setEnabled(false);
  m_ui->m_stopSolo->isChecked();
}

void MiningFrame::startSolo() {
  m_miner->start(m_walletAddress.toStdString(), m_ui->m_cpuCombo->currentData().toUInt());
  m_ui->m_soloLabel->setText(tr("Starting solo mining..."));
  m_soloHashRateTimerId = startTimer(HASHRATE_TIMER_INTERVAL);
  m_minerRoutineTimerId = startTimer(MINER_ROUTINE_TIMER_INTERVAL);
  addPoint(QDateTime::currentDateTime().toTime_t(), 0);
  m_ui->m_startSolo->setEnabled(false);
  m_ui->m_stopSolo->setEnabled(true);
  m_solo_mining = true;
}

void MiningFrame::stopSolo() {
  if(m_solo_mining) {
    killTimer(m_soloHashRateTimerId);
    m_soloHashRateTimerId = -1;
    killTimer(m_minerRoutineTimerId);
    m_minerRoutineTimerId = -1;
    m_miner->stop();
    addPoint(QDateTime::currentDateTime().toTime_t(), 0);
    m_ui->m_soloLabel->setText(tr("Stopped"));
  }
}

void MiningFrame::startStopSoloClicked(QAbstractButton* _button) {
  if (_button == m_ui->m_startSolo && m_ui->m_startSolo->isChecked() && m_wallet_closed != true) {
    startSolo();
  } else if (m_wallet_closed == true && _button == m_ui->m_stopSolo && m_ui->m_stopSolo->isChecked()) {
    m_ui->m_startSolo->setEnabled(false);
    stopSolo();
  } else if (_button == m_ui->m_stopSolo && m_ui->m_stopSolo->isChecked()) {
    stopSolo();
  }
}

void MiningFrame::setMiningThreads() {
  Settings::instance().setMiningThreads(m_ui->m_cpuCombo->currentText().toInt());
}

void MiningFrame::onBlockHeightUpdated() {
  m_miner->on_block_chain_update();
  m_ui->m_stakeLabel->setText(CurrencyAdapter::instance().formatAmount(NodeAdapter::instance().getStake()).remove(',') + ' ' + CurrencyAdapter::instance().getCurrencyTicker().toUpper());
}

void MiningFrame::updateBalance(quint64 _balance) {
  quint64 stake = NodeAdapter::instance().getStake();
  if (_balance < stake) {
    m_ui->m_startSolo->setEnabled(false);
    stopSolo();
    m_ui->m_soloLabel->setText(tr("Not enough balance for mining with stake %1").arg(CurrencyAdapter::instance().formatAmount(stake)));
  }
  m_ui->m_availableLabel->setText(CurrencyAdapter::instance().formatAmount(_balance).remove(',') + ' ' + CurrencyAdapter::instance().getCurrencyTicker().toUpper());
  m_ui->m_stakeLabel->setText(CurrencyAdapter::instance().formatAmount(stake).remove(',') + ' ' + CurrencyAdapter::instance().getCurrencyTicker().toUpper());
}

void MiningFrame::updatePendingBalance(quint64 _balance) {
  m_ui->m_lockedLabel->setText(CurrencyAdapter::instance().formatAmount(_balance).remove(',') + ' ' + CurrencyAdapter::instance().getCurrencyTicker().toUpper());
}

void MiningFrame::stakeMixinChanged(int _value) {
  m_miner->stakeMixinChanged(_value);
  m_ui->m_mixinLabel->setText(QString::number(_value));
}

void MiningFrame::updateMinerLog(const QString& _message) {
  QString message = _message + "\n";
  m_miner_log += message;

  m_ui->m_minerLog->setPlainText(m_miner_log);

  QScrollBar *sb = m_ui->m_minerLog->verticalScrollBar();
  sb->setValue(sb->maximum());
}

}
