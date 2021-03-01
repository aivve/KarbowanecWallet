// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2015-2016 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QDateTime>

#include "SortedOutputsModel.h"
#include "OutputsModel.h"

namespace WalletGui {

SortedOutputsModel& SortedOutputsModel::instance() {
  static SortedOutputsModel inst;
  return inst;
}

SortedOutputsModel::SortedOutputsModel() : QSortFilterProxyModel() {
  setSourceModel(&OutputsModel::instance());
  setDynamicSortFilter(true);
  sort(OutputsModel::COLUMN_GLOBAL_OUTPUT_INDEX, Qt::DescendingOrder);
}

SortedOutputsModel::~SortedOutputsModel() {
}

bool SortedOutputsModel::filterAcceptsRow(int _row, const QModelIndex &_parent) const {
  QModelIndex _index = sourceModel()->index(_row, 0, _parent);

  int ourType = _index.data(OutputsModel::ROLE_TYPE).value<quint8>();

  if(selectedType != -1) {
    if(ourType != selectedType)
      return false;
  }

  QModelIndex index2 = sourceModel()->index(_row, 2, _parent);
  QModelIndex index3 = sourceModel()->index(_row, 3, _parent);
  QModelIndex index6 = sourceModel()->index(_row, 6, _parent);

  return (sourceModel()->data(index2).toString().contains(searchString,Qt::CaseInsensitive)
       || sourceModel()->data(index3).toString().contains(searchString,Qt::CaseInsensitive)
       || sourceModel()->data(index6).toString().contains(searchString,Qt::CaseInsensitive));

  return true;
}

void SortedOutputsModel::setSearchFor(const QString &searchString) {
  this->searchString = searchString;
  invalidateFilter();
}

void SortedOutputsModel::setType(const int type) {
  this->selectedType = type;
  invalidateFilter();
}

}
