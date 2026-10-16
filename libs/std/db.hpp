#pragma once
#include "../vissrt.hpp"
#include "json.hpp"
#include "fs.hpp"
#include <sstream>
#include <regex>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace viss {
namespace db {

    struct DbColumn {
        Str name;
        Str type; // INT, DEC, TEXT, BOOL
        Bool is_primary_key = false;
        Bool is_auto_increment = false;
    };

    struct DbRow {
        std::map<Str, Str> fields;
    };

    class DbTable {
    public:
        Str name;
        std::vector<DbColumn> columns;
        std::vector<DbRow> rows;
        Int auto_inc_counter = 1;

        Int find_col_index(const Str& col_name) const {
            for (size_t i = 0; i < columns.size(); ++i) {
                if (str::lower(columns[i].name) == str::lower(col_name)) return (Int)i;
            }
            return -1;
        }
    };

    class Database {
    public:
        Str filepath;
        bool in_memory = false;
        std::map<Str, DbTable> tables;
        bool in_transaction = false;
        std::map<Str, DbTable> tx_snapshot;
        Str last_error;

        Database(const Str& path) : filepath(path) {
            if (path.empty() || path == ":memory:") {
                in_memory = true;
            } else {
                load_from_disk();
            }
        }

        ~Database() {
            if (!in_memory && !filepath.empty()) {
                save_to_disk();
            }
        }

        void save_to_disk() {
            if (in_memory || filepath.empty()) return;
            std::ofstream out(filepath, std::ios::binary);
            if (!out.is_open()) return;

            // Simple robust persistent format: JSON dump of tables & rows
            out << "{\n  \"tables\": {\n";
            size_t t_idx = 0;
            for (const auto& kv : tables) {
                const auto& tbl = kv.second;
                out << "    \"" << tbl.name << "\": {\n";
                out << "      \"auto_inc\": " << tbl.auto_inc_counter << ",\n";
                out << "      \"columns\": [";
                for (size_t c = 0; c < tbl.columns.size(); ++c) {
                    out << "{\"name\":\"" << tbl.columns[c].name << "\",\"type\":\"" << tbl.columns[c].type << "\",\"pk\":" << (tbl.columns[c].is_primary_key ? "true" : "false") << "}";
                    if (c + 1 < tbl.columns.size()) out << ", ";
                }
                out << "],\n      \"rows\": [\n";
                for (size_t r = 0; r < tbl.rows.size(); ++r) {
                    out << "        {";
                    size_t f_idx = 0;
                    for (const auto& f : tbl.rows[r].fields) {
                        out << "\"" << f.first << "\":\"" << json::escape(f.second) << "\"";
                        if (++f_idx < tbl.rows[r].fields.size()) out << ", ";
                    }
                    out << "}";
                    if (r + 1 < tbl.rows.size()) out << ",\n";
                }
                out << "\n      ]\n    }";
                if (++t_idx < tables.size()) out << ",";
                out << "\n";
            }
            out << "  }\n}\n";
        }

        void load_from_disk() {
            if (in_memory || filepath.empty() || !fs::exists(filepath)) return;
            std::ifstream in(filepath, std::ios::binary);
            if (!in.is_open()) return;
            std::stringstream ss;
            ss << in.rdbuf();
            Str content = ss.str();
            if (content.empty()) return;

            // Parse saved tables using built-in json
            try {
                auto data = json::parse(content);
                if (data.is_object() && data.contains("tables")) {
                    auto tbls = data["tables"];
                    auto t_keys = tbls.keys();
                    for (Int ti = 0; ti < t_keys.size(); ++ti) {
                        Str t_name = t_keys[ti];
                        DbTable tbl;
                        tbl.name = t_name;
                        auto t_obj = tbls[t_name];
                        if (t_obj.contains("auto_inc")) tbl.auto_inc_counter = (Int)t_obj["auto_inc"].to_int();
                        if (t_obj.contains("columns")) {
                            auto cols = t_obj["columns"];
                            for (Int ci = 0; ci < cols.size(); ++ci) {
                                auto col_item = cols[ci];
                                DbColumn col;
                                col.name = col_item["name"].to_str();
                                col.type = col_item["type"].to_str();
                                col.is_primary_key = col_item["pk"].to_bool();
                                tbl.columns.push_back(col);
                            }
                        }
                        if (t_obj.contains("rows")) {
                            auto r_list = t_obj["rows"];
                            for (Int ri = 0; ri < r_list.size(); ++ri) {
                                auto r_item = r_list[ri];
                                DbRow row;
                                auto r_keys = r_item.keys();
                                for (Int rki = 0; rki < r_keys.size(); ++rki) {
                                    row.fields[r_keys[rki]] = r_item[r_keys[rki]].to_str();
                                }
                                tbl.rows.push_back(row);
                            }
                        }
                        tables[tbl.name] = tbl;
                    }
                }
            } catch (...) {
                // Keep clean state if file was corrupted
            }
        }
    };

