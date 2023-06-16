#include <stdio.h>

#include "c_api/kuzu.h"

int main() {
    kuzu_database* db = kuzu_database_init("1.db", 0);
    kuzu_connection* conn = kuzu_connection_init(db);
    kuzu_connection_query(
        conn, "CREATE NODE TABLE User(name STRING, age INT64, PRIMARY KEY (name));");
    kuzu_connection_query(
        conn, "CREATE NODE TABLE City(name STRING, population INT64, PRIMARY KEY (name))");
    kuzu_connection_query(conn, "CREATE REL TABLE LivesIn(FROM User TO City)");
    kuzu_connection_query(
        conn, "COPY User From \"/Users/z473chen/Desktop/code/kuzu/dataset/demo-db/csv/user.csv\"");
    kuzu_connection_query(
        conn, "COPY City FROM \"/Users/z473chen/Desktop/code/kuzu/dataset/demo-db/csv/city.csv\"");
    kuzu_connection_query(conn,
        "COPY LivesIn FROM \"/Users/z473chen/Desktop/code/kuzu/dataset/demo-db/csv/lives-in.csv\"");
    kuzu_connection_query(
        conn, "CREATE (:User {name:'Peter'})-[:LivesIn]->(:City {name:'Grafton'})");
    return 0;
}
