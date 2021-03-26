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

#ifndef HLASMPLUGIN_PARSERLIBRARY_PROCESSING_TRACER_H
#define HLASMPLUGIN_PARSERLIBRARY_PROCESSING_TRACER_H

namespace hlasm_plugin::parser_library::processing {

class processing_tracer
{
protected:
    ~processing_tracer() = default;

public:
    virtual void statement(range statement_range) = 0;
};

} // namespace hlasm_plugin::parser_library::processing

#endif