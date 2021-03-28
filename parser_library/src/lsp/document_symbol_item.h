

#ifndef LSP_DOCUMENT_SYMBOL_ITEM_H
#define LSP_DOCUMENT_SYMBOL_ITEM_H

#include <string>
#include <vector>

#include "protocol.h"

namespace hlasm_plugin::parser_library::lsp {

using document_symbol_list_s = std::vector<document_symbol_item_s>;

// representation of document symbol item based on LSP
struct document_symbol_item_s
{
public:
    // contents directly passed via constructor
    document_symbol_item_s(std::string name,
        document_symbol_kind kind,
        range symbol_range,
        range symbol_selection_range);

    // several features of document symbol item from LSP
    std::string name;
    document_symbol_kind kind;
    range symbol_range;
    range symbol_selection_range;
};
    
} // namespace hlasm_plugin::parser_library::lsp

#endif