    using DbHandle = std::shared_ptr<Database>;

    inline DbHandle open(const Str& path = ":memory:") {
        return std::make_shared<Database>(path);
    }

    inline void close(DbHandle db) {
        if (!db) return;
        db->save_to_disk();
    }

    inline Bool begin(DbHandle db) {
        if (!db) return false;
        db->in_transaction = true;
        db->tx_snapshot = db->tables;
        return true;
    }

    inline Bool commit(DbHandle db) {
        if (!db || !db->in_transaction) return false;
        db->in_transaction = false;
        db->tx_snapshot.clear();
        db->save_to_disk();
        return true;
    }

    inline Bool rollback(DbHandle db) {
        if (!db || !db->in_transaction) return false;
        db->tables = db->tx_snapshot;
        db->in_transaction = false;
        db->tx_snapshot.clear();
        return true;
    }

    inline List<Str> tables(DbHandle db) {
        List<Str> result;
        if (!db) return result;
        for (const auto& kv : db->tables) {
            result.push(kv.first);
        }
        return result;
    }

    inline Int count(DbHandle db, const Str& table_name) {
        if (!db) return 0;
        Str t_name = str::lower(table_name);
        auto it = db->tables.find(t_name);
        if (it != db->tables.end()) {
            return (Int)it->second.rows.size();
        }
        return 0;
    }

