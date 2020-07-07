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

#include "hlasm_context.h"

#include <ctime>
#include <stdexcept>

#include "ebcdic_encoding.h"
#include "expressions/conditional_assembly/terms/ca_constant.h"
#include "instruction.h"

namespace hlasm_plugin::parser_library::context {

code_scope* hlasm_context::curr_scope() { return &scope_stack_.back(); }

const code_scope* hlasm_context::curr_scope() const { return &scope_stack_.back(); }

hlasm_context::instruction_storage hlasm_context::init_instruction_map()
{
    hlasm_context::instruction_storage instr_map;
    for (auto& [name, instr] : instruction::machine_instructions)
    {
        auto id = ids_.add(name);
        instr_map.emplace(id, instruction::instruction_array::MACH);
    }
    for (auto& [name, instr] : instruction::assembler_instructions)
    {
        auto id = ids_.add(name);
        instr_map.emplace(id, instruction::instruction_array::ASM);
    }
    for (auto& instr : instruction::ca_instructions)
    {
        auto id = ids_.add(instr.name);
        instr_map.emplace(id, instruction::instruction_array::CA);
    }
    for (auto& [name, instr] : instruction::mnemonic_codes)
    {
        auto id = ids_.add(name);
        instr_map.emplace(id, instruction::instruction_array::MNEM);
    }
    return instr_map;
}

void hlasm_context::add_system_vars_to_scope()
{
    if (curr_scope()->is_in_macro())
    {
        {
            auto SYSECT = ids().add("SYSECT");

            auto val_sect = std::make_shared<set_symbol<C_t>>(SYSECT, true, false);
            auto sect_name = ord_ctx.current_section() ? ord_ctx.current_section()->name : id_storage::empty_id;
            val_sect->set_value(*sect_name);
            curr_scope()->variables.insert({ SYSECT, val_sect });
        }

        {
            auto SYSNDX = ids().add("SYSNDX");

            auto val_ndx = std::make_shared<set_symbol<A_t>>(SYSNDX, true, false);
            val_ndx->set_value((A_t)SYSNDX_);
            curr_scope()->variables.insert({ SYSNDX, val_ndx });
        }

        {
            auto SYSSTYP = ids().add("SYSSTYP");

            auto val_styp = std::make_shared<set_symbol<C_t>>(SYSSTYP, true, false);
            if (ord_ctx.current_section())
            {
                switch (ord_ctx.current_section()->kind)
                {
                    case context::section_kind::COMMON:
                        val_styp->set_value("COM");
                        break;
                    case context::section_kind::DUMMY:
                        val_styp->set_value("DSECT");
                        break;
                    case context::section_kind::READONLY:
                        val_styp->set_value("RSECT");
                        break;
                    case context::section_kind::EXECUTABLE:
                        val_styp->set_value("CSECT");
                        break;
                    default:
                        break;
                }
            }
            curr_scope()->variables.insert({ SYSSTYP, val_styp });
        }

        {
            auto SYSLOC = ids().add("SYSLOC");

            auto var = std::make_shared<set_symbol<C_t>>(SYSLOC, true, false);

            if (ord_ctx.current_section())
            {
                var->set_value(*ord_ctx.current_section()->current_location_counter().name);
            }
            curr_scope()->variables.insert({ SYSLOC, var });
        }

        {
            auto SYSNEST = ids().add("SYSNEST");

            auto var = std::make_shared<set_symbol<A_t>>(SYSNEST, true, false);

            var->set_value((context::A_t)scope_stack_.size() - 1);

            curr_scope()->variables.insert({ SYSNEST, var });
        }

        {
            auto SYSMAC = ids().add("SYSMAC");

            std::vector<macro_data_ptr> data;

            for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it)
            {
                std::string tmp;
                if (it->is_in_macro())
                    tmp = *it->this_macro->id;
                else
                    tmp = "OPEN CODE";
                data.push_back(std::make_unique<macro_param_data_single>(std::move(tmp)));
            }

            macro_data_ptr mac_data = std::make_unique<macro_param_data_composite>(std::move(data));

            auto var = std::make_shared<system_variable>(SYSMAC, std::move(mac_data), false);

            curr_scope()->system_variables.insert({ SYSMAC, var });
        }
    }
    add_global_system_vars();
}

