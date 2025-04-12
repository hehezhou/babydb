#include "storage/art.hpp"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <vector>

#if __SSE2__ == 1
#include <emmintrin.h>
#endif // __SSE2__ == 1

namespace babydb {

/**
 * TIPS
 *
 * Node Structure
 * Art have 4 different node types, the number in the type is the maximum possible sons.
 * Art partition the uint64 to 8 bytes, so each node have at most 256 children.
 * Each type use different way to manage and retrieve children.
 * For type 4 and 16, they store children and values in increasing order, and when retrieve, sequentially scan them.
 * Note that for type 16, Art will use SIMD (SSE2), we provide codes with same effects.
 * For type 48, it stores children in a prefix of an array (in any order), and use 256 byte as the array index,
 * which means child[childIndex[i]] is the child i, if exists.
 * For type 256, it stores children directly in an array, so child[i] is the child[i].
 * Notice that there is a common structure in the node: prefix.
 * It means, all keys in the subtree have the same prefix, so we store it in the node and just skip it.
 * During keys are inserted and erased, the longest common prefix on each node are changed. Art maintains carefully.
 * When retrieving keys, it cannot be ignored. Be carefully.
 *
 * TreePointer
 * It stores a pointer to a node or a data_t type (on leaf), distinguished by the last bit.
 * It's not elegant and hard to understand or use, but it is efficient.
 */

namespace Art {

enum ArtNodeType : uint8_t {
    NodeType4   = 0,
    NodeType16  = 1,
    NodeType48  = 2,
    NodeType256 = 3
};

static const uint32_t MAX_PREFIX_LENGTH = 9;
static const uint8_t EMPTY_MARKER = 48;
static const idx_t ART_KEY_LENGTH = 8;

typedef uint8_t key_t[ART_KEY_LENGTH];

// Shared structure for each type of tree nodes on ART
struct ArtNode {
    uint32_t prefixLength;
    uint16_t count;
    ArtNodeType type;
    uint8_t prefix[MAX_PREFIX_LENGTH];

    ArtNode(ArtNodeType t) : prefixLength(0), count(0), type(t) {}
};

static_assert(sizeof(ArtNode*) == sizeof(idx_t), "Please use 64-bit machine");

//! Store a data or a pointer, distinguished by the last bit.
class TreePointer {
public:
    // Equals to TreePointer(nullptr)
    TreePointer() : ptr_or_data_(0) {}
    TreePointer(ArtNode* ptr) : ptr_or_data_(reinterpret_cast<uint64_t>(ptr)) {}
    TreePointer(data_t data) : ptr_or_data_((static_cast<uint64_t>(data) << 1) | 1) {}

public:
    bool IsLeaf() {
        return ptr_or_data_ % 2 == 1;
    }
    data_t AsData() {
        return static_cast<data_t>(ptr_or_data_ >> 1);
    }
    ArtNode* AsPtr() {
        return reinterpret_cast<ArtNode*>(ptr_or_data_);
    }
    ArtNode* operator->() {
        return AsPtr();
    }
    bool Empty() {
        return ptr_or_data_ == 0;
    }

private:
    uint64_t ptr_or_data_;
};

static_assert(sizeof(TreePointer) == sizeof(idx_t));

struct Node4 : ArtNode {
    uint8_t key[4];
    TreePointer child[4];

    Node4() : ArtNode(NodeType4) {
        std::memset(key, 0, sizeof(key));
    }
};

struct Node16 : ArtNode {
    uint8_t key[16];
    TreePointer child[16];

    Node16() : ArtNode(NodeType16) {
        std::memset(key, 0, sizeof(key));
    }
};

struct Node48 : ArtNode {
    uint8_t childIndex[256];
    TreePointer child[48];

    Node48() : ArtNode(NodeType48) {
        std::memset(childIndex, EMPTY_MARKER, sizeof(childIndex));
    }
};

struct Node256 : ArtNode {
    TreePointer child[256];

