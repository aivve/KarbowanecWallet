// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QFrame>
#include <QMenu>
#include <QWidget>
#include <QAbstractItemModel>

#include <IWalletLegacy.h>

namespace Ui {
class CoinsFrame;
}

namespace WalletGui {

class DnsManager;
class VisibleOutputsModel;

class CoinsFrame : public QFrame {
  Q_OBJECT

public:
  CoinsFrame(QWidget* _parent);
  ~CoinsFrame();

  QModelIndex m_index;

  enum TypeEnum
  {
      AllTypes,
      Spent,
      Unspent
  };

public slots:
  void onCustomContextMenu(const QPoint &point);
  void chooseType(int idx);
  void changedSearchFor(const QString &searchstring);

public Q_SLOTS:
  void copyHash();
  void copyKey();
  void copyGindex();
  void showDetails();
  void computeSelected();
  void walletClosed();
  void sendClicked();

protected:
  void timerEvent(QTimerEvent* _event) Q_DECL_OVERRIDE;

private:
  QScopedPointer<Ui::CoinsFrame> m_ui;
  QScopedPointer<VisibleOutputsModel> m_visibleOutputsModel;
  QMenu* contextMenu;
  DnsManager* m_aliasProvider;

  int m_addressInputTimer;
  std::list<CryptoNote::TransactionOutputInformation> m_selectedTransfers;
  quint64 m_selectedAmount;

  void currentOutputChanged(const QModelIndex& _currentIndex);
  void onAliasFound(const QString& _name, const QString& _address);
  void sendTxCompleted(CryptoNote::TransactionId _transactionId, bool _error, const QString& _errorText);
  void resetSendForm();

  QString extractAddress(const QString& _addressString) const;

  Q_SLOT void outputDoubleClicked(const QModelIndex& _index);
  Q_SLOT void addressEdited(const QString& _text);
  Q_SLOT void addressBookClicked();
  Q_SLOT void pasteClicked();
  
private slots:
  void resetFilterClicked();

};

}
