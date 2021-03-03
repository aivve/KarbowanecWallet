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
#include "OutputDetailsDialog.h"
#include "OutputsModel.h"
#include "SortedOutputsModel.h"
#include "VisibleOutputsModel.h"
#include "CurrencyAdapter.h"
#include "NodeAdapter.h"
#include "WalletAdapter.h"
#include "WalletEvents.h"
#include "IWalletLegacy.h"

#include "ui_coinsframe.h"

namespace WalletGui {

CoinsFrame::CoinsFrame(QWidget* _parent) : QFrame(_parent), m_ui(new Ui::CoinsFrame),
  m_visibleOutputsModel(new VisibleOutputsModel)
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
  if (!m_ui->m_outputsView->selectionModel() || !WalletAdapter::instance().isOpen())
    return;

  QModelIndexList selection = m_ui->m_outputsView->selectionModel()->selectedRows();
  std::list<CryptoNote::TransactionOutputInformation> selectedOutputs;

  foreach (QModelIndex index, selection) {
    CryptoNote::TransactionOutputInformation o = OutputsModel::instance().getOutput(index);
    if (o.transactionHash != CryptoNote::NULL_HASH && o.amount != 0) {
      selectedOutputs.push_back(o);
    }
  }

  Q_EMIT sendOutputsSignal(selectedOutputs);
}

}