    Node256() : ArtNode(NodeType256) {}
};

uint8_t flipSign(uint8_t keyByte) {
    return keyByte ^ 128;
}

static void loadKey(data_t data, key_t key) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    reinterpret_cast<uint64_t*>(key)[0] = __builtin_bswap64(data);
#else
    reinterpret_cast<uint64_t*>(key)[0] = data;
#endif
}

#if __SSE2__ == 1
static inline uint32_t ctz(uint16_t x) {
#ifdef __GNUC__
    return __builtin_ctz(x);
#else
    uint32_t n = 1;
    if ((x & 0xFF) == 0) { n += 8; x >>= 8; }
    if ((x & 0x0F) == 0) { n += 4; x >>= 4; }
    if ((x & 0x03) == 0) { n += 2; x >>= 2; }
    return n - (x & 1);
#endif
}
#endif // __SSE2__ == 1

TreePointer& findChild(ArtNode* n, uint8_t keyByte) {
    switch (n->type) {
        case NodeType4: {
            Node4* node = static_cast<Node4*>(n);
            for (idx_t i = 0; i < node->count; i++) {
                if (node->key[i] == keyByte) {
                    return node->child[i];
                }
            }
            break;
        }
        case NodeType16: {
            Node16* node = static_cast<Node16*>(n);

#if __SSE2__ == 1
            __m128i cmp = _mm_cmpeq_epi8(_mm_set1_epi8(flipSign(keyByte)),
                                         _mm_loadu_si128(reinterpret_cast<__m128i*>(node->key)));
            uint32_t bitfield = _mm_movemask_epi8(cmp) & ((1 << node->count) - 1);
            if (bitfield) {
                return node->child[ctz(bitfield)];
            }
#else // __SSE2__ == 1
            uint8_t target = flipSign(keyByte);
            for (unsigned i = 0; i < node->count; i++) {
                if (node->key[i] == target)
                    return node->child[i];
            }
#endif // __SSE2__ == 1

            break;
        }
        case NodeType48: {
            Node48* node = static_cast<Node48*>(n);
            if (node->childIndex[keyByte] != EMPTY_MARKER) {
                return node->child[node->childIndex[keyByte]];
            }
            break;
        }
        case NodeType256: {
            Node256* node = static_cast<Node256*>(n);
            return node->child[keyByte];
        }
        default: {
            B_ASSERT_MSG(false, "Invalid ArtNode Type in ART");
        }
    }
    static TreePointer nullNode = nullptr;
    return nullNode;
}

TreePointer minimum(TreePointer node) {
    if (node.Empty()) {
        return nullptr;
    }
    if (node.IsLeaf()) {
        return node;
    }
    switch (node->type) {
        case NodeType4: {
            Node4* n = static_cast<Node4*>(node.AsPtr());
            return minimum(n->child[0]);
        }
        case NodeType16: {
            Node16* n = static_cast<Node16*>(node.AsPtr());
            return minimum(n->child[0]);
        }
        case NodeType48: {
            Node48* n = static_cast<Node48*>(node.AsPtr());
            uint32_t pos = 0;
            while (n->childIndex[pos] == EMPTY_MARKER) {
                pos++;
            }
            return minimum(n->child[n->childIndex[pos]]);
        }
        case NodeType256: {
            Node256* n = static_cast<Node256*>(node.AsPtr());
            uint32_t pos = 0;
            while (n->child[pos].Empty()) {
                pos++;
            }
            return minimum(n->child[pos]);
        }
        default: {
            B_ASSERT_MSG(false, "Invalid ArtNode Type in ART");
        }
    }
    return nullptr;
}

TreePointer maximum(TreePointer node) {
    if (node.Empty()) {
        return nullptr;
    }
    if (node.IsLeaf()) {
        return node;
    }
    switch(node->type) {
        case NodeType4: {
            Node4* n = static_cast<Node4*>(node.AsPtr());
            return maximum(n->child[n->count - 1]);
        }
        case NodeType16: {
            Node16* n = static_cast<Node16*>(node.AsPtr());
            return maximum(n->child[n->count - 1]);
        }
        case NodeType48: {
            Node48* n = static_cast<Node48*>(node.AsPtr());
            uint32_t pos = 255;
            while (n->childIndex[pos] == EMPTY_MARKER) {
                pos--;
            }
            return maximum(n->child[n->childIndex[pos]]);
        }
        case NodeType256: {
            Node256* n = static_cast<Node256*>(node.AsPtr());
            uint32_t pos = 255;
            while (n->child[pos].Empty()) {
                pos--;
            }
            return maximum(n->child[pos]);
        }
        default: {
            B_ASSERT_MSG(false, "Invalid ArtNode Type in ART");
        }
    }
    return nullptr;
}

bool leafMatches(data_t leaf, key_t key, uint32_t depth) {
    if (depth != ART_KEY_LENGTH) {
        key_t leafKey;
        loadKey(leaf, leafKey);
        for (idx_t i = depth; i < ART_KEY_LENGTH; i++) {
            if (leafKey[i] != key[i]) {
                return false;
            }
        }
    }
    return true;
}

uint32_t prefixMismatch(TreePointer node, key_t key, uint32_t depth) {
    uint32_t pos;
    if (node->prefixLength > MAX_PREFIX_LENGTH) {
        for (pos = 0; pos < MAX_PREFIX_LENGTH; pos++) {
            if (key[depth + pos] != node->prefix[pos]) {
                return pos;
            }
        }
        key_t minKey;
        loadKey(minimum(node).AsData(), minKey);
        for (; pos < node->prefixLength; pos++) {
            if (key[depth + pos] != minKey[depth + pos]) {
                return pos;
            }
        }
    } else {
        for (pos = 0; pos < node->prefixLength; pos++) {
            if (key[depth + pos] != node->prefix[pos]) {
                return pos;
            }
        }
    }
    return pos;
}

TreePointer lookup(TreePointer node, key_t key, uint32_t depth) {
    bool skippedPrefix = false;
    while (!node.Empty()) {
        if (node.IsLeaf()) {
            if (!skippedPrefix && depth == ART_KEY_LENGTH) {
                return node;
            }
            key_t leafKey;
            loadKey(node.AsData(), leafKey);
            for (idx_t i = (skippedPrefix ? 0 : depth); i < ART_KEY_LENGTH; i++) {
                if (leafKey[i] != key[i]) {
                    return nullptr;
                }
            }
            return node;
        }
        if (node->prefixLength) {
            if (node->prefixLength < MAX_PREFIX_LENGTH) {
                for (uint32_t pos = 0; pos < node->prefixLength; pos++) {
                    if (key[depth + pos] != node->prefix[pos]) {
                        return nullptr;
                    }
                }
            } else {
                skippedPrefix = true;
            }
            depth += node->prefixLength;
        }
        auto &child = findChild(node.AsPtr(), key[depth]);
        node = child;
        depth++;
    }
    return nullptr;
}


void insert(TreePointer node, TreePointer* nodeRef, key_t key, uint32_t depth, data_t value);
void insertNode4(Node4* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child);
void insertNode16(Node16* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child);
void insertNode48(Node48* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child);
void insertNode256(Node256* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child);

void insert(TreePointer node, TreePointer* nodeRef, key_t key, uint32_t depth, data_t value) {
    if (node.Empty()) {
        *nodeRef = TreePointer(value);
        return;
    }
    if (node.IsLeaf()) {
        key_t existingKey;
        loadKey(node.AsData(), existingKey);
        uint32_t newPrefixLength = 0;
        while (existingKey[depth + newPrefixLength] == key[depth + newPrefixLength]) {
            newPrefixLength++;
        }
        Node4* newNode = new Node4();
        newNode->prefixLength = newPrefixLength;
        std::memcpy(newNode->prefix, key + depth, std::min(newPrefixLength, MAX_PREFIX_LENGTH));
        *nodeRef = newNode;
        insertNode4(newNode, nodeRef, existingKey[depth + newPrefixLength], node);
        insertNode4(newNode, nodeRef, key[depth + newPrefixLength], TreePointer(value));
        return;
    }
    if (node->prefixLength) {
        uint32_t mismatchPos = prefixMismatch(node, key, depth);
        if (mismatchPos != node->prefixLength) {
            Node4* newNode = new Node4();
            *nodeRef = newNode;
            newNode->prefixLength = mismatchPos;
            std::memcpy(newNode->prefix, node->prefix, std::min(mismatchPos, MAX_PREFIX_LENGTH));
            if (node->prefixLength < MAX_PREFIX_LENGTH) {
                insertNode4(newNode, nodeRef, node->prefix[mismatchPos], node);
                node->prefixLength -= (mismatchPos + 1);
                std::memmove(node->prefix, node->prefix + mismatchPos + 1, std::min(node->prefixLength, MAX_PREFIX_LENGTH));
            } else {
                node->prefixLength -= (mismatchPos + 1);
                key_t minKey;
                loadKey(minimum(node).AsData(), minKey);
                insertNode4(newNode, nodeRef, minKey[depth + mismatchPos], node);
                std::memmove(node->prefix, minKey + depth + mismatchPos + 1, std::min(node->prefixLength, MAX_PREFIX_LENGTH));
            }
            insertNode4(newNode, nodeRef, key[depth + mismatchPos], TreePointer(value));
            return;
        }
        depth += node->prefixLength;
    }
    auto node_p = node.AsPtr();
    auto &child = findChild(node_p, key[depth]);
    if (!child.Empty()) {
        insert(child, &child, key, depth + 1, value);
        return;
    }
    TreePointer newNode = TreePointer(value);
    switch (node->type) {
        case NodeType4:
            insertNode4(static_cast<Node4*>(node_p), nodeRef, key[depth], newNode);
            break;
        case NodeType16:
            insertNode16(static_cast<Node16*>(node_p), nodeRef, key[depth], newNode);
            break;
        case NodeType48:
            insertNode48(static_cast<Node48*>(node_p), nodeRef, key[depth], newNode);
            break;
        case NodeType256:
            insertNode256(static_cast<Node256*>(node_p), nodeRef, key[depth], newNode);
            break;
    }
}

void insertNode4(Node4* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child) {
    if (node->count < 4) {
        uint32_t pos;
        for (pos = 0; (pos < node->count) && (node->key[pos] < keyByte); pos++);
        std::memmove(node->key + pos + 1, node->key + pos, node->count - pos);
        std::memmove(node->child + pos + 1, node->child + pos, (node->count - pos) * sizeof(data_t));
        node->key[pos] = keyByte;
        node->child[pos] = child;
        node->count++;
    } else {
        Node16* newNode = new Node16();
        *nodeRef = newNode;
        newNode->count = 4;
        newNode->prefixLength = node->prefixLength;
        std::memcpy(newNode->prefix, node->prefix, std::min(node->prefixLength, MAX_PREFIX_LENGTH));
        for (idx_t i = 0; i < 4; i++)
            newNode->key[i] = flipSign(node->key[i]);
        std::memcpy(newNode->child, node->child, node->count * sizeof(data_t));
        delete node;
        insertNode16(newNode, nodeRef, keyByte, child);
    }
}

void insertNode16(Node16* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child) {
    if (node->count < 16) {
        uint8_t keyByteFlipped = flipSign(keyByte);

#if __SSE2__ == 1
        __m128i cmp = _mm_cmplt_epi8(_mm_set1_epi8(keyByteFlipped),
                                     _mm_loadu_si128(reinterpret_cast<__m128i*>(node->key)));
        uint16_t bitfield = _mm_movemask_epi8(cmp) & (0xFFFF >> (16 - node->count));
        uint32_t pos = bitfield ? ctz(bitfield) : node->count;
#else // __SSE2__ == 1
        unsigned pos = 0;
        while (pos < node->count && flipSign(node->key[pos]) < keyByte) {
            pos++;
        }
#endif // __SSE2__ == 1

        std::memmove(node->key + pos + 1, node->key + pos, node->count - pos);
        std::memmove(node->child + pos + 1, node->child + pos, (node->count - pos) * sizeof(data_t));
        node->key[pos] = keyByteFlipped;
        node->child[pos] = child;
        node->count++;
    } else {
        Node48* newNode = new Node48();
        *nodeRef = newNode;
        std::memcpy(newNode->child, node->child, node->count * sizeof(data_t));
        for (idx_t i = 0; i < node->count; i++) {
            newNode->childIndex[flipSign(node->key[i])] = i;
        }
        newNode->prefixLength = node->prefixLength;
        std::memcpy(newNode->prefix, node->prefix, std::min(node->prefixLength, MAX_PREFIX_LENGTH));
        newNode->count = node->count;
        delete node;
        insertNode48(newNode, nodeRef, keyByte, child);
    }
}

void insertNode48(Node48* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child) {
    if (node->count < 48) {
        uint32_t pos = node->count;
        while (!node->child[pos].Empty()) {
            pos++;
        }
        node->child[pos] = child;
        node->childIndex[keyByte] = pos;
        node->count++;
    } else {
        Node256* newNode = new Node256();
        for (idx_t i = 0; i < 256; i++) {
            if (node->childIndex[i] != EMPTY_MARKER) {
                newNode->child[i] = node->child[node->childIndex[i]];
            }
        }
        newNode->count = node->count;
        newNode->prefixLength = node->prefixLength;
        std::memcpy(newNode->prefix, node->prefix, std::min(node->prefixLength, MAX_PREFIX_LENGTH));
        *nodeRef = newNode;
        delete node;
        insertNode256(newNode, nodeRef, keyByte, child);
    }
}

void insertNode256(Node256* node, TreePointer* nodeRef, uint8_t keyByte, TreePointer child) {
    node->count++;
    node->child[keyByte] = child;
}

void erase(TreePointer node, TreePointer* nodeRef, key_t key, uint32_t depth);
void eraseNode4(Node4* node, TreePointer* nodeRef, TreePointer* leafPlace);
void eraseNode16(Node16* node, TreePointer* nodeRef, TreePointer* leafPlace);
void eraseNode48(Node48* node, TreePointer* nodeRef, uint8_t keyByte);
void eraseNode256(Node256* node, TreePointer* nodeRef, uint8_t keyByte);

void erase(TreePointer node, TreePointer* nodeRef, key_t key, uint32_t depth) {
    if (node.Empty()) {
        return;
    }
    if (node.IsLeaf()) {
        if (leafMatches(node.AsData(), key, depth)) {
            *nodeRef = nullptr;
        }
        return;
    }
    if (node->prefixLength) {
        if (prefixMismatch(node, key, depth) != node->prefixLength) {
            return;
        } else {
            depth += node->prefixLength;
        }
    }
    auto node_p = node.AsPtr();
    auto &child = findChild(node_p, key[depth]);
    if (child.IsLeaf() && leafMatches(child.AsData(), key, depth)) {
        switch (node->type) {
            case NodeType4:
                eraseNode4(static_cast<Node4*>(node_p), nodeRef, &child);
                break;
            case NodeType16:
                eraseNode16(static_cast<Node16*>(node_p), nodeRef, &child);
                break;
            case NodeType48:
                eraseNode48(static_cast<Node48*>(node_p), nodeRef, key[depth]);
                break;
            case NodeType256:
                eraseNode256(static_cast<Node256*>(node_p), nodeRef, key[depth]);
                break;
        }
    } else {
        erase(child, &child, key, depth + 1);
    }
}

void eraseNode4(Node4* node, TreePointer* nodeRef, TreePointer* leafPlace) {
    uint32_t pos = leafPlace - node->child;
    std::memmove(node->key + pos, node->key + pos + 1, node->count - pos - 1);
    std::memmove(node->child + pos, node->child + pos + 1, (node->count - pos - 1) * sizeof(data_t));
    node->count--;
    if (node->count == 1) {
        TreePointer child = node->child[0];
        if (!child.IsLeaf()) {
            uint32_t l1 = node->prefixLength;
            if (l1 < MAX_PREFIX_LENGTH) {
                node->prefix[l1] = node->key[0];
                l1++;
            }
            if (l1 < MAX_PREFIX_LENGTH) {
                uint32_t l2 = std::min(child->prefixLength, MAX_PREFIX_LENGTH - l1);
                std::memcpy(node->prefix + l1, child->prefix, l2);
                l1 += l2;
            }
            std::memcpy(child->prefix, node->prefix, std::min(l1, MAX_PREFIX_LENGTH));
            child->prefixLength += node->prefixLength + 1;
        }
        *nodeRef = child;
        delete node;
    }
}

void eraseNode16(Node16* node, TreePointer* nodeRef, TreePointer* leafPlace) {
    uint32_t pos = leafPlace - node->child;
    std::memmove(node->key + pos, node->key + pos + 1, node->count - pos - 1);
    std::memmove(node->child + pos, node->child + pos + 1, (node->count - pos - 1) * sizeof(data_t));
    node->count--;
    if (node->count == 3) {
        Node4* newNode = new Node4();
        newNode->count = node->count;
        std::memcpy(newNode->prefix, node->prefix, std::min(node->prefixLength, MAX_PREFIX_LENGTH));
        for (idx_t i = 0; i < newNode->count; i++) {
            newNode->key[i] = flipSign(node->key[i]);
        }
        std::memcpy(newNode->child, node->child, newNode->count * sizeof(data_t));
        *nodeRef = newNode;
        delete node;
    }
}

void eraseNode48(Node48* node, TreePointer* nodeRef, uint8_t keyByte) {
    node->child[node->childIndex[keyByte]] = nullptr;
    node->childIndex[keyByte] = EMPTY_MARKER;
    node->count--;
    if (node->count == 12) {
        Node16* newNode = new Node16();
        *nodeRef = newNode;
        for (idx_t b = 0; b < 256; b++) {
            if (node->childIndex[b] != EMPTY_MARKER) {
                newNode->key[newNode->count] = flipSign(b);
                newNode->child[newNode->count] = node->child[node->childIndex[b]];
                newNode->count++;
            }
        }
        delete node;
    }
}

void eraseNode256(Node256* node, TreePointer* nodeRef, uint8_t keyByte) {
    node->child[keyByte] = nullptr;
    node->count--;
    if (node->count == 37) {
        Node48* newNode = new Node48();
        *nodeRef = newNode;
        for (idx_t b = 0; b < 256; b++) {
            if (!node->child[b].Empty()) {
                newNode->childIndex[b] = newNode->count;
                newNode->child[newNode->count] = node->child[b];
                newNode->count++;
            }
        }
        delete node;
    }
}

void rangeScan(TreePointer node, key_t lowerKey, key_t upperKey, bool contain_start, bool contain_end,
               std::vector<babydb::idx_t>& row_ids) {
    // P1 TODO: Add your code here
    throw std::logic_error("Unimplemented function");
}

void destroy(TreePointer node) {
    if (node.Empty()) {
        return;
    }
    if (node.IsLeaf()) {
        return;
    }
    switch (node->type) {
        case NodeType4: {
            Node4* n4 = static_cast<Node4*>(node.AsPtr());
            for (idx_t i = 0; i < n4->count; i++) {
                destroy(n4->child[i]);
            }
            delete n4;
            break;
        }
        case NodeType16: {
            Node16* n16 = static_cast<Node16*>(node.AsPtr());
            for (idx_t i = 0; i < n16->count; i++) {
                destroy(n16->child[i]);
            }
            delete n16;
            break;
        }
        case NodeType48: {
            Node48* n48 = static_cast<Node48*>(node.AsPtr());
            for (idx_t i = 0; i < 256; i++) {
                if (n48->childIndex[i] != EMPTY_MARKER) {
                    destroy(n48->child[n48->childIndex[i]]);
                }
            }
            delete n48;
            break;
        }
        case NodeType256: {
            Node256* n256 = static_cast<Node256*>(node.AsPtr());
            for (idx_t i = 0; i < 256; i++) {
                if (!n256->child[i].Empty()) {
                    destroy(n256->child[i]);
                }
            }
            delete n256;
            break;
        }
        default: {
            B_ASSERT_MSG(false, "Invalid ArtNode Type in ART");
        }
    }
}

} // namespace Art