void hlasm_context::add_global_system_vars()
{
    auto SYSDATC = ids().add("SYSDATC");
    auto SYSDATE = ids().add("SYSDATE");
    auto SYSTIME = ids().add("SYSTIME");
    auto SYSPARM = ids().add("SYSPARM");
    auto SYSOPT_RENT = ids().add("SYSOPT_RENT");

    if (!is_in_macro())
    {
        auto datc = std::make_shared<set_symbol<C_t>>(SYSDATC, true, true);
        auto date = std::make_shared<set_symbol<C_t>>(SYSDATE, true, true);
        auto time = std::make_shared<set_symbol<C_t>>(SYSTIME, true, true);

        auto tmp_now = std::time(0);
        auto now = std::localtime(&tmp_now);

        std::string datc_val;
        std::string date_val;
        datc_val.reserve(8);
        date_val.reserve(8);
        auto year = std::to_string(now->tm_year + 1900);
        datc_val.append(year);

        if (now->tm_mon + 1 < 10)
        {
            datc_val.push_back('0');
            date_val.push_back('0');
        }

        datc_val.append(std::to_string(now->tm_mon + 1));

        date_val.append(std::to_string(now->tm_mon + 1));
        date_val.push_back('/');

        if (now->tm_mday < 10)
        {
            datc_val.push_back('0');
            date_val.push_back('0');
        }

        datc_val.append(std::to_string(now->tm_mday));

        date_val.append(std::to_string(now->tm_mday));
        date_val.push_back('/');

        datc->set_value(std::move(datc_val));

        date_val.append(year.c_str() + 2);
        date->set_value(std::move(date_val));

        globals_.insert({ SYSDATC, datc });
        globals_.insert({ SYSDATE, date });

        std::string time_val;
        if (now->tm_hour < 10)
            time_val.push_back('0');
        time_val.append(std::to_string(now->tm_hour));
        time_val.push_back(':');
        if (now->tm_min < 10)
            time_val.push_back('0');
        time_val.append(std::to_string(now->tm_min));

        time->set_value(std::move(time_val));
        globals_.insert({ SYSTIME, time });

        {
            auto val = std::make_shared<set_symbol<C_t>>(SYSPARM, true, true);
            globals_.insert({ SYSPARM, std::move(val) });
        }
        {
            auto val = std::make_shared<set_symbol<B_t>>(SYSOPT_RENT, true, true);
            globals_.insert({ SYSOPT_RENT, std::move(val) });
        }
    }

    auto glob = globals_.find(SYSDATC);
    curr_scope()->variables.insert({ glob->second->id, glob->second });
    glob = globals_.find(SYSDATE);
    curr_scope()->variables.insert({ glob->second->id, glob->second });
    glob = globals_.find(SYSTIME);
    curr_scope()->variables.insert({ glob->second->id, glob->second });
    glob = globals_.find(SYSPARM);
    curr_scope()->variables.insert({ glob->second->id, glob->second });
    glob = globals_.find(SYSOPT_RENT);
    curr_scope()->variables.insert({ glob->second->id, glob->second });
}

hlasm_context::hlasm_context(std::string file_name)
    : instruction_map_(init_instruction_map())
    , SYSNDX_(0)
    , ord_ctx(ids_)
    , lsp_ctx(std::make_shared<lsp_context>())
{
    scope_stack_.emplace_back();
    visited_files_.insert(file_name);
    push_statement_processing(processing::processing_kind::ORDINARY, std::move(file_name));
    add_global_system_vars();
}

void hlasm_context::set_source_position(position pos) { source_stack_.back().current_instruction.pos = pos; }

void hlasm_context::set_source_indices(size_t begin_index, size_t end_index, size_t end_line)
{
    source_stack_.back().begin_index = begin_index;
    source_stack_.back().end_index = end_index;
    source_stack_.back().end_line = end_line;
}

