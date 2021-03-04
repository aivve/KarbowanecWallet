// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2016-2021 The Karbo developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QDrag>
#include <QTimer>
#include <QImage>
#include <QMimeData>
#include <QClipboard>
#include <QFileDialog>
#include <QMouseEvent>
#include <QApplication>
#include <QFontDatabase>
#include "qrencode.h"
#include <QGraphicsDropShadowEffect>
#include "MainWindow.h"
#include "AccountFrame.h"
#include "WalletAdapter.h"
#include "CurrencyAdapter.h"

#include "ui_accountframe.h"

namespace WalletGui {

QStringList AccountFrame::divideAmount(quint64 _val) {
  QStringList list;
  QString str = CurrencyAdapter::instance().formatAmount(_val).remove(',');

  quint32 offset = str.indexOf(".") + 3; // add two digits .00
  QString before = str.left(offset);
  QString after  = str.mid(offset);

  list << before << after;

  return list;
}

AccountFrame::AccountFrame(QWidget* _parent) : QFrame(_parent), m_ui(new Ui::AccountFrame), contextMenu(0) {
  m_ui->setupUi(this);
  connect(&WalletAdapter::instance(), &WalletAdapter::updateWalletAddressSignal, this, &AccountFrame::updateWalletAddress);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletActualBalanceUpdatedSignal, this, &AccountFrame::updateActualBalance,
    Qt::QueuedConnection);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletPendingBalanceUpdatedSignal, this, &AccountFrame::updatePendingBalance,
    Qt::QueuedConnection);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletUnmixableBalanceUpdatedSignal, this, &AccountFrame::updateUnmixableBalance,
    Qt::QueuedConnection);
  connect(&WalletAdapter::instance(), &WalletAdapter::walletCloseCompletedSignal, this, &AccountFrame::reset);

  contextMenu = new QMenu();
  QAction* saveImageAction = new QAction(tr("&Save Image..."), this);
  connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));
  contextMenu->addAction(saveImageAction);
  QAction* copyImageAction = new QAction(tr("&Copy Image"), this);
  connect(copyImageAction, SIGNAL(triggered()), this, SLOT(copyImage()));
  contextMenu->addAction(copyImageAction);

  m_ui->m_unmixableBalanceLabel->setVisible(false);

  int id = QFontDatabase::addApplicationFont(":/fonts/mplusm");
  QString family = QFontDatabase::applicationFontFamilies(id).at(0);
  QFont monospace(family);
  monospace.setPixelSize(16);
  m_ui->m_addressLabel->setFont(monospace);
  // shadow under address
  QGraphicsDropShadowEffect *textShadow = new QGraphicsDropShadowEffect(this);
  textShadow->setBlurRadius(4.0);
  textShadow->setColor(QColor(0, 0, 0));
  textShadow->setOffset(0,1);
  m_ui->m_addressLabel->setGraphicsEffect(textShadow);
}

AccountFrame::~AccountFrame() {
}

void AccountFrame::updateWalletAddress(const QString& _address) {
  m_ui->m_addressLabel->setText(_address);
  showQRCode(_address);
}

void AccountFrame::copyAddress() {
  QApplication::clipboard()->setText(m_ui->m_addressLabel->text());
}

void AccountFrame::updateActualBalance(quint64 _balance) {
  QStringList actualList = divideAmount(_balance);
  m_ui->m_actualBalanceLabel->setText(QString(tr("<p style=\"height:30\">Available: <strong style=\"font-size:14px; color: #ffffff;\">%1</strong><small style=\"font-size:10px; color: #D3D3D3;\">%2 %3</small></p>")).arg(actualList.first()).arg(actualList.last()).arg(CurrencyAdapter::instance().getCurrencyTicker().toUpper()));

  quint64 pendingBalance = WalletAdapter::instance().getPendingBalance();

  QStringList pendingList = divideAmount(_balance + pendingBalance);
  m_ui->m_totalBalanceLabel->setText(QString(tr("<p style=\"height:30\">Total: <strong style=\"font-size:18px; color: #ffffff;\">%1</strong><small style=\"font-size:10px; color: #D3D3D3;\">%2 %3</small></p>")).arg(pendingList.first()).arg(pendingList.last()).arg(CurrencyAdapter::instance().getCurrencyTicker().toUpper()));
}

