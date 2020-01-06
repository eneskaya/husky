#include <string>
#include <vector>

#include "boost/tokenizer.hpp"

#include "core/engine.hpp"
#include "io/input/inputformat_store.hpp"

class Vertex {
   public:
    using KeyT = int;

    explicit Vertex(const KeyT& id) : vertexId(id) {}
    const KeyT& id() const { return vertexId; }

    // Serialization and deserialization
    friend husky::BinStream& operator<<(husky::BinStream& stream, const Vertex& u) {
        stream << u.vertexId;
        return stream;
    }
    friend husky::BinStream& operator>>(husky::BinStream& stream, Vertex& u) {
        stream >> u.vertexId;
        return stream;
    }

    int vertexId;
};

void semicluster() {
    husky::LOG_I << "Semi Clustering started...";
}

int main(int argc, char** argv) {
    std::vector<std::string> args;

    if (husky::init_with_args(argc, argv, args)) {
        husky::run_job(semicluster);
        return 0;
    }
    return 1;
}
