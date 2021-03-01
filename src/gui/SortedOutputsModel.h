// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QSortFilterProxyModel>

namespace WalletGui {

class SortedOutputsModel : public QSortFilterProxyModel {
  Q_OBJECT
  Q_DISABLE_COPY(SortedOutputsModel)

public:
  static SortedOutputsModel& instance();

  void setType(const int type);
  void setSearchFor(const QString &searchstring);

protected:
  bool filterAcceptsRow(int _row, const QModelIndex &_parent) const Q_DECL_OVERRIDE;

private:
  SortedOutputsModel();
  ~SortedOutputsModel();

  QString searchString;
  int selectedType = -1;

};

}