void AccountFrame::updatePendingBalance(quint64 _balance) {
  QStringList pendingList = divideAmount(_balance);
  m_ui->m_pendingBalanceLabel->setText(QString(tr("<p style=\"height:30\">Pending: <strong style=\"font-size:14px; color: #ffffff;\">%1</strong><small style=\"font-size:10px; color: #D3D3D3;\">%2 %3</small></p>")).arg(pendingList.first()).arg(pendingList.last()).arg(CurrencyAdapter::instance().getCurrencyTicker().toUpper()));

  quint64 actualBalance = WalletAdapter::instance().getActualBalance();

  QStringList totalList = divideAmount(_balance + actualBalance);
  m_ui->m_totalBalanceLabel->setText(QString(tr("<p style=\"height:30\">Total: <strong style=\"font-size:18px; color: #ffffff;\">%1</strong><small style=\"font-size:10px; color: #D3D3D3;\">%2 %3</small></p>")).arg(totalList.first()).arg(totalList.last()).arg(CurrencyAdapter::instance().getCurrencyTicker().toUpper()));
}

void AccountFrame::updateUnmixableBalance(quint64 _balance) {
  QStringList unmixableList = divideAmount(_balance);

  m_ui->m_unmixableBalanceLabel->setText(QString(tr("<p style=\"height:30\">Unmixable: <strong style=\"font-size:14px; color: #ffffff;\">%1</strong><small style=\"font-size:10px; color: #D3D3D3;\">%2 %3</small></p>")).arg(unmixableList.first()).arg(unmixableList.last()).arg(CurrencyAdapter::instance().getCurrencyTicker().toUpper()));
  if (_balance != 0) {
    m_ui->m_unmixableBalanceLabel->setVisible(true);
  } else {
    m_ui->m_unmixableBalanceLabel->setVisible(false);
  }
}

void AccountFrame::showQRCode(const QString& _dataString) {
  QRcode *qrcode = QRcode_encodeString(_dataString.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
  if (qrcode == nullptr) {
    return;
  }

  QImage qrCodeImage = QImage(qrcode->width + 8, qrcode->width + 8, QImage::Format_RGB32);
  qrCodeImage.fill(0xffffff);
  unsigned char *p = qrcode->data;
  for (int y = 0; y < qrcode->width; y++) {
    for (int x = 0; x < qrcode->width; x++) {
      qrCodeImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
      p++;
    }
  }

  QRcode_free(qrcode);
  m_ui->m_qrCodeLabel->setPixmap(QPixmap::fromImage(qrCodeImage).scaled(150, 150));
  m_ui->m_qrCodeLabel->setEnabled(true);
}

QImage AccountFrame::exportImage()
{
    if (!m_ui->m_qrCodeLabel->pixmap())
        return QImage();
    return m_ui->m_qrCodeLabel->pixmap()->toImage();
}

void AccountFrame::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_ui->m_qrCodeLabel->pixmap()) {
        event->accept();
        QMimeData* mimeData = new QMimeData;
        mimeData->setImageData(exportImage());

        QDrag* drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec();
    } else {
        //m_ui->m_qrCodeLabel->mousePressEvent(event);
        Q_EMIT clicked();
    }
}

void AccountFrame::saveImage()
{
    if (!m_ui->m_qrCodeLabel->pixmap())
        return;
    QString fn = QFileDialog::getSaveFileName(&MainWindow::instance(), tr("Save QR Code"), QDir::homePath(), "PNG (*.png)");
    if (!fn.isEmpty()) {
        exportImage().save(fn);
    }
}

void AccountFrame::copyImage()
{
    if (!m_ui->m_qrCodeLabel->pixmap())
        return;
    QApplication::clipboard()->setImage(exportImage());
}

void AccountFrame::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_ui->m_qrCodeLabel->pixmap())
        return;
    contextMenu->exec(event->globalPos());
}

void AccountFrame::reset() {
  updateActualBalance(0);
  updatePendingBalance(0);
  updateUnmixableBalance(0);
  m_ui->m_addressLabel->clear();
}

}
