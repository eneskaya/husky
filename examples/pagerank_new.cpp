// Copyright 2016 Husky Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <vector>

#include "boost/tokenizer.hpp"

#include "core/engine.hpp"
#include "io/input/inputformat_store.hpp"
#include "io/input/nfs_binary_inputformat.hpp"

class Vertex {
   public:
    using KeyT = int;

    Vertex() : pr(0.15) {}
    explicit Vertex(const KeyT& id) : vertexId(id), pr(0.15) {}
    const KeyT& id() const { return vertexId; }

    // Serialization and deserialization
    friend husky::BinStream& operator<<(husky::BinStream& stream, const Vertex& u) {
        stream << u.vertexId << u.adj << u.pr;
        return stream;
    }
    friend husky::BinStream& operator>>(husky::BinStream& stream, Vertex& u) {
        stream >> u.vertexId >> u.adj >> u.pr;
        return stream;
    }

    int vertexId;
    std::vector<int> adj;
    float pr;
};

void pagerank() {
    auto& infmt = husky::io::InputFormatStore::create_line_inputformat();
    infmt.set_input(husky::Context::get_param("input"));

    husky::LOG_I << "Something happening";
    husky::LOG_I << husky::Context::get_param("input");

    // Create and globalize vertex objects
    auto& vertex_list = husky::ObjListStore::create_objlist<Vertex>();

    husky::LOG_I << "Created vertexlist";

    // Reading graph files from SNAP format: http://snap.stanford.edu/data
    //
    // (each line representing an edge, with tab in between)
    // vertex   vertex
    // vertex1  vertex2
    // ...
    // TODO: Extend for directed/undirected differentiation, maybe the Vertex class needs to be able to reflect if the underlying graph is directed or undirected
    // TODO: Also, the vertices and edges of a graph should have additional data attached to them
    husky::load(infmt, [&vertex_list](auto& chunk) {
        husky::LOG_I << "Read a line: " << chunk;

        if (chunk.size() == 0)
            return;
        boost::char_separator<char> sep("\t");
        boost::tokenizer<boost::char_separator<char>> tok(chunk, sep);
        boost::tokenizer<boost::char_separator<char>>::iterator it = tok.begin();

        int id = std::stoi(*it++);

        // The vertex already exists
        if (vertex_list.find(id)) {
            Vertex* known_v = vertex_list.find(id);
            known_v->adj.push_back(stoi(*it++));
        } else {
            Vertex v(id);

            while (it != tok.end()) {
                v.adj.push_back(stoi(*it++));
            }
            vertex_list.add_object(std::move(v));
        }
    });

    husky::LOG_I << "globalizing on worker " << husky::Context::get_process_id();
    husky::globalize(vertex_list);

    // Iterative PageRank computation
    auto& prch =
            husky::ChannelStore::create_push_combined_channel<float, husky::SumCombiner<float>>(vertex_list, vertex_list);
    int numIters = stoi(husky::Context::get_param("iters"));

    // Not sure, but the iterations represent the supersteps of the Pregel framework? TODO: find out
    for (int iter = 0; iter < numIters; ++iter) {
        husky::list_execute(vertex_list, [&prch, iter](Vertex& u) {
            if (iter > 0)
                u.pr = 0.85 * prch.get(u) + 0.15;

            if (u.adj.size() == 0)
                return;
            float sendPR = u.pr / u.adj.size();
            for (auto& nb : u.adj) {
                prch.push(sendPR, nb);
            }
        });
    }
    // TODO: Find a better way of outputing the result
    // Maybe write to a MongoDB for iterative comparison
    husky::list_execute(vertex_list, [](Vertex& u) {
        husky::LOG_I << "VertexID: " << u.id() << ", has PR score of: " << u.pr;
    });
}

int main(int argc, char** argv) {
    husky::LOG_I << "STARTING MASTER";
    std::vector<std::string> args;
    args.push_back("hdfs_namenode");
    args.push_back("hdfs_namenode_port");
    args.push_back("input");
    args.push_back("iters");
    if (husky::init_with_args(argc, argv, args)) {
        husky::LOG_I << "Initialized PageRank App";
        husky::run_job(pagerank);
        return 0;
    }

    husky::LOG_I << "Failed initializing husky";

    return 1;
}
