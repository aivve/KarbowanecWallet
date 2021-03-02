// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QApplication>
#include <QClipboard>
#include <QDateTime>

#include "Common/FormatTools.h"
#include "CoinsFrame.h"
#include "MainWindow.h"
#include "AddressBookDialog.h"
#include "OutputDetailsDialog.h"
#include "OutputsModel.h"
#include "SortedOutputsModel.h"
#include "VisibleOutputsModel.h"
#include "CurrencyAdapter.h"
#include "DnsLookup.h"
#include "NodeAdapter.h"
#include "WalletAdapter.h"
#include "WalletEvents.h"
#include "IWalletLegacy.h"
#include "ConfirmSendDialog.h"

#include "ui_coinsframe.h"

namespace WalletGui {

Q_DECL_CONSTEXPR quint32 ADDRESS_INPUT_INTERVAL = 1500;
Q_DECL_CONSTEXPR quint32 MINUTE_SECONDS = 60;
Q_DECL_CONSTEXPR quint32 HOUR_SECONDS = 60 * MINUTE_SECONDS;

CoinsFrame::CoinsFrame(QWidget* _parent) : QFrame(_parent), m_ui(new Ui::CoinsFrame),
  m_visibleOutputsModel(new VisibleOutputsModel), m_aliasProvider(new DnsManager(this)), m_addressInputTimer(-1),
  m_selectedTransfers({}), m_selectedAmount(0)
{
  m_ui->setupUi(this);
  m_visibleOutputsModel->setDynamicSortFilter(true);
  m_ui->m_outputsView->setModel(m_visibleOutputsModel.data());
  m_ui->m_outputsView->header()->setSectionResizeMode(QHeaderView::Interactive);
  m_ui->m_outputsView->header()->setSectionResizeMode(0, QHeaderView::Fixed);
  m_ui->m_outputsView->header()->resizeSection(0, 30);
  m_ui->m_outputsView->header()->resizeSection(1, 30);
  m_ui->m_outputsView->header()->resizeSection(2, 200);
  m_ui->m_outputsView->header()->resizeSection(3, 200);
  m_ui->m_outputsView->header()->resizeSection(6, 50);
  m_ui->m_outputsView->header()->resizeSection(7, 50);

  connect(m_ui->m_outputsView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CoinsFrame::currentOutputChanged);
  connect(m_ui->m_outputsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CoinsFrame::computeSelected);

  m_ui->m_outputsView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_ui->m_outputsView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onCustomContextMenu(const QPoint &)));
  connect(&WalletAdapter::instance(), &WalletAdapter::walletCloseCompletedSignal, this, &CoinsFrame::walletClosed);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletSendTransactionCompletedSignal, this, &CoinsFrame::sendTxCompleted, Qt::QueuedConnection);
  connect(m_aliasProvider, &DnsManager::aliasFoundSignal, this, &CoinsFrame::onAliasFound);

  contextMenu = new QMenu();
  contextMenu->addAction(QString(tr("Copy transaction &hash")), this, SLOT(copyHash()));
  contextMenu->addAction(QString(tr("Copy &key")), this, SLOT(copyKey()));
  contextMenu->addAction(QString(tr("Copy &global index")), this, SLOT(copyGindex()));
  contextMenu->addAction(QString(tr("Show &details")), this, SLOT(showDetails()));

  m_ui->m_typeSelect->addItem(tr("All types"), AllTypes);
  m_ui->m_typeSelect->addItem(tr("Spent"), Spent);
  m_ui->m_typeSelect->addItem(tr("Unspent"), Unspent);
  
  connect(m_ui->m_typeSelect, SIGNAL(activated(int)), this, SLOT(chooseType(int)));
  connect(m_ui->m_searchFor, SIGNAL(textChanged(QString)), this, SLOT(changedSearchFor(QString)));

  // set sorting to include all types of transactions
  SortedOutputsModel::instance().setState(-1);

  resetSendForm();
}

CoinsFrame::~CoinsFrame() {
}

void CoinsFrame::currentOutputChanged(const QModelIndex& _currentIndex) {

}

void CoinsFrame::outputDoubleClicked(const QModelIndex& _index) {
  if (!_index.isValid()) {
    return;
  }

  OutputDetailsDialog dlg(_index, &MainWindow::instance());
  if (dlg.exec() == QDialog::Accepted) {
    // do nothing
  }
}

void CoinsFrame::onCustomContextMenu(const QPoint &point)
{
  m_index = m_ui->m_outputsView->indexAt(point);
  contextMenu->exec(m_ui->m_outputsView->mapToGlobal(point));
}

void CoinsFrame::showDetails()
{
  outputDoubleClicked(m_index);
}

void CoinsFrame::copyHash()
{
  QApplication::clipboard()->setText(m_index.sibling(m_index.row(), 3).data().toString());
}

void CoinsFrame::copyKey()
{
  QApplication::clipboard()->setText(m_index.sibling(m_index.row(), 2).data().toString());
}

void CoinsFrame::copyGindex()
{
  QApplication::clipboard()->setText(m_index.sibling(m_index.row(), 5).data().toString());
}

