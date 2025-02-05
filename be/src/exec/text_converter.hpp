// This file is made available under Elastic License 2.0.
// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/be/src/exec/text_converter.hpp

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef STARROCKS_BE_SRC_QUERY_EXEC_TEXT_CONVERTER_HPP
#define STARROCKS_BE_SRC_QUERY_EXEC_TEXT_CONVERTER_HPP

#include "text_converter.h"

#include <boost/algorithm/string.hpp>

#include "runtime/decimal_value.h"
#include "runtime/decimalv2_value.h"
#include "runtime/decimalv3.h"
#include "runtime/descriptors.h"
#include "runtime/mem_pool.h"
#include "runtime/runtime_state.h"
#include "runtime/string_value.h"
#include "runtime/datetime_value.h"
#include "runtime/tuple.h"
#include "util/string_parser.hpp"
#include "util/types.h"
#include "storage/utils.h"

namespace starrocks {

// Our new vectorized query executor is more powerful and stable than old query executor,
// The executor query executor related codes could be deleted safely.
// TODO: Remove old query executor related codes before 2021-09-30

// Note: this function has a codegen'd version.  Changing this function requires
// corresponding changes to CodegenWriteSlot.
inline bool TextConverter::write_slot(const SlotDescriptor* slot_desc,
                                      Tuple* tuple,
                                      const char* data,
                                      int len,
                                      bool copy_string,
                                      bool need_escape,
                                      MemPool* pool) {
    // only \N will be treated as NULL
    if (true == slot_desc->is_nullable()) {
        if (len == 2 && data[0] == '\\' && data[1] == 'N') {
            tuple->set_null(slot_desc->null_indicator_offset());
            return true;
        } else {
            tuple->set_not_null(slot_desc->null_indicator_offset());
        }
    }

    StringParser::ParseResult parse_result = StringParser::PARSE_SUCCESS;
    void* slot = tuple->get_slot(slot_desc->tuple_offset());

    // Parse the raw-text data. Translate the text string to internal format.
    switch (slot_desc->type().type) {
    case TYPE_HLL:
    case TYPE_VARCHAR:
    case TYPE_CHAR: {
        StringValue* str_slot = reinterpret_cast<StringValue*>(slot);
        str_slot->ptr = const_cast<char*>(data);
        str_slot->len = len;
        if (len != 0 && (copy_string || need_escape)) {
            DCHECK(pool != NULL);
            char* slot_data = reinterpret_cast<char*>(pool->allocate(len));

            if (need_escape) {
                unescape_string(data, slot_data, &str_slot->len);
            } else {
                memcpy(slot_data, data, str_slot->len);
            }

            str_slot->ptr = slot_data;
        }

        break;
    }

    case TYPE_BOOLEAN:
        *reinterpret_cast<bool*>(slot) =
            StringParser::string_to_bool(data, len, &parse_result);
        break;

    case TYPE_TINYINT:
        *reinterpret_cast<int8_t*>(slot) =
            StringParser::string_to_int<int8_t>(data, len, &parse_result);
        break;

    case TYPE_SMALLINT:
        *reinterpret_cast<int16_t*>(slot) =
            StringParser::string_to_int<int16_t>(data, len, &parse_result);
        break;

    case TYPE_INT:
        *reinterpret_cast<int32_t*>(slot) =
            StringParser::string_to_int<int32_t>(data, len, &parse_result);
        break;

    case TYPE_BIGINT:
        *reinterpret_cast<int64_t*>(slot) =
            StringParser::string_to_int<int64_t>(data, len, &parse_result);
        break;

    case TYPE_LARGEINT: {
        __int128 tmp = StringParser::string_to_int<__int128>(data, len, &parse_result);
        memcpy(slot, &tmp, sizeof(tmp));
        break;
    }

    case TYPE_FLOAT:
        *reinterpret_cast<float*>(slot) =
            StringParser::string_to_float<float>(data, len, &parse_result);
        break;

    case TYPE_DOUBLE:
        *reinterpret_cast<double*>(slot) =
            StringParser::string_to_float<double>(data, len, &parse_result);
        break;

    case TYPE_DATE: {
        DateTimeValue* ts_slot = reinterpret_cast<DateTimeValue*>(slot);
        if (!ts_slot->from_date_str(data, len)) {
            parse_result = StringParser::PARSE_FAILURE;
            break;
        }

        ts_slot->cast_to_date();
        break;
    }

    case TYPE_DATETIME: {
        DateTimeValue* ts_slot = reinterpret_cast<DateTimeValue*>(slot);
        if (!ts_slot->from_date_str(data, len)) {
            parse_result = StringParser::PARSE_FAILURE;
        }

        ts_slot->to_datetime();
        break;
    }

    case TYPE_DECIMAL: {
        DecimalValue* decimal_slot = reinterpret_cast<DecimalValue*>(slot);

        if (decimal_slot->parse_from_str(data, len)) {
            parse_result = StringParser::PARSE_FAILURE;
        }

        break;
    }

    case TYPE_DECIMALV2: {
        DecimalV2Value decimal_slot;

        if (decimal_slot.parse_from_str(data, len)) {
            parse_result = StringParser::PARSE_FAILURE;
        }

        *reinterpret_cast<PackedInt128*>(slot) =
            *reinterpret_cast<const PackedInt128*>(&decimal_slot);

        break;
    }
    case TYPE_DECIMAL32: {
        int32_t v;
        if (DecimalV3Cast::from_string<int32_t>(&v, slot_desc->type().precision,
                                                slot_desc->type().scale, data, len)) {
            parse_result = StringParser::PARSE_FAILURE;
            break;
        }
        memcpy(slot, &v, sizeof(v));
        break;
    }
    case TYPE_DECIMAL64: {
        int64_t v;
        if (DecimalV3Cast::from_string<int64_t>(&v, slot_desc->type().precision,
                                                slot_desc->type().scale, data, len)) {
            parse_result = StringParser::PARSE_FAILURE;
            break;
        }
        memcpy(slot, &v, sizeof(v));
        break;
    }
    case TYPE_DECIMAL128: {
        int128_t v;
        if (DecimalV3Cast::from_string<int128_t>(&v, slot_desc->type().precision,
                                                 slot_desc->type().scale, data, len)) {
            parse_result = StringParser::PARSE_FAILURE;
            break;
        }
        memcpy(slot, &v, sizeof(v));
        break;
    }
    default:
        DCHECK(false) << "bad slot type: " << slot_desc->type();
        break;
    }

    // TODO: add warning for overflow case
    if (parse_result != StringParser::PARSE_SUCCESS) {
        tuple->set_null(slot_desc->null_indicator_offset());
        return false;
    }

    return true;
}

}

#endif
