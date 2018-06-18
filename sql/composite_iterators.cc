/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/composite_iterators.h"

#include <atomic>
#include <string>
#include <vector>

#include "sql/item.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"

class Item_sum;

using std::string;
using std::vector;

namespace {

void SwitchSlice(JOIN *join, int slice_num) {
  if (!join->ref_items[slice_num].is_null()) {
    join->set_ref_item_slice(slice_num);
  }
}

}  // namespace

int FilterIterator::Read() {
  for (;;) {
    int err = m_source->Read();
    if (err != 0) return err;

    bool matched = m_condition->val_int();

    if (thd()->killed) {
      thd()->send_kill_message();
      return 1;
    }

    /* check for errors evaluating the condition */
    if (thd()->is_error()) return 1;

    if (!matched) {
      m_source->UnlockRow();
      continue;
    }

    // Successful row.
    return 0;
  }
}

bool LimitOffsetIterator::Init() {
  if (m_source->Init()) {
    return true;
  }
  for (ha_rows row_idx = 0; row_idx < m_offset; ++row_idx) {
    int err = m_source->Read();
    if (err == 1) {
      return true;  // Note that this will propagate Read() errors to Init().
    } else if (err == -1) {
      m_seen_rows = m_offset;  // So that Read() will return -1.
      return false;            // EOF is not an error.
    }
    if (m_skipped_rows != nullptr) {
      ++*m_skipped_rows;
    }
    m_source->UnlockRow();
  }
  m_seen_rows = m_offset;
  return false;
}

int LimitOffsetIterator::Read() {
  if (m_seen_rows++ >= m_limit) {
    return -1;
  } else {
    return m_source->Read();
  }
}

bool AggregateIterator::Init() {
  DBUG_ASSERT(!m_join->tmp_table_param.precomputed_group_by);
  if (m_source->Init()) {
    return true;
  }

  // Store which slice we will be reading from.
  m_input_slice = m_join->get_ref_item_slice();

  m_first_row = true;
  m_eof = false;
  m_save_nullinfo = 0;
  return false;
}

int AggregateIterator::Read() {
  if (m_eof) {
    // We've seen the last row earlier.
    if (m_save_nullinfo != 0) {
      m_join->restore_fields(m_save_nullinfo);
      m_save_nullinfo = 0;
    }
    return -1;
  }

  // Switch to the input slice before we call Read(), so that any processing
  // that happens in sub-iterators is on the right slice.
  SwitchSlice(m_join, m_input_slice);

  if (m_first_row) {
    // Start the first group, if possible. (If we're not at the first row,
    // we already saw the first row in the new group at the previous Read().)
    m_first_row = false;
    int err = m_source->Read();
    if (err == -1) {
      m_eof = true;
      if (m_join->grouped || m_join->group_optimized_away) {
        return -1;
      } else {
        // If there's no GROUP BY, we need to output a row even if there are no
        // input rows.

        // Calculate aggregate functions for no rows
        for (Item &item : *m_join->get_current_fields()) {
          item.no_rows_in_result();
        }

        /*
          Mark tables as containing only NULL values for ha_write_row().
          Calculate a set of tables for which NULL values need to
          be restored after sending data.
        */
        if (m_join->clear_fields(&m_save_nullinfo)) {
          return 1;
        }
        return 0;
      }
    }
    if (err != 0) return err;
  }

  // This is the start of a new group. Make a copy of the group expressions,
  // because they risk being overwritten on the next call to m_source->Read().
  // We cannot reuse the Item_cached_* fields in m_join->group_fields for this
  // (even though also need to be initialized as part of the start of the
  // group), because they are overwritten by the testing at each row, just like
  // the data from Read() will be.
  {
    Switch_ref_item_slice slice_switch(m_join, REF_SLICE_ORDERED_GROUP_BY);
    if (copy_fields(&m_join->tmp_table_param, m_join->thd)) {
      return 1;
    }
    (void)update_item_cache_if_changed(m_join->group_fields);
    // TODO: Implement rollup.
    if (init_sum_functions(m_join->sum_funcs, m_join->sum_funcs_end[0])) {
      return 1;
    }
  }

  // Keep reading rows as long as they are part of the existing group.
  for (;;) {
    int err = m_source->Read();
    if (err == 1) return 1;  // Error.

    if (err == -1) {
      // End of input rows; return the last group.
      SwitchSlice(m_join, REF_SLICE_ORDERED_GROUP_BY);
      m_eof = true;
      return 0;
    }

    int idx = update_item_cache_if_changed(m_join->group_fields);
    if (idx >= 0) {
      // The group changed. Return the current row; the next Read() will deal
      // with the new group.
      SwitchSlice(m_join, REF_SLICE_ORDERED_GROUP_BY);
      return 0;
    } else {
      // We're still in the same group.
      if (update_sum_func(m_join->sum_funcs)) {
        return 1;
      }
    }
  }
}

void AggregateIterator::UnlockRow() {
  // Most likely, HAVING failed. Ideally, we'd like to backtrack and unlock
  // all rows that went into this aggregate, but we can't do that, and we also
  // can't unlock the _current_ row, since that belongs to a different group.
  // Thus, do nothing.
}

bool PrecomputedAggregateIterator::Init() {
  DBUG_ASSERT(m_join->tmp_table_param.precomputed_group_by);
  DBUG_ASSERT(m_join->grouped || m_join->group_optimized_away);
  return m_source->Init();
}

int PrecomputedAggregateIterator::Read() {
  int err = m_source->Read();
  if (err != 0) {
    return err;
  }

  // Even if the aggregates have been precomputed (typically by
  // QUICK_RANGE_MIN_MAX), we need to copy over the non-aggregated
  // fields here.
  if (copy_fields(&m_join->tmp_table_param, m_join->thd)) {
    return 1;
  }
  SwitchSlice(m_join, REF_SLICE_ORDERED_GROUP_BY);
  return 0;
}

void PrecomputedAggregateIterator::UnlockRow() {
  // See AggregateIterator::UnlockRow().
}