void hlasm_context::push_statement_processing(const processing::processing_kind kind)
{
    assert(!proc_stack_.empty());
    proc_stack_.emplace_back(kind, false);
}

void hlasm_context::push_statement_processing(const processing::processing_kind kind, std::string file_name)
{
    source_stack_.emplace_back(std::move(file_name));

    proc_stack_.emplace_back(kind, true);
}

void hlasm_context::pop_statement_processing()
{
    if (proc_stack_.back().owns_source)
        source_stack_.pop_back();

    proc_stack_.pop_back();
}

id_storage& hlasm_context::ids() { return ids_; }

const hlasm_context::instruction_storage& hlasm_context::instruction_map() const { return instruction_map_; }

processing_stack_t hlasm_context::processing_stack() const
{
    std::vector<processing_frame> res;

    for (size_t i = 0; i < source_stack_.size(); ++i)
    {
        res.emplace_back(source_stack_[i].current_instruction, scope_stack_.front(), file_processing_type::OPENCODE);
        for (const auto& member : source_stack_[i].copy_stack)
        {
            location loc(member.cached_definition[member.current_statement].get_base()->statement_position(),
                member.definition_location.file);
            res.emplace_back(std::move(loc), scope_stack_.front(), file_processing_type::COPY);
        }

        if (i == 0) // append macros immediately after ordinary processing
        {
            for (size_t j = 1; j < scope_stack_.size(); ++j)
            {
                auto offs = scope_stack_[j].this_macro->current_statement;

                const auto& nest = scope_stack_[j].this_macro->copy_nests[offs];
                for (size_t k = 0; k < nest.size(); ++k)
                    res.emplace_back(
                        nest[k], scope_stack_[j], k == 0 ? file_processing_type::MACRO : file_processing_type::COPY);
            }
        }
    }

    return res;
}

const std::deque<code_scope>& hlasm_context::scope_stack() const { return scope_stack_; }

const source_context& hlasm_context::current_source() const { return source_stack_.back(); }

std::vector<copy_member_invocation>& hlasm_context::current_copy_stack() { return source_stack_.back().copy_stack; }

std::vector<id_index> hlasm_context::whole_copy_stack() const
{
    std::vector<id_index> ret;

    for (auto& entry : source_stack_)
        for (auto& nest : entry.copy_stack)
            ret.push_back(nest.name);

    return ret;
}

void hlasm_context::fill_metrics_files()
{
    metrics.files = visited_files_.size();
    // for each line without '\n' at the end of the files
    metrics.lines += metrics.files;
}

const code_scope::set_sym_storage& hlasm_context::globals() const { return globals_; }

var_sym_ptr hlasm_context::get_var_sym(id_index name)
{
    auto tmp = curr_scope()->variables.find(name);
    if (tmp != curr_scope()->variables.end())
        return tmp->second;

    auto s_tmp = curr_scope()->system_variables.find(name);
    if (s_tmp != curr_scope()->system_variables.end())
        return s_tmp->second;

    if (curr_scope()->is_in_macro())
    {
        auto m_tmp = curr_scope()->this_macro->named_params.find(name);
        if (m_tmp != curr_scope()->this_macro->named_params.end())
            return m_tmp->second;
    }

    return var_sym_ptr();
}

void hlasm_context::add_sequence_symbol(sequence_symbol_ptr seq_sym)
{
    if (curr_scope()->is_in_macro())
        throw std::runtime_error("adding sequence symbols to macro definition not allowed");

    if (curr_scope()->sequence_symbols.find(seq_sym->name) == curr_scope()->sequence_symbols.end())
        curr_scope()->sequence_symbols.emplace(seq_sym->name, std::move(seq_sym));
}

const sequence_symbol* hlasm_context::get_sequence_symbol(id_index name) const
{
    label_storage::const_iterator found, end;

    if (curr_scope()->is_in_macro())
    {
        found = curr_scope()->this_macro->labels.find(name);
        end = curr_scope()->this_macro->labels.end();
    }
    else
    {
        found = curr_scope()->sequence_symbols.find(name);
        end = curr_scope()->sequence_symbols.end();
    }

    if (found != end)
        return found->second.get();
    else
        return nullptr;
}

