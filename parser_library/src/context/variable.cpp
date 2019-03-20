#include "variable.h"

using namespace std;

namespace hlasm_plugin::parser_library::context
{


variable_kind variable_symbol::var_kind() const
{
	return variable_kind::UNDEF_VAR_KIND;
}

set_symbol_base * variable_symbol::access_set_symbol_base()
{
	return dynamic_cast<set_symbol_base*>(this);
}

macro_param_base * variable_symbol::access_macro_param_base()
{
	return dynamic_cast<macro_param_base*>(this);
}

variable_symbol::variable_symbol(id_index name) :id(name) {}

variable_symbol::~variable_symbol() {}

SET_t_enum set_symbol_base::type() const
{
	return SET_t_enum::UNDEF_TYPE;
}

variable_kind set_symbol_base::var_kind() const
{
	return variable_kind::SET_VAR_KIND;
}

set_symbol_base::set_symbol_base(id_index name, bool is_scalar) : variable_symbol(name), is_scalar(is_scalar) {}


variable_kind macro_param_base::var_kind() const
{
	return variable_kind::MACRO_VAR_KIND;
}

macro_param_type macro_param_base::param_type() const
{
	return macro_param_type::UNDEF_PAR_TYPE;
}

keyword_param * macro_param_base::access_keyword_param()
{
	return dynamic_cast<keyword_param*>(this);
}

positional_param * macro_param_base::access_positional_param()
{
	return dynamic_cast<positional_param*>(this);
}

macro_param_base::macro_param_base(id_index name) :variable_symbol(name) {}

macro_param_type keyword_param::param_type() const
{
	return macro_param_type::KEY_PAR_TYPE;
}

const C_t & keyword_param::get_value(const std::vector<size_t>& offset) const
{
	const macro_param_data_component* tmp = data ? data.get() : default_data_.get();

	for (auto idx : offset)
	{
		tmp = tmp->get_ith(idx);
	}
	return tmp->get_value();
}

const C_t & keyword_param::get_value(size_t idx) const
{
	return (data ? data.get() : default_data_.get())->get_ith(idx)->get_value();
}

const C_t & keyword_param::get_value() const
{
	return (data ? data.get() : default_data_.get())->get_value();
}

keyword_param::keyword_param(id_index name, macro_data_ptr default_value) : macro_param_base(name), default_data_(std::move(default_value)) {}

macro_param_type positional_param::param_type() const
{
	return macro_param_type::POS_PAR_TYPE;
}

const C_t & positional_param::get_value(const std::vector<size_t>& offset) const
{
	const macro_param_data_component* tmp = (data ? data.get() : &*macro_param_data_component::dummy);

	for (auto idx : offset)
	{
		tmp = tmp->get_ith(idx);
	}
	return tmp->get_value();
}

const C_t & positional_param::get_value(size_t idx) const
{
	return (data ? data : macro_param_data_component::dummy)->get_ith(idx)->get_value();
}

const C_t & positional_param::get_value() const
{
	return (data ? data->get_value() : object_traits<C_t>::default_v());
}

positional_param::positional_param(id_index name, size_t position) : macro_param_base(name), position(position) {}

const sequence_symbol sequence_symbol::EMPTY = { nullptr,{0,0} };

sequence_symbol::operator bool() const
{
	return name;
}

}
