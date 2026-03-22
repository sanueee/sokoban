#ifndef DB_H
#define DB_H

#include "sqlite3.h"
#include "types.h"
#include "stdbool.h"

typedef struct
{
    int id;
    char name[64];
} User;

typedef struct
{
    int id;
    int user_id;
    int difficulty;
    int steps;
    int time;
    bool completed;
    char played_at[32];
} Session;

bool open(const char *path);
void close(void);

int create_user(const char *name);
int get_all_users(User *out, int max_count);
int find_user(const char* name);

void save_session(int user_id, Difficulty diff, int steps, float elapsed, bool completed);
int get_sessions(int user_id, Session *out, int max_count);

#endif