void CoinsFrame::chooseType(int idx)
{
    if(!m_visibleOutputsModel)
        return;

    switch(m_ui->m_typeSelect->itemData(idx).toInt())
    {
    case AllTypes:
        SortedOutputsModel::instance().setState(-1);
        break;
    case Spent:
        SortedOutputsModel::instance().setState(0);
        break;
    case Unspent:
        SortedOutputsModel::instance().setState(1);
        break;
    }
}

void CoinsFrame::changedSearchFor(const QString &searchstring)
{
  if(!m_visibleOutputsModel)
     return;
  SortedOutputsModel::instance().setSearchFor(searchstring);
}

void CoinsFrame::resetFilterClicked() {
  m_ui->m_searchFor->clear();
  m_ui->m_typeSelect->setCurrentIndex(0);
  SortedOutputsModel::instance().setState(-1);
  m_ui->m_outputsView->clearSelection();
}

void CoinsFrame::walletClosed() {
  QString amountText = QString::number(0, 'f', 12) + " " + CurrencyAdapter::instance().getCurrencyTicker().toUpper();
  m_ui->m_selectedAmount->setText(amountText);
}

void CoinsFrame::computeSelected() {
  double amount = 0;
  if(!m_ui->m_outputsView->selectionModel())
    return;

    QModelIndexList selection = m_ui->m_outputsView->selectionModel()->selectedRows();

    foreach (QModelIndex index, selection) {
        QString amountstring = index.sibling(index.row(), OutputsModel::COLUMN_AMOUNT).data().toString().remove(',');
        amount += amountstring.toDouble();
    }
    QString amountText = QString::number(amount, 'f', 12) + " " + CurrencyAdapter::instance().getCurrencyTicker().toUpper();
    m_ui->m_selectedAmount->show();
    m_ui->m_selectedAmount->setText(amountText);
}

void CoinsFrame::sendClicked() {
  if (!WalletAdapter::instance().isOpen()) {
    return;
  }

  if(!m_ui->m_outputsView->selectionModel())
    return;

  QModelIndexList selection = m_ui->m_outputsView->selectionModel()->selectedRows();

  m_selectedAmount = 0;

  foreach (QModelIndex index, selection) {
    CryptoNote::TransactionOutputInformation o = OutputsModel::instance().getOutput(index);
    if (o.transactionHash != CryptoNote::NULL_HASH && o.amount != 0) {
      m_selectedTransfers.push_back(o);
      m_selectedAmount += o.amount;
    }
  }

  std::vector<CryptoNote::WalletLegacyTransfer> transfers;

  QString address = extractAddress(m_ui->m_addressEdit->text().trimmed());
  CryptoNote::WalletLegacyTransfer dest;
  dest.address = address.toStdString();
  quint64 fee = NodeAdapter::instance().getMinimalFee();
  quint64 amountMinusFee = m_selectedAmount - fee;
  dest.amount = amountMinusFee;
  transfers.push_back(dest);

  ConfirmSendDialog dlg(&MainWindow::instance());
  dlg.showPasymentDetails(amountMinusFee);

  if (dlg.exec() == QDialog::Accepted) {
    if (WalletAdapter::instance().isOpen()) {
        WalletAdapter::instance().sendTransaction(transfers, m_selectedTransfers, fee, QString(), 0);
    }
  }
}

void CoinsFrame::resetSendForm() {
  m_ui->m_addressEdit->clear();
}

void CoinsFrame::sendTxCompleted(CryptoNote::TransactionId _transactionId, bool _error, const QString& _errorText) {
  Q_UNUSED(_transactionId);
  if (_error) {
    QCoreApplication::postEvent(
      &MainWindow::instance(),
      new ShowMessageEvent(_errorText, QtCriticalMsg));
  } else {
    resetSendForm();
  }
}

QString CoinsFrame::extractAddress(const QString& _addressString) const {
  QString address = _addressString;
  if (_addressString.contains('<')) {
    int startPos = _addressString.indexOf('<');
    int endPos = _addressString.indexOf('>');
    address = _addressString.mid(startPos + 1, endPos - startPos - 1);
  }

  return address;
}

void CoinsFrame::pasteClicked() {
  m_ui->m_addressEdit->setText(QApplication::clipboard()->text());
}

void CoinsFrame::addressBookClicked() {
  AddressBookDialog dlg(&MainWindow::instance());
  if(dlg.exec() == QDialog::Accepted) {
    m_ui->m_addressEdit->setText(dlg.getAddress());
  }
}

void CoinsFrame::timerEvent(QTimerEvent* _event) {
  if (_event->timerId() == m_addressInputTimer) {
    if (m_ui->m_addressEdit->text().trimmed().contains('.'))
        m_aliasProvider->getAddresses(m_ui->m_addressEdit->text().trimmed());
    killTimer(m_addressInputTimer);
    m_addressInputTimer = -1;
    return;
  }

  QFrame::timerEvent(_event);
}

void CoinsFrame::onAliasFound(const QString& _name, const QString& _address) {
  m_ui->m_addressEdit->setText(QString("%1 <%2>").arg(_name).arg(_address));
}

void CoinsFrame::addressEdited(const QString& _text) {
  if(!_text.isEmpty() && _text.contains('.')) {
    if (m_addressInputTimer != -1) {
      killTimer(m_addressInputTimer);
    }
    m_addressInputTimer = startTimer(ADDRESS_INPUT_INTERVAL);
  }
}

}
