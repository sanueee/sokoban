#include "db.h"
#include "sqlite3.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static sqlite3 *db = NULL;

bool open(const char *path)
{
    if (sqlite3_open(path, &db) != SQLITE_OK)
    {
        fprintf(stderr, "DB error: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "  id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT UNIQUE NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id      INTEGER NOT NULL,"
        "  diff   INTEGER NOT NULL,"
        "  steps        INTEGER NOT NULL,"
        "  time         INTEGER NOT NULL,"
        "  completed    INTEGER NOT NULL,"
        "  played_at    TEXT NOT NULL,"
        "  FOREIGN KEY(user_id) REFERENCES users(id)"
        ");";

    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK)
    {
        fprintf(stderr, "DB init error: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    return 1;
}

void close(void)
{
    if (db)
    {
        sqlite3_close(db);
        db = NULL;
    }
}

int create_user(const char *name)
{
    int existing = find_user(name);
    if (existing != -1)
        return existing;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO users(name) VALUES(?);", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db);
}

int find_user(const char *name)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id FROM users WHERE name=?;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return id;
}

int get_all_users(User *out, int max_count)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT id, name FROM users ORDER BY name;", -1, &stmt, NULL);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count)
    {
        out[count].id = sqlite3_column_int(stmt, 0);
        strncpy(out[count].name, (const char *)sqlite3_column_text(stmt, 1), 63);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

void save_session(int user_id, Difficulty diff, int steps, float elapsed, bool completed)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M", t);

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "INSERT INTO sessions(user_id, diff, steps, time, completed, played_at)"
                       " VALUES(?,?,?,?,?,?);",
                       -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, (int)diff);
    sqlite3_bind_int(stmt, 3, steps);
    sqlite3_bind_int(stmt, 4, (int)elapsed);
    sqlite3_bind_int(stmt, 5, completed);
    sqlite3_bind_text(stmt, 6, date_str, -1, SQLITE_STATIC);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int get_sessions(int user_id, Session *out, int max_count)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT id, user_id, diff, steps, time, completed, played_at"
                       " FROM sessions WHERE user_id=? ORDER BY id DESC;",
                       -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count)
    {
        out[count].id = sqlite3_column_int(stmt, 0);
        out[count].user_id = sqlite3_column_int(stmt, 1);
        out[count].difficulty = sqlite3_column_int(stmt, 2);
        out[count].steps = sqlite3_column_int(stmt, 3);
        out[count].time = sqlite3_column_int(stmt, 4);
        out[count].completed = sqlite3_column_int(stmt, 5);
        strncpy(out[count].played_at,
                (const char *)sqlite3_column_text(stmt, 6), 31);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}
