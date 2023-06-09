#include <chrono>
#include <filesystem>
#include <iostream>

#include "main/kuzu.h"
using namespace kuzu::main;

using namespace std::chrono;

int main() {
    std::filesystem::remove_all("./2.db");
    auto database =
        std::make_unique<Database>("./2.db" /* fill db path */, SystemConfig{1ull << 30});
    auto connection = std::make_unique<Connection>(database.get());
    connection->query("CREATE NODE TABLE twt_node(node_id SERIAL, twitter_id INT64, PRIMARY "
                      "KEY(node_id));");
    connection->query("COPY twt_node FROM '/home/z473chen/Code/dataset/node.csv' "
                      "(DELIM=\",\", HEADER=true);");
    connection->query("CREATE REL TABLE twt_rel(FROM twt_node TO twt_node, MANY_MANY);");
    auto start = high_resolution_clock::now();
    connection->query("COPY twt_rel FROM '/home/z473chen/Code/dataset/rel.csv' (DELIM=\" \");");
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<seconds>(stop - start);
    std::cout << "param:25, time spent: " << duration.count() << " seconds" << std::endl;
}