using namespace Art;

class ArtTree {
public:
    ArtTree() : root_() {}
    ~ArtTree() {
        destroy(root_);
    }

    TreePointer root_;
};

ArtIndex::ArtIndex(const std::string &name, Table &table, const std::string &key_name)
    : RangeIndex(name, table, key_name), art_tree_(std::make_unique<ArtTree>()) {
    auto read_guard = table.GetReadTableGuard();
    auto &rows = read_guard.Rows();
    if (!rows.empty()) {
        throw std::logic_error("Index can be only built on an empty table");
    }
}

ArtIndex::~ArtIndex() {}

void ArtIndex::InsertEntry(const data_t &key, idx_t row_id, ExecutionContext &exec_ctx) {
    // P1 TODO: Add ts support
    if (LookupKey(key, exec_ctx) != INVALID_ID) {
        throw std::logic_error("duplicated key");
    }
    key_t keyBytes;
    loadKey(key, keyBytes);
    insert(art_tree_->root_, &art_tree_->root_, keyBytes, 0, key);
}

idx_t ArtIndex::LookupKey(const data_t &key, ExecutionContext &exec_ctx) {
    // P1 TODO: This version returns the original key, change it to return the rowid & Add ts support
    key_t keyBytes;
    loadKey(key, keyBytes);
    TreePointer leaf = lookup(art_tree_->root_, keyBytes, 0);
    if (leaf.Empty() || !leaf.IsLeaf()) {
        return INVALID_ID;
    }
    return static_cast<idx_t>(leaf.AsData());
}

void ArtIndex::ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, ExecutionContext &exec_ctx) {
    // P1 TODO: Implement rangeScan & Add ts support (you can change the parameters for rangeScan)
    row_ids.clear();
    key_t lowerKey, upperKey;
    loadKey(range.start, lowerKey);
    loadKey(range.end, upperKey);

    rangeScan(art_tree_->root_, lowerKey, upperKey, range.contain_start, range.contain_end, row_ids);
}

} // namespace babydb
