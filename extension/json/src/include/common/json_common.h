#pragma once

#include "json_enums.h"
#include "yyjson.h"

namespace kuzu {
namespace json_extension {

struct JSONCommon {
public:
    static constexpr auto READ_FLAG =
        YYJSON_READ_ALLOW_INF_AND_NAN | YYJSON_READ_ALLOW_TRAILING_COMMAS;
    static constexpr auto READ_STOP_FLAG = READ_FLAG | YYJSON_READ_STOP_WHEN_DONE;
    static constexpr auto READ_INSITU_FLAG = READ_STOP_FLAG | YYJSON_READ_INSITU;
    static constexpr auto WRITE_FLAG = YYJSON_WRITE_ALLOW_INF_AND_NAN;
    static constexpr auto WRITE_PRETTY_FLAG = YYJSON_WRITE_ALLOW_INF_AND_NAN | YYJSON_WRITE_PRETTY;

    static yyjson_doc* readDocumentUnsafe(uint8_t* data, uint64_t size, const yyjson_read_flag flg,
        yyjson_read_err* err = nullptr) {
        return yyjson_read_opts((char*)data, size, flg, nullptr /* alc */, err);
    }
};

struct JsonConstant {
    // 16MB
    static constexpr uint64_t MAXIMUM_OBJECT_SIZE = 16777216;
    static constexpr uint64_t SCAN_BUFFER_CAPACITY = MAXIMUM_OBJECT_SIZE * 2;
    static constexpr JsonScanFormat DEFAULT_JSON_FORMAT = JsonScanFormat::AUTO_DETECT;
    static constexpr uint64_t DEFAULT_JSON_DETECT_DEPTH = 10;
    static constexpr uint64_t DEFAULT_JSON_DETECT_BREADTH = 2048;
    static constexpr bool DEFAULT_AUTO_DETECT_VALUE = true;
};

} // namespace json_extension
} // namespace kuzu
