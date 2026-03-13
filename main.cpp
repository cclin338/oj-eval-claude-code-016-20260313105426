#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int M = 100; // B+ tree order
const char INDEX_FILE[] = "index_file.dat";
const char DATA_FILE[] = "data_file.dat";

struct Key {
    char str[65];
    int value;

    Key() {
        memset(str, 0, sizeof(str));
        value = 0;
    }

    Key(const char* s, int v) : value(v) {
        strcpy(str, s);
    }

    bool operator<(const Key& other) const {
        int cmp = strcmp(str, other.str);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Key& other) const {
        return strcmp(str, other.str) == 0 && value == other.value;
    }

    bool keyEqual(const char* s) const {
        return strcmp(str, s) == 0;
    }
};

struct Node {
    bool isLeaf;
    int num;
    Key keys[M];
    int children[M + 1]; // file position for internal nodes
    int next; // next leaf node position (for leaf nodes)

    Node() {
        isLeaf = true;
        num = 0;
        next = -1;
        memset(children, -1, sizeof(children));
    }
};

class BPlusTree {
private:
    fstream indexFile;
    int root;
    int fileSize;

    int allocateNode() {
        int pos = fileSize;
        fileSize += sizeof(Node);
        return pos;
    }

    void readNode(int pos, Node& node) {
        if (pos < 0) return;
        indexFile.seekg(pos);
        indexFile.read((char*)&node, sizeof(Node));
    }

    void writeNode(int pos, const Node& node) {
        indexFile.seekp(pos);
        indexFile.write((char*)&node, sizeof(Node));
        indexFile.flush();
    }

    void splitChild(int parentPos, Node& parent, int idx) {
        Node child;
        readNode(parent.children[idx], child);

        Node newNode;
        newNode.isLeaf = child.isLeaf;
        int mid = child.num / 2;

        if (child.isLeaf) {
            newNode.num = child.num - mid;
            for (int i = 0; i < newNode.num; i++) {
                newNode.keys[i] = child.keys[mid + i];
            }
            child.num = mid;
            newNode.next = child.next;
            child.next = allocateNode();
            writeNode(child.next, newNode);

            // Move keys in parent
            for (int i = parent.num; i > idx; i--) {
                parent.keys[i] = parent.keys[i - 1];
                parent.children[i + 1] = parent.children[i];
            }
            parent.keys[idx] = newNode.keys[0];
            parent.children[idx + 1] = child.next;
            parent.num++;
        } else {
            newNode.num = child.num - mid - 1;
            for (int i = 0; i < newNode.num; i++) {
                newNode.keys[i] = child.keys[mid + 1 + i];
                newNode.children[i] = child.children[mid + 1 + i];
            }
            newNode.children[newNode.num] = child.children[child.num];
            child.num = mid;

            int newPos = allocateNode();
            writeNode(newPos, newNode);

            for (int i = parent.num; i > idx; i--) {
                parent.keys[i] = parent.keys[i - 1];
                parent.children[i + 1] = parent.children[i];
            }
            parent.keys[idx] = child.keys[mid];
            parent.children[idx + 1] = newPos;
            parent.num++;
        }

        writeNode(parent.children[idx], child);
        writeNode(parentPos, parent);
    }

    void insertNonFull(int pos, const Key& key) {
        Node node;
        readNode(pos, node);

        if (node.isLeaf) {
            int i = node.num - 1;
            while (i >= 0 && key < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                i--;
            }
            node.keys[i + 1] = key;
            node.num++;
            writeNode(pos, node);
        } else {
            int i = node.num - 1;
            while (i >= 0 && key < node.keys[i]) {
                i--;
            }
            i++;

            Node child;
            readNode(node.children[i], child);
            if (child.num >= M - 1) {
                splitChild(pos, node, i);
                readNode(pos, node);
                if (node.keys[i] < key) {
                    i++;
                }
            }
            insertNonFull(node.children[i], key);
        }
    }

    void deleteFromNode(int pos, const Key& key) {
        Node node;
        readNode(pos, node);

        if (node.isLeaf) {
            int i = 0;
            while (i < node.num && node.keys[i] < key) {
                i++;
            }
            if (i < node.num && node.keys[i] == key) {
                for (int j = i; j < node.num - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                }
                node.num--;
                writeNode(pos, node);
            }
        } else {
            int i = 0;
            while (i < node.num && node.keys[i] < key) {
                i++;
            }
            deleteFromNode(node.children[i], key);
        }
    }

public:
    BPlusTree() {
        ifstream check(INDEX_FILE);
        if (check.good()) {
            check.close();
            indexFile.open(INDEX_FILE, ios::in | ios::out | ios::binary);
            indexFile.seekg(0, ios::end);
            fileSize = indexFile.tellg();
            root = 0;
        } else {
            indexFile.open(INDEX_FILE, ios::out | ios::in | ios::binary | ios::trunc);
            Node rootNode;
            rootNode.isLeaf = true;
            rootNode.num = 0;
            root = 0;
            fileSize = sizeof(Node);
            writeNode(root, rootNode);
        }
    }

    ~BPlusTree() {
        if (indexFile.is_open()) {
            indexFile.close();
        }
    }

    void insert(const char* str, int value) {
        Key key(str, value);
        Node rootNode;
        readNode(root, rootNode);

        if (rootNode.num >= M - 1) {
            Node newRoot;
            newRoot.isLeaf = false;
            newRoot.num = 0;
            newRoot.children[0] = root;

            int newRootPos = allocateNode();
            writeNode(newRootPos, newRoot);

            splitChild(newRootPos, newRoot, 0);
            root = newRootPos;
            insertNonFull(root, key);
        } else {
            insertNonFull(root, key);
        }
    }

    void remove(const char* str, int value) {
        Key key(str, value);
        deleteFromNode(root, key);
    }

    vector<int> find(const char* str) {
        vector<int> result;
        Node node;
        int pos = root;
        readNode(pos, node);

        // Navigate to leaf
        while (!node.isLeaf) {
            int i = 0;
            while (i < node.num) {
                int cmp = strcmp(str, node.keys[i].str);
                if (cmp < 0) break;
                i++;
            }
            pos = node.children[i];
            readNode(pos, node);
        }

        // Search in leaf nodes
        while (pos >= 0) {
            readNode(pos, node);
            for (int i = 0; i < node.num; i++) {
                if (node.keys[i].keyEqual(str)) {
                    result.push_back(node.keys[i].value);
                } else if (strcmp(node.keys[i].str, str) > 0) {
                    return result;
                }
            }
            pos = node.next;
        }

        return result;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);

    int n;
    cin >> n;

    BPlusTree tree;

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string index;
            int value;
            cin >> index >> value;
            tree.insert(index.c_str(), value);
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;
            tree.remove(index.c_str(), value);
        } else if (cmd == "find") {
            string index;
            cin >> index;
            vector<int> result = tree.find(index.c_str());
            if (result.empty()) {
                cout << "null\n";
            } else {
                sort(result.begin(), result.end());
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << result[j];
                }
                cout << "\n";
            }
        }
    }

    return 0;
}
