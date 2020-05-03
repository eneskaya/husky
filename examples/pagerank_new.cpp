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

#include <chrono>

#include <string>
#include <vector>

#include "boost/tokenizer.hpp"
#include "boost/regex.hpp"

#include "core/engine.hpp"
#include "io/input/inputformat_store.hpp"
#include "io/input/nfs_binary_inputformat.hpp"
#include "lib/aggregator_factory.hpp"

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

    if (husky::Context::get_global_tid() == 0) {
        husky::LOG_I << husky::Context::get_param("input");
    }

    // Create and globalize vertex objects
    auto& vertex_list = husky::ObjListStore::create_objlist<Vertex>();

    // Reading graph files from SNAP format: http://snap.stanford.edu/data
    //
    // (each line representing an edge, with tab in between)
    // vertex   vertex
    // vertex1  vertex2
    // ...
    // TODO: Extend for directed/undirected differentiation, maybe the Vertex class needs to be able to reflect if the
    // underlying graph is directed or undirected
    // TODO: Also, the vertices and edges of a graph should have additional data attached to them
    husky::load(infmt, [&vertex_list](boost::string_ref& chunk) {
        if (chunk.size() == 0)
            return;

        boost::match_results<boost::string_ref::const_iterator> base;
        boost::regex re("[0-9]+\\s+[0-9]+");

        if (!boost::regex_match(chunk.cbegin(), chunk.cend(), base, re)) {                                                                              
            husky::LOG_I << "DOESNT MATCH! " << chunk;
            return;
        }

        boost::char_separator<char> sep(" \t");
        boost::tokenizer<boost::char_separator<char>> tok(chunk, sep);
        boost::tokenizer<boost::char_separator<char>>::iterator it = tok.begin();

        int id = std::stoi(*it++);

        // The vertex already exists
        if (vertex_list.find(id)) {
            Vertex* known_v = vertex_list.find(id);
            known_v->adj.push_back(stoi(*it++));
        } else {
            Vertex v(id);

            if (it != tok.end()) {
                v.adj.push_back(stoi(*it++));
            }
            vertex_list.add_object(std::move(v));
        }
    });

    husky::globalize(vertex_list);

    // Iterative PageRank computation
    auto& prch =
        husky::ChannelStore::create_push_combined_channel<float, husky::SumCombiner<float>>(vertex_list, vertex_list);
    int numIters = stoi(husky::Context::get_param("iters"));

    using namespace std::chrono;
    auto t1 = steady_clock::now();

    // Not sure, but the iterations represent the supersteps of the Pregel framework? TODO: find out
    for (int iter = 0; iter < numIters; ++iter) {
        if (husky::Context::get_global_tid() == 0) {
            husky::LOG_I << "----- Starting iteration # " << iter;
        }

        husky::list_execute(vertex_list, [&vertex_list, &prch, iter](Vertex& u) {
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

    auto t2 = steady_clock::now();
    auto time = duration_cast<duration<double>>(t2 - t1).count();
    if (husky::Context::get_global_tid() == 0) {
        husky::LOG_I << time << "s elapsed.";
    }

    // Aggregator
    husky::lib::Aggregator<float> totalPR;

    auto& ac = husky::lib::AggregatorFactory::get_channel();

    husky::list_execute(vertex_list, {}, {&ac}, [&totalPR](Vertex& u) {
        totalPR.update(u.pr);
    });

    if (husky::Context::get_global_tid() == 0) {
        husky::LOG_I << "Total PR value: " << totalPR.get_value();
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> args;
    args.push_back("input");
    args.push_back("iters");
    if (husky::init_with_args(argc, argv, args)) {
        husky::run_job(pagerank);
        return 0;
    }

    husky::LOG_I << "Failed initializing husky";

    return 1;
}