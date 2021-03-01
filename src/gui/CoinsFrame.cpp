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
//#include "OutputDetailsDialog.h"
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
  m_ui->m_outputsView->header()->resizeSection(2, 250);
  m_ui->m_outputsView->header()->resizeSection(3, 250);

  connect(m_ui->m_outputsView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CoinsFrame::currentOutputChanged);

  m_ui->m_outputsView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_ui->m_outputsView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onCustomContextMenu(const QPoint &)));
 
  contextMenu = new QMenu();
  contextMenu->addAction(QString(tr("Copy &tx hash")), this, SLOT(copyHash()));
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

  /*OutputDetailsDialog dlg(_index, &MainWindow::instance());
  if (dlg.exec() == QDialog::Accepted) {
    
  }*/
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
  QApplication::clipboard()->setText(m_index.sibling(m_index.row(), 2).data().toString());
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

}
