/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct SquareTreeNode {
    SquareTreeNode() = default;
    ~SquareTreeNode();
    SquareTreeNode(const SquareTreeNode&) = delete;
    SquareTreeNode& operator=(const SquareTreeNode&) = delete;

    // key (and str for values) live in one malloc with the DataItem, like StrNode
    struct DataItem {
        Str key;
        // only one of str or child are set
        Str str;
        SquareTreeNode* child = nullptr;
    };
    Vec<DataItem*> data;

    void RemoveDataAt(int idx);

    Str GetValue(Str key, int* startIdx = nullptr) const;
    SquareTreeNode* GetChild(Str key, int* startIdx = nullptr) const;
    // Set key=value, replacing any existing value item with the same key
    // (case-insensitive). Used by settings-file migration to remap name/blob
    // pairs before deserialization.
    void SetValue(Str key, Str value);
};

SquareTreeNode* ParseSquareTree(Str s);
void SerializeSquareTreeNode(str::Builder& out, SquareTreeNode* node, Str indentUnit, Str lineEnd, int depth = 0);
TempStr SerializeSquareTreeNodeTemp(SquareTreeNode*);