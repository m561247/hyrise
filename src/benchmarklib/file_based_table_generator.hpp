#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "abstract_table_generator.hpp"

namespace opossum {

class FileBasedTableGenerator : virtual public AbstractTableGenerator {
 public:
  FileBasedTableGenerator(const std::shared_ptr<BenchmarkConfig>& benchmark_config, const std::string& path, const std::optional<std::unordered_set<std::string>>& table_subset = std::nullopt);

  std::unordered_map<std::string, BenchmarkTableInfo> generate() override;

  /**
   * Set @param add_constraints_callback to define table constraints, if available. It is called by _add_constraints
   * after tables have been generated.
   */
  void set_add_constraints_callback(
      const std::function<void(std::unordered_map<std::string, BenchmarkTableInfo>&)>& add_constraints_callback);

 protected:
  const std::string _path;
  const std::optional<std::unordered_set<std::string>> _table_subset;
  void _add_constraints(std::unordered_map<std::string, BenchmarkTableInfo>& table_info_by_name) const override;
  std::function<void(std::unordered_map<std::string, BenchmarkTableInfo>&)> _add_constraints_callback;
};

}  // namespace opossum
