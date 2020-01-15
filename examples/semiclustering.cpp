#include <string>
#include <vector>

#include "boost/tokenizer.hpp"

#include "core/engine.hpp"
#include "io/input/inputformat_store.hpp"

class SemiCluster
{
    public:
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
};

class SemiVertex {
   public:
    using KeyT = int;

    SemiVertex() : neighbors(), clusters() {};
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


    husky::LOG_I << "Loading input graph from " << husky::Context::get_param("input") << " as " << husky::Context::get_param("format");

    auto& vertex_list = husky::ObjListStore::create_objlist<SemiVertex>();
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

    husky::LOG_I << "Semi Clustering started...";
    
    vertex_list.shuffle();

    husky::list_execute(vertex_list, [](SemiVertex& u) {
        husky::LOG_I << u.id() << " " << u.neighbors.front().first;
    });
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
