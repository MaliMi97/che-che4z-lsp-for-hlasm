/*
 * Copyright (c) 2019 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#include "lsp_context.h"

#include <cassert>
#include <regex>
#include <sstream>
#include <string_view>

#include "context/instruction.h"
#include "ebcdic_encoding.h"

namespace hlasm_plugin::parser_library::lsp {

void lsp_context::add_file(file_info file_i)
{
    std::string name = file_i.name;
    files_.try_emplace(std::move(name), std::make_unique<file_info>(std::move(file_i)));
}

void lsp_context::add_copy(context::copy_member_ptr copy, text_data_ref_t text_data)
{
    add_file(file_info(std::move(copy), std::move(text_data)));
}

void lsp_context::add_macro(macro_info_ptr macro_i, text_data_ref_t text_data)
{
    if (macro_i->external)
    {
        assert(text_data.text != "");
        add_file(file_info(macro_i->macro_definition, std::move(text_data)));
    }

    macros_[macro_i->macro_definition] = macro_i;
}

void lsp_context::add_opencode(opencode_info_ptr opencode_i, text_data_ref_t text_data)
{
    opencode_ = std::move(opencode_i);
    add_file(file_info(opencode_->hlasm_ctx.opencode_file_name(), std::move(text_data)));

    // distribute all occurences as all files are present
    for (const auto& [_, m] : macros_)
        distribute_macro_i(m);

    distribute_file_occurences(opencode_->file_occurences);
}

location lsp_context::definition(const std::string& document_uri, const position pos) const
{
    auto [occ, macro_scope] = find_occurence_with_scope(document_uri, pos);

    if (!occ)
        return { pos, document_uri };

    auto def = find_definition_location(*occ, macro_scope);

    if (def)
        return { def->pos, def->file };
    return { pos, document_uri };
}

void collect_references(location_list& refs, const symbol_occurence& occ, const file_occurences_t& file_occs)
{
    for (const auto& [file, occs] : file_occs)
    {
        auto file_refs = file_info::find_references(occ, occs);
        for (auto&& ref : file_refs)
            refs.push_back({ std::move(ref), file });
    }
}

location_list lsp_context::references(const std::string& document_uri, const position pos) const
{
    location_list result;

    auto [occ, macro_scope] = find_occurence_with_scope(document_uri, pos);

    if (!occ)
        return { { pos, document_uri } };

    auto def = find_definition_location(*occ, macro_scope);

    if (!def)
        return { { pos, document_uri } };

    std::vector<location> scoped_result;

    if (occ->is_scoped())
    {
        if (macro_scope)
            collect_references(result, *occ, macro_scope->file_occurences_);
        else
            collect_references(result, *occ, opencode_->file_occurences);
    }
    else
    {
        for (const auto& [_, mac_i] : macros_)
            collect_references(result, *occ, mac_i->file_occurences_);
        collect_references(result, *occ, opencode_->file_occurences);
    }

    return result;
}

string_array lsp_context::hover(const std::string& document_uri, const position pos) const
{
    string_array result;
    auto [occ, macro_scope] = find_occurence_with_scope(document_uri, pos);

    if (!occ)
        return result;

    return find_hover(*occ, macro_scope);
}

size_t constexpr continuation_column = 71;

bool should_complete_instr(const text_data_ref_t& text, const position pos)
{
    std::string_view line_before = text.get_line(pos.line - 1);
    std::string_view line_so_far = text.get_line_beginning(pos);

    static const std::regex instruction_regex("^([^*][^*]\\S*\\s+\\S+|\\s+\\S*)");
    return (line_before.size() <= continuation_column || std::isspace(line_before[continuation_column]))
        && std::regex_match(line_so_far.begin(), line_so_far.end(), instruction_regex);
}

completion_list_s lsp_context::completion(const std::string& document_uri,
    const position pos,
    const char trigger_char,
    completion_trigger_kind trigger_kind) const
{
    auto file_it = files_.find(document_uri);
    if (file_it == files_.end())
        return completion_list_s();
    const text_data_ref_t& text = file_it->second->data;

    char last_char =
        (trigger_kind == completion_trigger_kind::trigger_character) ? trigger_char : text.get_character_before(pos);

    if (last_char == '&')
        return complete_var(file_it->second, pos);
    else if (last_char == '.')
        return complete_seq(file_it->second, pos);
    else if (should_complete_instr(text, pos))
        return complete_instr(file_it->second, pos);

    return completion_list_s();
}

completion_list_s lsp_context::complete_var(const file_info_ptr& file, position pos) const
{
    auto scope = file->find_scope(pos);


    completion_list_s items;
    const vardef_storage& var_defs = scope ? scope->var_definitions : opencode_->variable_definitions;
    for (const auto& vardef : var_defs)
    {
        auto cont = hover(vardef);
        completion_item_s item(
            "&" + *vardef.name, std::move(cont[0]), "&" + *vardef.name, "", completion_item_kind::var_sym);
        items.push_back(std::move(item));
    }

    return items;
}

completion_list_s lsp_context::complete_seq(const file_info_ptr& file, position pos) const
{
    auto macro_i = file->find_scope(pos);

    const context::label_storage& seq_syms =
        macro_i ? macro_i->macro_definition->labels : opencode_->hlasm_ctx.current_scope().sequence_symbols;

    completion_list_s items;
    for (const auto& sym : seq_syms)
    {
        auto cont = hover(*sym.second);
        std::string label = "." + *sym.second->name;
        items.emplace_back(label, std::move(cont[0]), label, "", completion_item_kind::seq_sym);
    }
    return items;
}

std::string get_macro_signature(const context::macro_definition& m)
{
    std::stringstream signature;
    if (*m.get_label_param_name() != "")
        signature << "&" << *m.get_label_param_name() << " ";
    signature << *m.id << " ";

    bool first = true;
    const auto& pos_params = m.get_positional_params();
    // First positional parameter is always label, even when empty
    for (size_t i = 1; i < pos_params.size(); ++i)
    {
        if (pos_params[i] == nullptr)
            continue;
        if (!first)
            signature << ",";
        else
            first = false;

        signature << "&" << *pos_params[i]->id;
    }
    for (const auto& param : m.get_keyword_params())
    {
        if (!first)
            signature << ",";
        else
            first = false;
        signature << "&" << *param->id << "=" << param->default_data->get_value();
    }
    return signature.str();
}


bool is_comment(std::string_view line)
{
    return line.size() > 0 && (line[0] == '*' || (line.size() > 1 && line.substr(0, 2) == ".*"));
}

bool is_continued_line(std::string_view line)
{
    return line.size() > continuation_column && !isspace(line[continuation_column]);
}


std::string get_macro_documentation(const text_data_ref_t& text, const macro_info& m)
{
    // We start at line where the name of the macro is written


    size_t MACRO_line = m.definition_location.pos.line - 1;
    // Skip over MACRO statement
    size_t doc_before_begin_line = MACRO_line - 1;
    // Find the beginning line of documentation written in front of macro definition
    while (doc_before_begin_line != -1 && is_comment(text.get_line(doc_before_begin_line)))
        --doc_before_begin_line;
    ++doc_before_begin_line;

    std::string_view doc_before = text.get_range_content({ { doc_before_begin_line, 0 }, { MACRO_line, 0 } });

    // Find the end line of macro definition
    size_t macro_def_end_line = m.definition_location.pos.line;
    while (macro_def_end_line < text.line_indices.size() && is_continued_line(text.get_line(macro_def_end_line)))
        ++macro_def_end_line;
    ++macro_def_end_line;

    std::string_view macro_def =
        text.get_range_content({ { m.definition_location.pos.line, 0 }, { macro_def_end_line, 0 } });

    // Find the end line of documentation that comes after the macro definition
    size_t doc_after_end_line = macro_def_end_line;

    while (doc_after_end_line < text.line_indices.size() && is_comment(text.get_line(doc_after_end_line)))
        ++doc_after_end_line;

    std::string_view doc_after = text.get_range_content({ { macro_def_end_line, 0 }, { doc_after_end_line, 0 } });

    std::string result = "```\n";
    result.append(macro_def);
    result.append(doc_before);
    result.append(doc_after);
    result.append("```\n");

    return result;
}

completion_list_s lsp_context::complete_instr(const file_info_ptr& file, position pos) const
{
    completion_list_s result = completion_item_s::instruction_completion_items_;

    for (const auto& macro_i : macros_)
    {
        const context::macro_definition& m = *macro_i.second->macro_definition;

        std::string s = macro_i.second->definition_location.file + "["
            + std::to_string(macro_i.second->definition_location.pos.line) + ","
            + std::to_string(macro_i.second->definition_location.pos.column);
        
        std::string macro_documentation;
        
        if (auto it = files_.find(macro_i.second->definition_location.file); it != files_.end())
            macro_documentation = get_macro_documentation(it->second->data, *macro_i.second);
        
        //get_macro_documentation(file->data, m)
        result.emplace_back(*m.id,
            get_macro_signature(m),
            *m.id, std::move(macro_documentation),
            completion_item_kind::macro);
        // CHECKNI AMXOPUT
    }

    return result;
}


template<typename T>
bool files_present(
    const std::unordered_map<std::string, file_info_ptr>& files, const std::unordered_map<std::string, T>& scopes)
{
    bool present = true;
    for (const auto& [file, _] : scopes)
        present &= files.find(file) != files.end();
    return present;
}

void lsp_context::distribute_macro_i(macro_info_ptr macro_i)
{
    assert(files_present(files_, macro_i->file_scopes_));

    for (const auto& [file, slices] : macro_i->file_scopes_)
        files_[file]->update_slices(file_slice_t::transform_slices(slices, macro_i));

    distribute_file_occurences(macro_i->file_occurences_);
}

void lsp_context::distribute_file_occurences(const file_occurences_t& occurences)
{
    assert(files_present(files_, occurences));

    for (const auto& [file, occs] : occurences)
        files_[file]->update_occurences(occs);
}

occurence_scope_t lsp_context::find_occurence_with_scope(const std::string& document_uri, const position pos) const
{
    if (auto file = files_.find(document_uri); file != files_.end())
        return file->second->find_occurence_with_scope(pos);
    return std::make_pair(nullptr, nullptr);
}

template<lsp::occurence_kind kind>
struct occ_type_helper
{};
template<>
struct occ_type_helper<lsp::occurence_kind::ORD>
{
    using ret_type = const context::symbol*;
};
template<>
struct occ_type_helper<lsp::occurence_kind::SEQ>
{
    using ret_type = const context::sequence_symbol*;
};
template<>
struct occ_type_helper<lsp::occurence_kind::VAR>
{
    using ret_type = const variable_symbol_definition*;
};
template<>
struct occ_type_helper<lsp::occurence_kind::INSTR>
{
    using ret_type = context::opcode_t;
};
template<>
struct occ_type_helper<lsp::occurence_kind::COPY_OP>
{
    using ret_type = const context::copy_member*;
};


template<lsp::occurence_kind kind>
typename occ_type_helper<kind>::ret_type find_definition(const symbol_occurence& occ,
    macro_info_ptr macro_i,
    const opencode_info& opencode,
    const std::unordered_map<std::string, file_info_ptr>& files)
{
    if constexpr (kind == lsp::occurence_kind::ORD)
    {
        return opencode.hlasm_ctx.ord_ctx.get_symbol(occ.name);
    }
    if constexpr (kind == lsp::occurence_kind::SEQ)
    {
        const context::label_storage& seq_syms =
            macro_i ? macro_i->macro_definition->labels : opencode.hlasm_ctx.current_scope().sequence_symbols;
        if (auto sym = seq_syms.find(occ.name); sym != seq_syms.end())
            return sym->second.get();
        return nullptr;
    }
    if constexpr (kind == lsp::occurence_kind::VAR)
    {
        const vardef_storage& var_syms = macro_i ? macro_i->var_definitions : opencode.variable_definitions;

        auto sym =
            std::find_if(var_syms.begin(), var_syms.end(), [&](const auto& var) { return var.name == occ.name; });

        if (sym != var_syms.end())
            return &*sym;
        return nullptr;
    }
    if constexpr (kind == lsp::occurence_kind::INSTR)
    {
        context::opcode_t retval;
        retval.machine_opcode = occ.name;
        retval.macro_opcode = occ.opcode;
        return retval;
    }
    if constexpr (kind == lsp::occurence_kind::COPY_OP)
    {
        const context::copy_member* retval = nullptr;
        auto copy = std::find_if(files.begin(), files.end(), [&](const auto& f) {
            return f.second->type == file_type::COPY
                && std::get<context::copy_member_ptr>(f.second->owner)->name == occ.name;
        });
        if (copy != files.end())
            retval = &*std::get<context::copy_member_ptr>(copy->second->owner);
        return retval;
    }
}

std::optional<location> lsp_context::find_definition_location(const symbol_occurence& occ, macro_info_ptr macro_i) const
{
    switch (occ.kind)
    {
        case lsp::occurence_kind::ORD: {
            auto sym = find_definition<lsp::occurence_kind::ORD>(occ, macro_i, *opencode_, files_);
            if (sym)
                return sym->symbol_location;
            break;
        }
        case lsp::occurence_kind::SEQ: {
            auto sym = find_definition<lsp::occurence_kind::SEQ>(occ, macro_i, *opencode_, files_);
            if (sym)
                return sym->symbol_location;
            break;
        }
        case lsp::occurence_kind::VAR: {
            auto sym = find_definition<lsp::occurence_kind::VAR>(occ, macro_i, *opencode_, files_);
            if (sym)
            {
                if (macro_i)
                    return location(
                        sym->def_position, macro_i->macro_definition->copy_nests[sym->def_location].back().file);
                return location(sym->def_position, sym->file);
            }
            break;
        }
        case lsp::occurence_kind::INSTR: {
            auto sym = find_definition<lsp::occurence_kind::INSTR>(occ, macro_i, *opencode_, files_);
            if (sym.macro_opcode && macros_.find(sym.macro_opcode) != macros_.end())
                return macros_.find(sym.macro_opcode)->second->definition_location;
            break;
        }
        case lsp::occurence_kind::COPY_OP: {
            auto sym = find_definition<lsp::occurence_kind::COPY_OP>(occ, macro_i, *opencode_, files_);
            if (sym)
                return sym->definition_location;
            break;
        }
        default:
            break;
    }
    return std::nullopt;
}

string_array lsp_context::find_hover(const symbol_occurence& occ, macro_info_ptr macro_i) const
{
    switch (occ.kind)
    {
        case lsp::occurence_kind::ORD: {
            auto sym = find_definition<lsp::occurence_kind::ORD>(occ, macro_i, *opencode_, files_);
            if (sym)
                return hover(*sym);
            break;
        }
        case lsp::occurence_kind::SEQ: {
            auto sym = find_definition<lsp::occurence_kind::SEQ>(occ, macro_i, *opencode_, files_);
            if (sym)
                return hover(*sym);
            break;
        }
        case lsp::occurence_kind::VAR: {
            auto sym = find_definition<lsp::occurence_kind::VAR>(occ, macro_i, *opencode_, files_);
            if (sym)
                return hover(*sym);
            break;
        }
        case lsp::occurence_kind::INSTR: {
            auto sym = find_definition<lsp::occurence_kind::INSTR>(occ, macro_i, *opencode_, files_);
            return hover(sym);
        }
        case lsp::occurence_kind::COPY_OP: {
            auto sym = find_definition<lsp::occurence_kind::COPY_OP>(occ, macro_i, *opencode_, files_);
            if (sym)
                return hover(*sym);
            break;
        }
        default:
            break;
    }
    return {};
}

string_array lsp_context::hover(const context::symbol& sym) const
{
    string_array result;
    if (sym.value().value_kind() == context::symbol_value_kind::UNDEF)
        return result;

    if (sym.value().value_kind() == context::symbol_value_kind::ABS)
    {
        result = { std::to_string(sym.value().get_abs()) };
        result.push_back("Absolute Symbol");
    }
    else if (sym.value().value_kind() == context::symbol_value_kind::RELOC)
    {
        result = { sym.value().get_reloc().to_string() }; // move to_string method from that class to this class
        result.push_back("Relocatable Symbol");
    }

    const auto& attrs = sym.attributes();
    if (attrs.is_defined(context::data_attr_kind::L))
        result.push_back("L: " + std::to_string(attrs.length()));
    if (attrs.is_defined(context::data_attr_kind::I))
        result.push_back("I: " + std::to_string(attrs.integer()));
    if (attrs.is_defined(context::data_attr_kind::S))
        result.push_back("S: " + std::to_string(attrs.scale()));
    if (attrs.is_defined(context::data_attr_kind::T))
        result.push_back("T: " + ebcdic_encoding::to_ascii((unsigned char)attrs.type()));

    return result;
}

string_array lsp_context::hover(const context::sequence_symbol& sym) const
{
    return string_array { "Sequence symbol" };
}

string_array lsp_context::hover(const variable_symbol_definition& sym) const
{
    string_array result;

    if (sym.macro_param)
        result.push_back("MACRO parameter");
    else
        switch (sym.type)
        {
            case context::SET_t_enum::A_TYPE:
                result.push_back("SETA variable");
                break;
            case context::SET_t_enum::B_TYPE:
                result.push_back("SETB variable");
                break;
            case context::SET_t_enum::C_TYPE:
                result.push_back("SETC variable");
                break;
            default:
                break;
        }

    return result;
}

string_array lsp_context::hover(const context::opcode_t& sym) const { return string_array(); }

string_array lsp_context::hover(const context::copy_member& sym) const { return string_array(); }

} // namespace hlasm_plugin::parser_library::lsp
