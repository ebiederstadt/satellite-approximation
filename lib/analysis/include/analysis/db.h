#pragma once

#include <sqlite3.h>
#include <filesystem>
#include <givde/types.hpp>
#include <variant>

namespace fs = std::filesystem;
using namespace givde;

namespace analysis {
    struct CloudShadowStatus {
        bool clouds_exist;
        bool shadows_exist;
    };

    enum class Indices;
    struct UseApproximatedData;
    struct UseRealData;
    using DataChoices = std::variant<UseApproximatedData, UseRealData>;

    class DataBase {
    public:
        DataBase(fs::path const &base_path);

        ~DataBase();

        CloudShadowStatus get_status(std::string_view date);

        std::optional<int> result_exists(
                Indices index,
                f64 threshold,
                int start_year,
                int end_year,
                DataChoices choice
        );
        int save_result_in_table(
                Indices index,
                f64 threshold,
                int start_year,
                int end_year,
                DataChoices choice);

    private:
        sqlite3* db;
        std::string sql;
        sqlite3_stmt* stmt;

        void create_sis_table();
        void prepare_stmt(
                sqlite3_stmt* stmt_to_prepare,
                std::string_view sql_string,
                Indices index,
                f64 threshold,
                int start_year,
                int end_year,
                DataChoices choice);
    };
}
