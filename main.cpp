#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <set>

using namespace std;

const int BLOCK_SIZE = 200; // Larger block size for better performance
const char DATA_FILE[] = "bpt_data.dat";

struct Entry {
    char key[65];
    int value;

    Entry() {
        memset(key, 0, sizeof(key));
        value = 0;
    }

    Entry(const char* k, int v) : value(v) {
        strncpy(key, k, 64);
        key[64] = 0;
    }

    bool operator<(const Entry& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Entry& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }
};

struct Block {
    int size;
    Entry entries[BLOCK_SIZE];
    int next;

    Block() : size(0), next(-1) {}
};

class BPlusTree {
private:
    fstream file;
    int root;
    int blockCount;

    void readBlock(int pos, Block& block) {
        if (pos < 0 || pos >= blockCount) return;
        file.seekg(pos * sizeof(Block));
        file.read((char*)&block, sizeof(Block));
    }

    void writeBlock(int pos, const Block& block) {
        file.seekp(pos * sizeof(Block));
        file.write((char*)&block, sizeof(Block));
        file.flush();
    }

    int allocateBlock() {
        return blockCount++;
    }

    void insertIntoBlock(int pos, const Entry& entry) {
        Block block;
        readBlock(pos, block);

        // Check if entry already exists
        for (int i = 0; i < block.size; i++) {
            if (block.entries[i] == entry) {
                return; // Duplicate, don't insert
            }
        }

        if (block.size < BLOCK_SIZE) {
            // Insert in sorted order
            int i = block.size - 1;
            while (i >= 0 && entry < block.entries[i]) {
                block.entries[i + 1] = block.entries[i];
                i--;
            }
            block.entries[i + 1] = entry;
            block.size++;
            writeBlock(pos, block);
        } else {
            // Block is full, need to split
            Block newBlock;
            int mid = BLOCK_SIZE / 2;

            // Create temporary array
            Entry temp[BLOCK_SIZE + 1];
            int j = 0, k = 0;
            bool inserted = false;

            for (int i = 0; i < BLOCK_SIZE; i++) {
                if (!inserted && entry < block.entries[i]) {
                    temp[j++] = entry;
                    inserted = true;
                }
                temp[j++] = block.entries[i];
            }
            if (!inserted) {
                temp[j++] = entry;
            }

            // Split
            block.size = mid;
            for (int i = 0; i < mid; i++) {
                block.entries[i] = temp[i];
            }

            newBlock.size = j - mid;
            for (int i = mid; i < j; i++) {
                newBlock.entries[i - mid] = temp[i];
            }

            newBlock.next = block.next;
            block.next = allocateBlock();

            writeBlock(pos, block);
            writeBlock(block.next, newBlock);
        }
    }

    void deleteFromBlock(int pos, const Entry& entry) {
        Block block;
        readBlock(pos, block);

        bool found = false;
        int idx = -1;
        for (int i = 0; i < block.size; i++) {
            if (block.entries[i] == entry) {
                idx = i;
                found = true;
                break;
            }
        }

        if (found) {
            for (int i = idx; i < block.size - 1; i++) {
                block.entries[i] = block.entries[i + 1];
            }
            block.size--;
            writeBlock(pos, block);
        }
    }

public:
    BPlusTree() {
        ifstream check(DATA_FILE);
        bool fileExists = check.good();
        check.close();

        if (fileExists) {
            file.open(DATA_FILE, ios::in | ios::out | ios::binary);
            file.seekg(0, ios::end);
            int fileSize = file.tellg();
            blockCount = fileSize / sizeof(Block);
            root = 0;
        } else {
            file.open(DATA_FILE, ios::out | ios::in | ios::binary | ios::trunc);
            Block rootBlock;
            root = 0;
            blockCount = 1;
            writeBlock(root, rootBlock);
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const char* key, int value) {
        Entry entry(key, value);

        // Simple linear scan through blocks
        int pos = root;
        while (pos >= 0 && pos < blockCount) {
            Block block;
            readBlock(pos, block);

            // Check if this entry belongs in this block
            if (block.size == 0 ||
                strcmp(entry.key, block.entries[block.size - 1].key) <= 0 ||
                block.next == -1) {
                insertIntoBlock(pos, entry);
                return;
            }

            pos = block.next;
        }

        // If we reach here, insert in the last block (shouldn't happen normally)
        insertIntoBlock(root, entry);
    }

    void remove(const char* key, int value) {
        Entry entry(key, value);

        int pos = root;
        while (pos >= 0 && pos < blockCount) {
            Block block;
            readBlock(pos, block);

            bool found = false;
            for (int i = 0; i < block.size; i++) {
                if (block.entries[i] == entry) {
                    found = true;
                    break;
                }
            }

            if (found) {
                deleteFromBlock(pos, entry);
                return;
            }

            pos = block.next;
        }
    }

    vector<int> find(const char* key) {
        vector<int> result;

        int pos = root;
        while (pos >= 0 && pos < blockCount) {
            Block block;
            readBlock(pos, block);

            for (int i = 0; i < block.size; i++) {
                if (strcmp(block.entries[i].key, key) == 0) {
                    result.push_back(block.entries[i].value);
                }
            }

            pos = block.next;
        }

        sort(result.begin(), result.end());
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
