#include "gtest/gtest.h"
#include "analyzer_fixture.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::lsp;

std::string write_document_symbol_list(const document_symbol_list_s& outline)
{
    std::string result = "";
    for (auto& item : outline)
    {
        result += "name: "+*item.name+" kind: "+std::to_string((int)item.kind)+" range: {{"+
        std::to_string(item.symbol_range.start.line)+","+std::to_string(item.symbol_range.start.column)+"},{"+
        std::to_string(item.symbol_range.end.line)+","+std::to_string(item.symbol_range.end.column)+"}} children start:\n";
        result += write_document_symbol_list(item.children);
        result += "children end\n";
    }
    return result;
}

void write_it(const document_symbol_list_s& outline, const document_symbol_list_s& expected)
{
    std::ofstream a;
    a.open("/home/malimi/a.txt",std::ofstream::app);
    a << write_document_symbol_list(outline);
    a << "\n------------------------------------------------------------------------\n";
    a << write_document_symbol_list(expected);
    a.close();
}

struct lsp_context_document_symbol_ord_sect : analyzer_fixture
{
    const static inline std::string input = R"(
C CSECT
P LR 1,1
)";
    lsp_context_document_symbol_ord_sect()
        : analyzer_fixture(input) {}
};

bool is_permutation_with_permutations(const document_symbol_list_s& lhs, const document_symbol_list_s& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (auto& item : lhs)
    {
        auto i = std::find(rhs.begin(),rhs.end(),item);
        if (i == rhs.end())
        {
            return false;
        }
        if (!is_permutation_with_permutations(item.children,i->children))
        {
            return false;
        }
    }
    return true;
}

TEST_F(lsp_context_document_symbol_ord_sect, sect)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string C = "C", P = "P";
    document_symbol_list_s expected = {
        document_symbol_item_s{
            &C,
            document_symbol_kind::EXECUTABLE,
            range{{1,0},{1,0}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &P,
                    document_symbol_kind::MACH,
                    range{{2,2},{2,2}}
                }
            }
        }
    };
    EXPECT_EQ(outline, expected);
}

struct lsp_context_document_symbol_ord_equ_in_sect : analyzer_fixture
{
    const static inline std::string input = R"(
C CSECT
P LR 1,1
E EQU 1
)";
    lsp_context_document_symbol_ord_equ_in_sect()
        : analyzer_fixture(input) {}
};

TEST_F(lsp_context_document_symbol_ord_equ_in_sect, equ_in_sect)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string C = "C", P = "P", E = "E";
    document_symbol_list_s expected = {
        document_symbol_item_s{
            &C,
            document_symbol_kind::EXECUTABLE,
            range{{1,0},{1,0}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &P,
                    document_symbol_kind::MACH,
                    range{{2,2},{2,2}}
                }
            }
        }, document_symbol_item_s{
            &E,
            document_symbol_kind::EQU,
            range{{3,0},{3,0}}
        }
    };
    EXPECT_TRUE(std::is_permutation(outline.begin(),outline.end(), expected.begin()));
}

struct lsp_context_document_symbol_ord_sect_in_sect : analyzer_fixture
{
    const static inline std::string input = R"(
C1 CSECT
P1 LR 1,1
C2 CSECT
P2 LR 1,1
)";
    lsp_context_document_symbol_ord_sect_in_sect()
        : analyzer_fixture(input) {}
};

TEST_F(lsp_context_document_symbol_ord_sect_in_sect, sect_in_sect)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string C1 = "C1", P1 = "P1", C2 = "C2", P2 = "P2";
    document_symbol_list_s expected = {
        document_symbol_item_s{
            &C1,
            document_symbol_kind::EXECUTABLE,
            range{{1,0},{1,0}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &P1,
                    document_symbol_kind::MACH,
                    range{{2,2},{2,2}}
                }
            }
        }, document_symbol_item_s{
            &C2,
            document_symbol_kind::EXECUTABLE,
            range{{3,0},{3,0}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &P2,
                    document_symbol_kind::MACH,
                    range{{4,2},{4,2}}
                }
            }
        }
    };
    EXPECT_TRUE(std::is_permutation(outline.begin(),outline.end(), expected.begin()));
}

struct lsp_context_document_symbol_ord_not_used_macro : analyzer_fixture
{
    const static inline std::string input = R"(
    MACRO
    M
E   EQU 1
    MEND
)";
    lsp_context_document_symbol_ord_not_used_macro()
        : analyzer_fixture(input) {}
};

TEST_F(lsp_context_document_symbol_ord_not_used_macro, not_used_macro)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string C1 = "C1", P1 = "P1", C2 = "C2", P2 = "P2";
    document_symbol_list_s expected = {};
    EXPECT_EQ(outline,expected);
}

struct lsp_context_document_symbol_ord_macro : analyzer_fixture
{
    const static inline std::string input = R"(
    MACRO
    M
E   EQU 1
    MEND
    M
)";
    lsp_context_document_symbol_ord_macro()
        : analyzer_fixture(input) {}
};

TEST_F(lsp_context_document_symbol_ord_macro, macro)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string M = "M", E = "E";
    document_symbol_list_s expected = {
        document_symbol_item_s{
            &M,
            document_symbol_kind::MACRO,
            range{{5,4},{5,4}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &E,
                    document_symbol_kind::EQU,
                    range{{5,4},{5,4}}
                }
            }
        }
    };
    EXPECT_EQ(outline,expected);
}

