#pragma once
#include "binder/binder.h"
#include "binder/bound_statement.h"
#include "binder/query/bound_regular_query.h"
#include "common/copier_config/file_scan_info.h"

namespace kuzu {
namespace binder {

struct ExportedTableData {
    std::string tableName;
    std::unique_ptr<BoundRegularQuery> regularQuery;
    std::vector<std::string> columnNames;
    std::vector<common::LogicalType> columnTypes;

    const std::vector<common::LogicalType>& getColumnTypesRef() const { return columnTypes; }
    const BoundRegularQuery* getRegularQuery() const { return regularQuery.get(); }
};

class BoundExportDatabase final : public BoundStatement {
public:
    BoundExportDatabase(std::string filePath, common::FileTypeInfo fileTypeInfo,
        std::vector<ExportedTableData> exportData,
        common::case_insensitive_map_t<common::Value> csvOption)
        : BoundStatement{common::StatementType::EXPORT_DATABASE,
              BoundStatementResult::createSingleStringColumnResult()},
          exportData(std::move(exportData)),
          boundFileInfo(std::move(fileTypeInfo), std::vector{std::move(filePath)}) {
        boundFileInfo.options = std::move(csvOption);
    }

    std::string getFilePath() const { return boundFileInfo.filePaths[0]; }
    common::FileType getFileType() const { return boundFileInfo.fileTypeInfo.fileType; }
    common::case_insensitive_map_t<common::Value> getExportOptions() const {
        return boundFileInfo.options;
    }
    const common::FileScanInfo* getBoundFileInfo() const { return &boundFileInfo; }
    const std::vector<ExportedTableData>* getExportData() const { return &exportData; }

private:
    std::vector<ExportedTableData> exportData;
    common::FileScanInfo boundFileInfo;
};

} // namespace binder
} // namespace kuzu
