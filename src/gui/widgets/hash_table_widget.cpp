/*  Copyright (C) 2014-2016 FastoGT. All right reserved.

    This file is part of FastoNoSQL.

    FastoNoSQL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FastoNoSQL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FastoNoSQL.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gui/widgets/hash_table_widget.h"

#include <common/macros.h>
#include <common/qt/utils_qt.h>

#include "gui/hash_table_model.h"
#include "gui/key_value_table_item.h"
#include "gui/action_cell_delegate.h"

namespace fastonosql {
namespace gui {

HashTableWidget::HashTableWidget(QWidget* parent) : QTableView(parent), model_(nullptr) {
  model_ = new HashTableModel(this);
  setModel(model_);
  ActionDelegate* del = new ActionDelegate(this);
  VERIFY(connect(del, &ActionDelegate::addClicked, this, &HashTableWidget::addRow));
  VERIFY(connect(del, &ActionDelegate::removeClicked, this, &HashTableWidget::removeRow));

  setItemDelegateForColumn(KeyValueTableItem::kAction, del);
  setContextMenuPolicy(Qt::ActionsContextMenu);
  setSelectionBehavior(QAbstractItemView::SelectRows);
}

HashTableWidget::~HashTableWidget() {}

void HashTableWidget::insertRow(const QString& first, const QString& second) {
  model_->insertRow(first, second);
}

void HashTableWidget::clear() {
  model_->clear();
}

common::ZSetValue* HashTableWidget::zsetValue() const {
  return model_->zsetValue();
}

common::HashValue* HashTableWidget::hashValue() const {
  return model_->hashValue();
}

void HashTableWidget::addRow(const QModelIndex& index) {
  KeyValueTableItem* node =
      common::qt::item<common::qt::gui::TableItem*, KeyValueTableItem*>(index);
  model_->insertRow(node->key(), node->value());
}

void HashTableWidget::removeRow(const QModelIndex& index) {
  model_->removeRow(index.row());
}

}  // namespace gui
}  // namespace fastonosql