struct lsp_context_document_symbol_ord_macro_in_macro : analyzer_fixture
{
    const static inline std::string input = R"(
    MACRO
    M1
P1  LR  1,1
    MEND
    
    MACRO
    M2
P2  LR  1,1
    M1
    MEND

    M2
)";
    lsp_context_document_symbol_ord_macro_in_macro()
        : analyzer_fixture(input) {}
};

TEST_F(lsp_context_document_symbol_ord_macro_in_macro, macro_in_macro)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string M1 = "M1", P1 = "P1", M2 = "M2", P2 = "P2";
    document_symbol_list_s expected = {
        document_symbol_item_s{
            &M2,
            document_symbol_kind::MACRO,
            range{{12,4},{12,4}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &P2,
                    document_symbol_kind::MACH,
                    range{{12,4},{12,4}}
                }, document_symbol_item_s{
                    &M1,
                    document_symbol_kind::MACRO,
                    range{{12,4},{12,4}},
                    document_symbol_list_s{
                        document_symbol_item_s{
                            &P1,
                            document_symbol_kind::MACH,
                            range{{12,4},{12,4}}
                        }
                    }
                }
            }
        }
    };
    EXPECT_TRUE(is_permutation_with_permutations(outline,expected));
}

struct lsp_context_document_symbol_ord_inline_macros_and_sects_1 : analyzer_fixture
{
    const static inline std::string input = R"(
    MACRO
    M1
P1  LR 1,1
    MEND

    MACRO
    M2
    M1
P2  LR 1,1
    MEND

C   CSECT
P3  LR 1,1
    M2
)";
    lsp_context_document_symbol_ord_inline_macros_and_sects_1()
        : analyzer_fixture(input) {}
};

TEST_F(lsp_context_document_symbol_ord_inline_macros_and_sects_1, inline_macros_in_sect_1)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string M1 = "M1", P1 = "P1", M2 = "M2", P2 = "P2", P3 = "P3", C = "C";
    document_symbol_list_s expected = {
        document_symbol_item_s{
            &C,
            document_symbol_kind::EXECUTABLE,
            range{{12,0},{12,0}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &P3,
                    document_symbol_kind::MACH,
                    range{{13,4},{13,4}}
                }, document_symbol_item_s{
                    &M2,
                    document_symbol_kind::MACRO,
                    range{{14,4},{14,4}},
                    document_symbol_list_s{
                        document_symbol_item_s{
                            &P2,
                            document_symbol_kind::MACH,
                            range{{14,4},{14,4}}
                        }, document_symbol_item_s{
                            &M1,
                            document_symbol_kind::MACRO,
                            range{{14,4},{14,4}},
                            document_symbol_list_s{
                                document_symbol_item_s{
                                    &P1,
                                    document_symbol_kind::MACH,
                                    range{{14,4},{14,4}}
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    EXPECT_TRUE(is_permutation_with_permutations(outline,expected));
}

struct lsp_context_document_symbol_ord_inline_macros_and_sects_2 : analyzer_fixture
{
    const static inline std::string input = R"(
    MACRO
    M1
P1  LR  1,1
    MEND

    MACRO
    M2
P2  LR  1,1
C2  CSECT
    M1
P3  LR  1,1
    MEND

C1  CSECT
P4  LR 1,1
    M2
)";
    lsp_context_document_symbol_ord_inline_macros_and_sects_2()
        : analyzer_fixture(input) {}
};

TEST_F(lsp_context_document_symbol_ord_inline_macros_and_sects_2, inline_macros_in_sect_2)
{
    document_symbol_list_s outline = a.context().lsp_ctx->document_symbol(opencode_file_name);
    std::string M1 = "M1", M2 = "M2", P1 = "P1", P2 = "P2", P3 = "P3", P4 = "P4", C1 = "C1", C2 = "C2";
    document_symbol_list_s expected = {
        document_symbol_item_s{
            &C1,
            document_symbol_kind::EXECUTABLE,
            range{{14,0},{14,0}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &P4,
                    document_symbol_kind::MACH,
                    range{{15,4},{15,4}}
                }, document_symbol_item_s{
                    &M2,
                    document_symbol_kind::MACRO,
                    range{{16,4},{16,4}},
                    document_symbol_list_s{
                        document_symbol_item_s{
                            &P2,
                            document_symbol_kind::MACH,
                            range{{16,4},{16,4}}
                        }
                    }
                }
            }
        }, document_symbol_item_s{
            &M2,
            document_symbol_kind::MACRO,
            range{{16,4},{16,4}},
            document_symbol_list_s{
                document_symbol_item_s{
                    &C2,
                    document_symbol_kind::EXECUTABLE,
                    range{{16,4},{16,4}},
                    document_symbol_list_s{
                        document_symbol_item_s{
                            &P3,
                            document_symbol_kind::MACH,
                            range{{16,4},{16,4}}
                        }, document_symbol_item_s{
                            &M1,
                            document_symbol_kind::MACRO,
                            range{{16,4},{16,4}},
                            document_symbol_list_s{
                                document_symbol_item_s{
                                    &P1,
                                    document_symbol_kind::MACH,
                                    range{{16,4},{16,4}}
                                }
                            }
                        }
                    }
                }
            }
        }
    };
    write_it(outline,expected);
    EXPECT_TRUE(is_permutation_with_permutations(outline,expected));
}