void hlasm_context::set_branch_counter(A_t value)
{
    curr_scope()->branch_counter = value;
    ++curr_scope()->branch_counter_change;
}

int hlasm_context::get_branch_counter() const { return curr_scope()->branch_counter; }

void hlasm_context::decrement_branch_counter() { --curr_scope()->branch_counter; }

void hlasm_context::add_mnemonic(id_index mnemo, id_index op_code)
{
    auto tmp = opcode_mnemo_.find(op_code);
    if (tmp != opcode_mnemo_.end())
    {
        opcode_mnemo_.insert_or_assign(mnemo, tmp->second);
    }
    else
    {
        if (macros_.find(op_code) != macros_.end())
        {
            opcode_mnemo_.insert_or_assign(mnemo, op_code);
            return;
        }
        else if (instruction_map_.find(op_code) != instruction_map_.end())
        {
            opcode_mnemo_.insert_or_assign(mnemo, op_code);
            return;
        }
        else
            throw std::invalid_argument("undefined operation code");
    }
}

void hlasm_context::remove_mnemonic(id_index mnemo) { opcode_mnemo_.insert_or_assign(mnemo, id_storage::empty_id); }

id_index hlasm_context::get_mnemonic_opcode(id_index mnemo) const
{
    auto tmp = opcode_mnemo_.find(mnemo);
    if (tmp != opcode_mnemo_.end())
        return tmp->second;
    else
        return mnemo;
}

SET_t hlasm_context::get_attribute_value_ca(
    data_attr_kind attribute, var_sym_ptr var_symbol, std::vector<size_t> offset)
{
    switch (attribute)
    {
        case data_attr_kind::K:
            return var_symbol ? var_symbol->count(std::move(offset)) : 0;
        case data_attr_kind::N:
            return var_symbol ? var_symbol->number(std::move(offset)) : 0;
        case data_attr_kind::T:
            return get_type_attr(var_symbol, std::move(offset));
        default:
            break;
    }

    return SET_t();
}

SET_t hlasm_context::get_attribute_value_ca(data_attr_kind attribute, id_index symbol_name)
{
    switch (attribute)
    {
        case data_attr_kind::D:
            if (ord_ctx.symbol_defined(symbol_name))
                return (A_t)1;
            return (A_t)0;
        case data_attr_kind::T:
            if (ord_ctx.symbol_defined(symbol_name))
            {
                auto attr_val = ord_ctx.get_symbol(symbol_name)->attributes().get_attribute_value(attribute);
                return { (char)ebcdic_encoding::e2a[attr_val] };
            }
            return std::string("U");
        case data_attr_kind::O:
            return get_opcode_attr(symbol_name);
        default:
            if (ord_ctx.symbol_defined(symbol_name))
                return ord_ctx.get_symbol(symbol_name)->attributes().get_attribute_value(attribute);
            return symbol_attributes::default_value(attribute);
    }
}

C_t hlasm_context::get_type_attr(var_sym_ptr var_symbol, const std::vector<size_t>& offset)
{
    if (!var_symbol)
        return "U";

    C_t value;

    if (auto set_sym = var_symbol->access_set_symbol_base())
    {
        if (set_sym->type != SET_t_enum::C_TYPE)
            return "N";

        auto setc_sym = set_sym->access_set_symbol<C_t>();
        if (offset.empty())
            value = setc_sym->get_value();
        else
            value = setc_sym->get_value(offset.front());
    }
    else if (auto mac_par = var_symbol->access_macro_param_base())
    {
        auto data = mac_par->get_data(offset);

        while (dynamic_cast<const context::macro_param_data_composite*>(data))
            data = data->get_ith(0);

        value = data->get_value();
    }

    if (value.empty())
        return "O";

    auto res = expressions::ca_constant::try_self_defining_term(value);
    if (res)
        return "N";

    id_index symbol_name = ids_.add(std::move(value));
    auto tmp_symbol = ord_ctx.get_symbol(symbol_name);

    if (tmp_symbol)
        return { (char)ebcdic_encoding::e2a[tmp_symbol->attributes().type()] };

    return "U";
}

