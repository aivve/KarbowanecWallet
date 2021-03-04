// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2016-2020 The Karbo developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QFrame>
#include <QLabel>
#include <QMenu>

namespace Ui {
class AccountFrame;
}

namespace WalletGui {

class AccountFrame : public QFrame {
  Q_OBJECT
  Q_DISABLE_COPY(AccountFrame)

public:
  AccountFrame(QWidget* _parent);
  ~AccountFrame();

  void showQRCode(const QString& _dataString);
  QImage exportImage();

Q_SIGNALS:
  void clicked();

public Q_SLOTS:
  void saveImage();
  void copyImage();

protected:
  virtual void mousePressEvent(QMouseEvent* event);
  virtual void contextMenuEvent(QContextMenuEvent* event);

private:
  QScopedPointer<Ui::AccountFrame> m_ui;

  QMenu* contextMenu;

  void updateWalletAddress(const QString& _address);
  void updateActualBalance(quint64 _balance);
  void updatePendingBalance(quint64 _balance);
  void updateUnmixableBalance(quint64 _balance);
  void reset();

  QStringList divideAmount(quint64 _val);

  Q_SLOT void copyAddress();

Q_SIGNALS:
  void showQRcodeSignal();

};

}