    // Helper: split string respecting single/double quoted strings
    inline std::vector<Str> split_quoted(const Str& s, char delim = ',') {
        std::vector<Str> res;
        Str cur;
        bool in_single = false;
        bool in_double = false;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '\'' && !in_double) {
                if (in_single && i + 1 < s.size() && s[i+1] == '\'') {
                    cur += "''";
                    i++;
                    continue;
                }
                in_single = !in_single;
                cur += c;
            } else if (c == '"' && !in_single) {
                in_double = !in_double;
                cur += c;
            } else if (c == delim && !in_single && !in_double) {
                res.push_back(str::trim(cur));
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) res.push_back(str::trim(cur));
        return res;
    }

    // Helper: evaluate simple condition expression against row fields (e.g. "id = 1", "age > 18 AND status = 'active'")
    inline bool eval_row_condition(const DbRow& row, const Str& where_clause) {
        Str cond = str::trim(where_clause);
        if (cond.empty()) return true;

        // Support simple AND combinations
        auto and_parts = str::split(cond, " AND ");
        if (and_parts.size() == 1) and_parts = str::split(cond, " and ");

        for (const auto& part : and_parts) {
            Str p = str::trim(part);
            if (p.empty()) continue;

            // Check comparison operators
            Str op = "";
            size_t op_pos = Str::npos;
            size_t match_len = 0;
            if ((op_pos = p.find("!=")) != Str::npos) { op = "!="; match_len = 2; }
            else if ((op_pos = p.find("<=")) != Str::npos) { op = "<="; match_len = 2; }
            else if ((op_pos = p.find(">=")) != Str::npos) { op = ">="; match_len = 2; }
            else if ((op_pos = p.find("=")) != Str::npos) { op = "="; match_len = 1; }
            else if ((op_pos = p.find("<")) != Str::npos) { op = "<"; match_len = 1; }
            else if ((op_pos = p.find(">")) != Str::npos) { op = ">"; match_len = 1; }
            else if ((op_pos = str::lower(p).find(" like ")) != Str::npos) { op = "LIKE"; match_len = 6; }

            if (op.empty() || op_pos == Str::npos) continue;

            Str left_col = str::trim(p.substr(0, op_pos));
            Str right_val = str::trim(p.substr(op_pos + match_len));

            // Strip quotes from right_val if present
            if ((right_val.front() == '\'' && right_val.back() == '\'') ||
                (right_val.front() == '"' && right_val.back() == '"')) {
                right_val = right_val.substr(1, right_val.length() - 2);
            }

            Str row_val = "";
            for (const auto& f : row.fields) {
                if (str::lower(f.first) == str::lower(left_col)) {
                    row_val = f.second;
                    break;
                }
            }

            if (op == "=") {
                if (row_val != right_val) return false;
            } else if (op == "!=") {
                if (row_val == right_val) return false;
            } else if (op == "<") {
                try {
                    if (std::stod(row_val) >= std::stod(right_val)) return false;
                } catch (...) {
                    if (row_val >= right_val) return false;
                }
            } else if (op == ">") {
                try {
                    if (std::stod(row_val) <= std::stod(right_val)) return false;
                } catch (...) {
                    if (row_val <= right_val) return false;
                }
            } else if (op == "<=") {
                try {
                    if (std::stod(row_val) > std::stod(right_val)) return false;
                } catch (...) {
                    if (row_val > right_val) return false;
                }
            } else if (op == ">=") {
                try {
                    if (std::stod(row_val) < std::stod(right_val)) return false;
                } catch (...) {
                    if (row_val < right_val) return false;
                }
            } else if (op == "LIKE") {
                Str regex_pat = "^";
                for (char ch : right_val) {
                    if (ch == '%') regex_pat += ".*";
                    else if (ch == '_') regex_pat += ".";
                    else if (ch == '.' || ch == '^' || ch == '$' || ch == '*' || ch == '+' || ch == '?' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == '|' || ch == '\\') {
                        regex_pat += '\\';
                        regex_pat += ch;
                    } else {
                        regex_pat += ch;
                    }
                }
                regex_pat += "$";
                try {
                    if (!std::regex_match(row_val, std::regex(regex_pat, std::regex_constants::icase))) return false;
                } catch (...) {
                    if (str::lower(row_val).find(str::lower(right_val)) == Str::npos) return false;
                }
            }
        }
        return true;
    }

    // Execute DDL and DML statements (CREATE, INSERT, UPDATE, DELETE, DROP)
    inline Int exec(DbHandle db, const Str& sql_query) {
        if (!db) return -1;
        Str q = str::trim(sql_query);
        if (q.empty()) return 0;
        if (q.back() == ';') q.pop_back();
        q = str::trim(q);

        Str upper_q = str::upper(q);

        // --- CREATE TABLE ---
        if (str::starts_with(upper_q, "CREATE TABLE")) {
            size_t open_paren = q.find('(');
            size_t close_paren = q.rfind(')');
            if (open_paren == Str::npos || close_paren == Str::npos) return -1;

            Str header = q.substr(12, open_paren - 12);
            header = str::replace(str::upper(header), "IF NOT EXISTS", "");
            Str table_name = str::lower(str::trim(header));

            if (db->tables.find(table_name) != db->tables.end()) {
                return 0; // Table already exists
            }

            DbTable tbl;
            tbl.name = table_name;

            Str body = q.substr(open_paren + 1, close_paren - open_paren - 1);
            auto col_defs = str::split(body, ",");
            for (const auto& c_def : col_defs) {
                Str c_str = str::trim(c_def);
                if (c_str.empty()) continue;
                auto parts = str::split(c_str, " ");
                if (parts.size() >= 2) {
                    DbColumn col;
                    col.name = parts[0];
                    col.type = str::upper(parts[1]);
                    Str full_def = str::upper(c_str);
                    if (full_def.find("PRIMARY KEY") != Str::npos) col.is_primary_key = true;
                    if (full_def.find("AUTOINCREMENT") != Str::npos) col.is_auto_increment = true;
                    tbl.columns.push_back(col);
                }
            }
            db->tables[table_name] = tbl;
            db->save_to_disk();
            return 1;
        }

        // --- INSERT INTO ---
        if (str::starts_with(upper_q, "INSERT INTO")) {
            size_t into_pos = upper_q.find("INTO") + 4;
            size_t val_pos = upper_q.find("VALUES");
            if (val_pos == Str::npos) return -1;

            Str tbl_and_cols = str::trim(q.substr(into_pos, val_pos - into_pos));
            Str table_name = tbl_and_cols;
            std::vector<Str> explicit_cols;

            size_t col_open = tbl_and_cols.find('(');
            if (col_open != Str::npos) {
                table_name = str::trim(tbl_and_cols.substr(0, col_open));
                size_t col_close = tbl_and_cols.find(')');
                if (col_close != Str::npos) {
                    Str cols_str = tbl_and_cols.substr(col_open + 1, col_close - col_open - 1);
                    auto sp = str::split(cols_str, ",");
                    for (auto& s : sp) explicit_cols.push_back(str::trim(s));
                }
            }
            table_name = str::lower(table_name);

            auto it = db->tables.find(table_name);
            if (it == db->tables.end()) {
                db->last_error = "Table " + table_name + " does not exist";
                return -1;
            }

            DbTable& tbl = it->second;
            Str vals_raw = q.substr(val_pos + 6);
            vals_raw = str::trim(vals_raw);
            if (vals_raw.front() == '(' && vals_raw.back() == ')') {
                vals_raw = vals_raw.substr(1, vals_raw.length() - 2);
            }

            // Split values safely respecting quotes
            std::vector<Str> parsed_vals = split_quoted(vals_raw, ',');

            DbRow row;
            if (!explicit_cols.empty()) {
                for (size_t i = 0; i < explicit_cols.size() && i < parsed_vals.size(); ++i) {
                    Str v = parsed_vals[i];
                    if ((v.front() == '\'' && v.back() == '\'') || (v.front() == '"' && v.back() == '"')) {
                        v = v.substr(1, v.length() - 2);
                        v = str::replace(v, "''", "'");
                    }
                    row.fields[explicit_cols[i]] = v;
                }
            } else {
                for (size_t i = 0; i < tbl.columns.size() && i < parsed_vals.size(); ++i) {
                    Str v = parsed_vals[i];
                    if ((v.front() == '\'' && v.back() == '\'') || (v.front() == '"' && v.back() == '"')) {
                        v = v.substr(1, v.length() - 2);
                        v = str::replace(v, "''", "'");
                    }
                    row.fields[tbl.columns[i].name] = v;
                }
            }

            // Check auto-increment / primary keys
            for (const auto& col : tbl.columns) {
                if (col.is_auto_increment || col.is_primary_key) {
                    if (row.fields.find(col.name) == row.fields.end() || row.fields[col.name].empty()) {
                        row.fields[col.name] = std::to_string(tbl.auto_inc_counter++);
                    }
                }
            }

            tbl.rows.push_back(row);
            db->save_to_disk();
            return 1;
        }

        // --- UPDATE ---
        if (str::starts_with(upper_q, "UPDATE")) {
            size_t set_pos = upper_q.find("SET");
            if (set_pos == Str::npos) return -1;
            Str table_name = str::lower(str::trim(q.substr(7, set_pos - 7)));
            auto it = db->tables.find(table_name);
            if (it == db->tables.end()) return -1;

            DbTable& tbl = it->second;
            size_t where_pos = upper_q.find("WHERE", set_pos);
            Str set_clause = "";
            Str where_clause = "";

            if (where_pos != Str::npos) {
                set_clause = str::trim(q.substr(set_pos + 3, where_pos - (set_pos + 3)));
                where_clause = str::trim(q.substr(where_pos + 5));
            } else {
                set_clause = str::trim(q.substr(set_pos + 3));
            }

            // Parse sets (e.g. "age = 20, name = 'Jake'")
            std::map<Str, Str> updates;
            auto sets = split_quoted(set_clause, ',');
            for (const auto& s : sets) {
                size_t eq = s.find('=');
                if (eq != Str::npos) {
                    Str col = str::trim(s.substr(0, eq));
                    Str val = str::trim(s.substr(eq + 1));
                    if ((val.front() == '\'' && val.back() == '\'') || (val.front() == '"' && val.back() == '"')) {
                        val = val.substr(1, val.length() - 2);
                        val = str::replace(val, "''", "'");
                    }
                    updates[col] = val;
                }
            }

            Int count_updated = 0;
            for (auto& row : tbl.rows) {
                if (eval_row_condition(row, where_clause)) {
                    for (const auto& u : updates) {
                        row.fields[u.first] = u.second;
                    }
                    count_updated++;
                }
            }
            db->save_to_disk();
            return count_updated;
        }

        // --- DELETE FROM ---
        if (str::starts_with(upper_q, "DELETE FROM")) {
            Str rest = str::trim(q.substr(11));
            Str table_name = rest;
            Str where_clause = "";
            size_t where_pos = str::upper(rest).find("WHERE");
            if (where_pos != Str::npos) {
                table_name = str::lower(str::trim(rest.substr(0, where_pos)));
                where_clause = str::trim(rest.substr(where_pos + 5));
            } else {
                table_name = str::lower(str::trim(rest));
            }

            auto it = db->tables.find(table_name);
            if (it == db->tables.end()) return -1;

            DbTable& tbl = it->second;
            std::vector<DbRow> retained;
            Int count_deleted = 0;
            for (const auto& row : tbl.rows) {
                if (eval_row_condition(row, where_clause)) {
                    count_deleted++;
                } else {
                    retained.push_back(row);
                }
            }
            tbl.rows = retained;
            db->save_to_disk();
            return count_deleted;
        }

        // --- DROP TABLE ---
        if (str::starts_with(upper_q, "DROP TABLE")) {
            Str table_name = str::trim(q.substr(10));
            table_name = str::replace(str::upper(table_name), "IF EXISTS", "");
            table_name = str::lower(str::trim(table_name));
            db->tables.erase(table_name);
            db->save_to_disk();
            return 1;
        }

        return 0;
    }

    // Execute SELECT queries and return rows as List<Map<Str, Str>>
    inline List<Map<Str, Str>> query(DbHandle db, const Str& sql_query) {
        List<Map<Str, Str>> results;
        if (!db) return results;

        Str q = str::trim(sql_query);
        if (q.empty()) return results;
        if (q.back() == ';') q.pop_back();

        Str upper_q = str::upper(q);
        if (!str::starts_with(upper_q, "SELECT")) return results;

        size_t from_pos = upper_q.find("FROM");
        if (from_pos == Str::npos) return results;

        Str cols_part = str::trim(q.substr(6, from_pos - 6));
        Str rest = str::trim(q.substr(from_pos + 4));

        Str table_name = rest;
        Str where_clause = "";
        Str order_by = "";
        Int limit_n = -1;

        size_t where_pos = str::upper(rest).find("WHERE");
        size_t order_pos = str::upper(rest).find("ORDER BY");
        size_t limit_pos = str::upper(rest).find("LIMIT");

        if (limit_pos != Str::npos) {
            try {
                limit_n = std::stoll(str::trim(rest.substr(limit_pos + 5)));
            } catch (...) {}
            rest = str::trim(rest.substr(0, limit_pos));
        }

        if (order_pos != Str::npos) {
            order_by = str::trim(rest.substr(order_pos + 8));
            rest = str::trim(rest.substr(0, order_pos));
        }

        if (where_pos != Str::npos) {
            table_name = str::lower(str::trim(rest.substr(0, where_pos)));
            where_clause = str::trim(rest.substr(where_pos + 5));
        } else {
            table_name = str::lower(str::trim(rest));
        }

        auto it = db->tables.find(table_name);
        if (it == db->tables.end()) return results;

        const DbTable& tbl = it->second;
        std::vector<DbRow> matched_rows;

        for (const auto& row : tbl.rows) {
            if (eval_row_condition(row, where_clause)) {
                matched_rows.push_back(row);
            }
        }

        // Apply simple ORDER BY if specified
        if (!order_by.empty()) {
            auto ord_parts = str::split(order_by, " ");
            Str ord_col = ord_parts[0];
            bool desc = ord_parts.size() > 1 && str::upper(ord_parts[1]) == "DESC";

            std::sort(matched_rows.begin(), matched_rows.end(), [&](const DbRow& a, const DbRow& b) {
                Str va = a.fields.count(ord_col) ? a.fields.at(ord_col) : "";
                Str vb = b.fields.count(ord_col) ? b.fields.at(ord_col) : "";
                try {
                    double da = std::stod(va);
                    double db_ = std::stod(vb);
                    return desc ? (da > db_) : (da < db_);
                } catch (...) {
                    return desc ? (va > vb) : (va < vb);
                }
            });
        }

        // Parse requested columns
        bool select_all = (cols_part == "*");
        std::vector<Str> req_cols;
        if (!select_all) {
            auto sp = str::split(cols_part, ",");
            for (auto& s : sp) req_cols.push_back(str::trim(s));
        }

        Int count_added = 0;
        for (const auto& r : matched_rows) {
            if (limit_n >= 0 && count_added >= limit_n) break;

            Map<Str, Str> row_map;
            if (select_all) {
                for (const auto& f : r.fields) {
                    row_map.set(f.first, f.second);
                }
            } else {
                for (const auto& col : req_cols) {
                    for (const auto& f : r.fields) {
                        if (str::lower(f.first) == str::lower(col)) {
                            row_map.set(col, f.second);
                            break;
                        }
                    }
                }
            }
            results.push(row_map);
            count_added++;
        }

        return results;
    }

    // Query and return directly as JSON string
    inline Str query_json(DbHandle db, const Str& sql_query) {
        auto rows = query(db, sql_query);
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < (size_t)rows.size(); ++i) {
            out << "{";
            auto m = rows[i];
            auto keys = m.keys();
            for (size_t k = 0; k < (size_t)keys.size(); ++k) {
                out << "\"" << keys[k] << "\":\"" << json::escape(m[keys[k]]) << "\"";
                if (k + 1 < (size_t)keys.size()) out << ", ";
            }
            out << "}";
            if (i + 1 < (size_t)rows.size()) out << ", ";
        }
        out << "]";
        return out.str();
    }

    // Parameterized exec helper (replaces ? with string-escaped parameter values)
    inline Int exec_params(DbHandle db, const Str& sql_query, const List<Str>& params) {
        Str final_sql = sql_query;
        size_t search_pos = 0;
        for (size_t i = 0; i < (size_t)params.size(); ++i) {
            size_t q_pos = final_sql.find('?', search_pos);
            if (q_pos == Str::npos) break;
            Str val = "'" + str::replace(params[i], "'", "''") + "'";
            final_sql.replace(q_pos, 1, val);
            search_pos = q_pos + val.size();
        }
        return exec(db, final_sql);
    }

    // Parameterized query helper
    inline List<Map<Str, Str>> query_params(DbHandle db, const Str& sql_query, const List<Str>& params) {
        Str final_sql = sql_query;
        size_t search_pos = 0;
        for (size_t i = 0; i < (size_t)params.size(); ++i) {
            size_t q_pos = final_sql.find('?', search_pos);
            if (q_pos == Str::npos) break;
            Str val = "'" + str::replace(params[i], "'", "''") + "'";
            final_sql.replace(q_pos, 1, val);
            search_pos = q_pos + val.size();
        }
        return query(db, final_sql);
    }

    // Snapshot backup of active database
    inline Bool backup(DbHandle db, const Str& backup_filepath) {
        if (!db) return false;
        Str old_path = db->filepath;
        db->filepath = backup_filepath;
        db->in_memory = false;
        db->save_to_disk();
        db->filepath = old_path;
        return true;
    }

} // namespace db
} // namespace viss