C_t hlasm_context::get_opcode_attr(id_index symbol)
{
    auto it = instruction_map_.find(symbol);

    auto mac_it = macros_.find(symbol);

    if (mac_it != macros_.end())
        return "M";

    if (it != instruction_map_.end())
    {
        auto& [opcode, type] = *it;
        switch (type)
        {
            case instruction::instruction_array::ASM:
            case instruction::instruction_array::CA:
                return "A";
            case instruction::instruction_array::MNEM:
                return "E";
            case instruction::instruction_array::MACH:
                return "O";
            default:
                break;
        }
    }

    return "U";
}

const macro_definition& hlasm_context::add_macro(id_index name,
    id_index label_param_name,
    std::vector<macro_arg> params,
    statement_block definition,
    copy_nest_storage copy_nests,
    label_storage labels,
    location definition_location)
{
    return *macros_
                .insert_or_assign(name,
                    std::make_unique<macro_definition>(name,
                        label_param_name,
                        std::move(params),
                        std::move(definition),
                        std::move(copy_nests),
                        std::move(labels),
                        std::move(definition_location)))
                .first->second.get();
}

const hlasm_context::macro_storage& hlasm_context::macros() const { return macros_; }

bool hlasm_context::is_in_macro() const { return scope_stack_.back().is_in_macro(); }

macro_invo_ptr hlasm_context::enter_macro(id_index name, macro_data_ptr label_param_data, std::vector<macro_arg> params)
{
    auto tmp = macros_.find(name);
    assert(tmp != macros_.end());

    auto& [macro_name, macro_def] = *tmp;

    auto invo((macro_def->call(std::move(label_param_data), std::move(params), ids().add("SYSLIST"))));
    scope_stack_.emplace_back(invo);
    add_system_vars_to_scope();

    visited_files_.insert(macro_def->definition_location.file);

    ++SYSNDX_;
    return invo;
}

void hlasm_context::leave_macro() { scope_stack_.pop_back(); }

macro_invo_ptr hlasm_context::this_macro() const
{
    if (is_in_macro())
        return curr_scope()->this_macro;
    return macro_invo_ptr();
}

const std::string& hlasm_context::opencode_file_name() const { return source_stack_.front().current_instruction.file; }

const std::set<std::string>& hlasm_context::get_visited_files() { return visited_files_; }

void hlasm_context::add_copy_member(id_index member, statement_block definition, location definition_location)
{
    copy_members_.try_emplace(member, member, std::move(definition), definition_location);
    visited_files_.insert(std::move(definition_location.file));
}

void hlasm_context::enter_copy_member(id_index member_name)
{
    auto tmp = copy_members_.find(member_name);
    if (tmp == copy_members_.end())
        throw std::runtime_error("unknown copy member");

    auto& [name, member] = *tmp;

    source_stack_.back().copy_stack.emplace_back(member.enter());
}

const hlasm_context::copy_member_storage& hlasm_context::copy_members() { return copy_members_; }

void hlasm_context::leave_copy_member() { source_stack_.back().copy_stack.pop_back(); }

void hlasm_context::apply_source_snapshot(source_snapshot snapshot)
{
    assert(proc_stack_.size() == 1);

    source_stack_.back().current_instruction = std::move(snapshot.instruction);
    source_stack_.back().begin_index = snapshot.begin_index;
    source_stack_.back().end_index = snapshot.end_index;
    source_stack_.back().end_line = snapshot.end_line;

    source_stack_.back().copy_stack.clear();

    for (auto& frame : snapshot.copy_frames)
    {
        auto invo = copy_members_.at(frame.copy_member).enter();
        invo.current_statement = (int)frame.statement_offset;
        source_stack_.back().copy_stack.push_back(std::move(invo));
    }
}

const code_scope& hlasm_context::current_scope() const { return *curr_scope(); }

} // namespace hlasm_plugin::parser_library::context
