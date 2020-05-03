#include <algorithm>
#include <string>
#include <vector>

#include "boost/tokenizer.hpp"
#include "boost/regex.hpp"

#include "core/engine.hpp"
#include "io/input/inputformat_store.hpp"
#include "lib/aggregator_factory.hpp"

int MMAX = 10;

class SemiCluster {
public:
    SemiCluster() : semiScore(1.0), members({}) {};

    SemiCluster(const SemiCluster &rhs) {
        semiScore = rhs.semiScore;
        members = rhs.members;
    }

    float semiScore;
    std::vector<int> members;

    // Serialization and deserialization
    friend husky::BinStream &operator<<(husky::BinStream &stream, const SemiCluster &c) {
        stream << c.semiScore << c.members;
        return stream;
    }

    friend husky::BinStream &operator>>(husky::BinStream &stream, SemiCluster &c) {
        stream >> c.semiScore >> c.members;
        return stream;
    }

    /**
     * Add a new vertex to this cluster, by appending to members.
     * Also update the semiScore.
     * For this, we look into the edges of the new vertex.
     * Iterating over the edges, for each edge u, we will check wether it is in the members list already.
     * If yes, the weight of that edge is added to innerWeight.
     * If no, the weight of that edge is added to outerWeight.
     */
    bool addToCluster(int newVertexId, std::vector <std::pair<int, float>> edges, int v_max, float fB) {
        // abort if Vmax is reached
        if (members.size() == v_max || std::find(members.begin(), members.end(), newVertexId) != members.end())
            return false;

        float innerWeight = 0;
        float outerWeight = 0;

        members.push_back(newVertexId);

        for (std::pair<int, float> e : edges) {
            int u = e.first;
            float weight = e.second;

            // If u is not in the in the members list, it is an outEdge (outside of cluster)
            // https://stackoverflow.com/questions/571394/how-to-find-out-if-an-item-is-present-in-a-stdvector
            if (std::find(members.begin(), members.end(), u) == members.end()) {
                outerWeight += weight;
            } else {
                innerWeight += weight;
            }
        }

        // compute S_c
        semiScore = (innerWeight - fB * outerWeight) / ((members.size() * (members.size() - 1)) / 2);
        return true;
    }

    bool operator<(const SemiCluster &rhs) const { return semiScore < rhs.semiScore; }

    bool operator>(const SemiCluster &rhs) const { return semiScore > rhs.semiScore; }

    bool operator==(const SemiCluster &rhs) const {
        // if (semiScore != rhs.semiScore)
        //     return false;
        
        if (members.size() != rhs.members.size())
            return false;

        std::vector<int> copyThis = members;
        std::vector<int> copyRhs = rhs.members;

        std::sort(copyThis.begin(), copyThis.end());
        std::sort(copyRhs.begin(), copyRhs.end());
        
        for(std::size_t i = 0; i < copyThis.size(); ++i) {
            if (copyThis[i] != copyRhs[i])
                return false;
        }

        return true;
    }
};

template<typename MsgT>
struct UnionCombiner {
    static void combine(std::vector<MsgT> &u1, std::vector<MsgT> const &u2) {
        for (MsgT x : u2) {
            u1.push_back(x);
        }
    }
};

class SemiVertex {
public:
    using KeyT = int;

    SemiVertex() : neighbors(), clusters() {};

    explicit SemiVertex(const KeyT &id) : vertexId(id), neighbors(), clusters() {}

    const KeyT &id() const { return vertexId; }

    // Serialization and deserialization
    friend husky::BinStream &operator<<(husky::BinStream &stream, const SemiVertex &u) {
        stream << u.vertexId << u.neighbors << u.clusters;
        return stream;
    }

    friend husky::BinStream &operator>>(husky::BinStream &stream, SemiVertex &u) {
        stream >> u.vertexId >> u.neighbors >> u.clusters;
        return stream;
    }

    int vertexId;
    std::vector <std::pair<int, float>> neighbors;
    std::vector <SemiCluster> clusters;

