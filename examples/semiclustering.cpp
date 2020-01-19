#include <algorithm>
#include <string>
#include <vector>

#include "boost/tokenizer.hpp"

#include "core/engine.hpp"
#include "io/input/inputformat_store.hpp"
#include "lib/aggregator_factory.hpp"

int MMAX = 10;

class SemiCluster {
   public:
    SemiCluster() : semiScore(1.0), members({}){};
    SemiCluster(const SemiCluster& rhs) {
        semiScore = rhs.semiScore;
        members = rhs.members;
    }
    float semiScore;
    std::vector<int> members;

    // Serialization and deserialization
    friend husky::BinStream& operator<<(husky::BinStream& stream, const SemiCluster& c) {
        stream << c.semiScore << c.members;
        return stream;
    }
    friend husky::BinStream& operator>>(husky::BinStream& stream, SemiCluster& c) {
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
    bool addToCluster(int newVertexId, std::vector<std::pair<int, float>> edges, int v_max, float fB) {
        // abort if Vmax is reached
        if (members.size() == v_max || std::find(members.begin(), members.end(), newVertexId) != members.end())
            return false;

        int innerWeight = 0;
        int outerWeight = 0;

        members.push_back(newVertexId);

        for (std::pair<int, float> e : edges) {
            int u = e.first;
            float weight = e.second;

            // If u is not in the in the members list, it is an outEdge (outside of cluster)
            // https://stackoverflow.com/questions/571394/how-to-find-out-if-an-item-is-present-in-a-stdvector
            if (std::find(members.begin(), members.end(), u) != members.end()) {
                outerWeight += weight;
            } else {
                innerWeight += weight;
            }
        }

        // compute S_c
        semiScore = (innerWeight - fB * outerWeight) / ((members.size() * (members.size() - 1)) / 2);
        return true;
    }

    bool operator<(const SemiCluster& rhs) const { return semiScore < rhs.semiScore; }
    bool operator>(const SemiCluster& rhs) const { return semiScore > rhs.semiScore; }
};

template <typename MsgT>
struct UnionCombiner {
    static void combine(std::vector<MsgT>& u1, std::vector<MsgT> const& u2) {
        for (MsgT x : u2) {
            u1.push_back(x);
        }
    }
};

class SemiVertex {
   public:
    using KeyT = int;

    SemiVertex() : neighbors(), clusters(){};
    explicit SemiVertex(const KeyT& id) : vertexId(id), neighbors(), clusters() {}
    const KeyT& id() const { return vertexId; }

    // Serialization and deserialization
    friend husky::BinStream& operator<<(husky::BinStream& stream, const SemiVertex& u) {
        stream << u.vertexId << u.neighbors << u.clusters;
        return stream;
    }
    friend husky::BinStream& operator>>(husky::BinStream& stream, SemiVertex& u) {
        stream >> u.vertexId >> u.neighbors >> u.clusters;
        return stream;
    }

    int vertexId;
    std::vector<std::pair<int, float>> neighbors;
    std::vector<SemiCluster> clusters;

    void addSemiCluster(SemiCluster s) {
        clusters.push_back(s);
        std::sort(clusters.begin(), clusters.end());
        std::reverse(clusters.begin(), clusters.end());
    }
};

void semicluster() {
    auto& infmt = husky::io::InputFormatStore::create_line_inputformat();
    infmt.set_input(husky::Context::get_param("input"));

    std::string format = husky::Context::get_param("format");

    int cMax = stoi(husky::Context::get_param("c_max"));
    int vMax = stoi(husky::Context::get_param("v_max"));
    int mMax = stoi(husky::Context::get_param("m_max"));
    float fB = stof(husky::Context::get_param("f_b"));

    husky::LOG_I << "SemiClustering with Hyperparameters:";
    husky::LOG_I << "C_max: " << cMax;
    husky::LOG_I << "V_max: " << vMax;
    husky::LOG_I << "M_max: " << mMax;
    husky::LOG_I << "f_B: " << fB;

    auto& vertex_list = husky::ObjListStore::create_objlist<SemiVertex>();

    husky::load(infmt, [&vertex_list](auto& chunk) {
        if (chunk.size() == 0)
            return;
        boost::char_separator<char> sep("\t");
        boost::tokenizer<boost::char_separator<char>> tok(chunk, sep);
        boost::tokenizer<boost::char_separator<char>>::iterator it = tok.begin();

        int v_left = std::stoi(*it++);
        int v_right = std::stoi(*it++);

        // The vertex already exists
        if (vertex_list.find(v_left)) {
            // TODO actual weight instead of 0.0
            vertex_list.find(v_left)->neighbors.push_back(std::pair<int, float>(v_right, 1.0));
        } else {
            SemiVertex v(v_left);
            v.neighbors.push_back(std::pair<int, float>(v_right, 1.0));
            vertex_list.add_object(v);
        }

        if (vertex_list.find(v_right)) {
            vertex_list.find(v_right)->neighbors.push_back(std::pair<int, float>(v_left, 1.0));
        } else {
            SemiVertex v(v_right);
            v.neighbors.push_back(std::pair<int, float>(v_left, 1.0));
            vertex_list.add_object(v);
        }
    });

    husky::globalize(vertex_list);

    if (husky::Context::get_global_tid() == 0)
        husky::LOG_I << "Loading input graph from " << husky::Context::get_param("input") << " as "
                     << husky::Context::get_param("format");

    auto& scch =
        husky::ChannelStore::create_push_combined_channel<std::vector<SemiCluster>, UnionCombiner<SemiCluster>>(
            vertex_list, vertex_list);

    // Initialization
    husky::LOG_I << "Semi Clustering started...";

    int numIters = stoi(husky::Context::get_param("iters"));

    for (int iter = 0; iter < numIters; ++iter) {
        husky::list_execute(vertex_list, [&scch, iter, vMax, fB](SemiVertex& v) {
            if (iter == 0) {
                SemiCluster s;
                s.members.push_back(v.vertexId);
                s.semiScore = 1.0;

                v.clusters.push_back(s);
            } else {
                // for each cluster, enter v and proceed
                for (SemiCluster c : scch.get(v)) {
                    SemiCluster nC(c);
                    nC.addToCluster(v.id(), v.neighbors, vMax, fB);
                    v.addSemiCluster(nC);
                }
            }

            for (auto& nb : v.neighbors) {
                scch.push(v.clusters, nb.first);
            }
        });
    }

    // RESULT

    std::vector<SemiCluster> result = {};

    husky::list_execute(vertex_list, [&result](SemiVertex& v) {
        for (SemiCluster s : v.clusters) {
            result.push_back(s);
        }
    });

    std::sort(result.begin(), result.end());
    std::reverse(result.begin(), result.end());
    result.resize(cMax);

    for (SemiCluster s : result) {
        husky::LOG_I << s.semiScore;
        for (auto& m : s.members) {
            husky::LOG_I << m;
        }
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> args;

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