    bool addSemiCluster(SemiCluster s, int vMax) {
        // for (SemiCluster c : clusters) {
        //     if (c == s)
        //         return false;
        // }

        clusters.push_back(s);
        // std::sort(clusters.begin(), clusters.end());
        // std::reverse(clusters.begin(), clusters.end());

        if (clusters.size() > vMax) {
            clusters.resize(vMax);
        }

        return true;
    }
};

std::string printCluster(SemiCluster const& data) {
    std::stringstream out;

    out << "score: " << data.semiScore << ", members: " << " {";
    for (int m : data.members) {
        out << " " << m << " ";
    }
    out <<  "} ";

    return out.str();
}

std::string printSemiVertex(SemiVertex const& data) {
    std::stringstream out;

    out << std::endl;
    out << std::endl;

    out << " ------- " << data.id() << " ------- " << std::endl;

    out << "adj =  ";
    for (std::pair<int, float> n : data.neighbors) {
        out << "[ " << n.first << "; " << n.second << " ] ";
    }
    out << std::endl;

    out << "\n";
    for (const SemiCluster& s : data.clusters) {
        out << "[ " << printCluster(s) << " ] " << "\n";
    }
    out << "\n" << std::endl;
    out << std::endl;

    return out.str();

}

void semicluster() {
    auto &infmt = husky::io::InputFormatStore::create_line_inputformat();
    infmt.set_input(husky::Context::get_param("input"));
    

    int cMax = stoi(husky::Context::get_param("c_max"));
    int vMax = stoi(husky::Context::get_param("v_max"));
    int mMax = stoi(husky::Context::get_param("m_max"));
    float fB = stof(husky::Context::get_param("f_b"));


    if (husky::Context::get_global_tid() == 0) {
        husky::LOG_I << "SemiClustering with Hyperparameters:";
        husky::LOG_I << "File: " << husky::Context::get_param("input");
        husky::LOG_I << "C_max: " << cMax;
        husky::LOG_I << "V_max: " << vMax;
        husky::LOG_I << "M_max: " << mMax;
        husky::LOG_I << "f_B: " << fB;
    }

    auto &vertex_list = husky::ObjListStore::create_objlist<SemiVertex>();

    husky::load(infmt, [&vertex_list](boost::string_ref &chunk) {
        if (chunk.empty())
            return;

        // boost::match_results<boost::string_ref::const_iterator> base;
        // boost::regex re("[0-9]+\\s+[0-9]+");

        // if (!boost::regex_match(chunk.cbegin(), chunk.cend(), base, re)) {                                                                              
        //     husky::LOG_I << "DOESNT MATCH! " << chunk;
        //     return;
        // }

        boost::char_separator<char> sep(" \t");
        boost::tokenizer <boost::char_separator<char>> tok(chunk, sep);
        boost::tokenizer < boost::char_separator < char >> ::iterator it = tok.begin();

        int v_left = std::stoi(*it++);
        int v_right = std::stoi(*it++);
        float weight = std::stof(*it++);
        // float weight = 1.0;

        // The vertex already exists
        if (vertex_list.find(v_left)) {
            vertex_list.find(v_left)->neighbors.push_back(std::pair<int, float>(v_right, weight));
        } else {
            SemiVertex v(v_left);
            v.neighbors.push_back(std::pair<int, float>(v_right, weight));
            vertex_list.add_object(v);
        }

        if (vertex_list.find(v_right)) {
            vertex_list.find(v_right)->neighbors.push_back(std::pair<int, float>(v_left, weight));
        } else {
            SemiVertex v(v_right);
            v.neighbors.push_back(std::pair<int, float>(v_left, weight));
            vertex_list.add_object(v);
        }
    });

    husky::globalize(vertex_list);

    auto &scch = husky::ChannelStore::create_push_combined_channel< std::vector < SemiCluster > , UnionCombiner<SemiCluster>>(vertex_list, vertex_list);
    auto &neighborsBroadcast = husky::ChannelStore::create_broadcast_channel<int, std::vector<std::pair<int, float>>>(vertex_list);

    // Thou shalt broadcast thy neighbours...
    husky::list_execute(vertex_list, [&neighborsBroadcast](SemiVertex& v) {
        neighborsBroadcast.broadcast(v.id(), v.neighbors);
    });

    int numIters = stoi(husky::Context::get_param("iters"));

    using namespace std::chrono;
    auto t1 = steady_clock::now();

    for (int i = 0; i < numIters; ++i) {
        if (husky::Context::get_global_tid() == 0) {
            husky::LOG_I << "---- Iteration ----" << i;
        }

        husky::list_execute(vertex_list, [&vertex_list, &scch, mMax, vMax, fB, i, &neighborsBroadcast](SemiVertex &v) {
            if (i == 0) {
                SemiCluster s;
                s.members.push_back(v.vertexId);
                s.semiScore = 1.0;

                v.clusters.push_back(s);

                for (auto &nb : v.neighbors) {
                    scch.push(v.clusters, nb.first);
                }
            } else {
                // Get the ClusterList of the neighbors of v...
                std::vector<SemiCluster> incoming = scch.get(v);
                bool changed = false;

                // Now, for every cluster, insert v...
                for (SemiCluster c : incoming) {
                    if (std::find(c.members.begin(), c.members.end(), v.id()) != c.members.end()) {
                        if (husky::Context::get_global_tid() == 0) {
                            // husky::LOG_I << "Continuing...";
                        }
                        continue;
                    }

                    // if (husky::Context::get_global_tid() == 0) {
                    //     husky::LOG_I << "Copy semicluster...";
                    // }

                    SemiCluster nC(c);

                    // if (husky::Context::get_global_tid() == 0) {
                    //     husky::LOG_I << "Gather neighbors STARt...";
                    // }
                    // Gather all the neighbors of the member, so we have the complete neighbourhood of the cluster members.
                    // std::vector<std::pair<int,float>> allNeighbors;
                    // allNeighbors.insert(allNeighbors.end(), v.neighbors.begin(), v.neighbors.end());
                    // for (std::pair<int,float> u : v.neighbors) {
                    //     auto& neighborsOfU = neighborsBroadcast.get(u.first);
                    //     // https://stackoverflow.com/questions/2551775/appending-a-vector-to-a-vector
                    //     for (std::pair<int,float> x : neighborsOfU) {
                    //         if (x.first != v.id()) {
                    //             allNeighbors.push_back(x);
                    //         }
                    //     }
                    // }
                    // if (husky::Context::get_global_tid() == 0) {
                    //     husky::LOG_I << "Gather neighbors END...";
                    // }

                    changed = nC.addToCluster(v.id(), v.neighbors, vMax, fB);
                    v.addSemiCluster(nC, vMax);
                }

                if (changed) {
                    for (auto &nb : v.neighbors) {
                        scch.push(v.clusters, nb.first);
                    }
                }
            }

            // if (husky::Context::get_global_tid() == 0) {
            //     husky::LOG_I << printSemiVertex(v);
            // }
        });
    }

    auto t2 = steady_clock::now();
    auto time = duration_cast<duration<double>>(t2 - t1).count();
    if (husky::Context::get_global_tid() == 0) {
        husky::LOG_I << time << "s elapsed.";
    }

    // RESULT
    
    // TODO use aggregators
    // husky::list_execute(vertex_list, [&cMax](SemiVertex &v) {
    //     husky::LOG_I;
    //     husky::LOG_I << " ------- For vertex: " << v.id();
    //     v.clusters.resize(cMax);

    //     for (SemiCluster s : v.clusters) {
    //         husky::LOG_I << printCluster(s);
    //     }
    //     husky::LOG_I << " ------ ";
    // });

    // if (husky::Context::get_global_tid() == 0) {
    // }
}

int main(int argc, char **argv) {
    std::vector <std::string> args;

    // The graph file
    args.push_back("input");
    // one of: snap, weighted
    args.push_back("format");
    // iteration count (Supersteps)
    args.push_back("iters");

    // SemiCluster Algorithm hyper-parameters:

    // C_max
    args.push_back("c_max");
    // V_max
    args.push_back("v_max");
    // M_max
    args.push_back("m_max");
    // f_B
    args.push_back("f_b");

    if (husky::init_with_args(argc, argv, args)) {
        husky::run_job(semicluster);
        return 0;
    }
    return 1;
}
