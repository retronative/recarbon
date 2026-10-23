// From SpeedBreaker v0.1.8's tests/switch_tables_test.cpp (GPL-3.0-or-later).
// ReCarbon changes: Carbon's config. Checked: every jump table in
// config/switch_tables.toml and config/switch_tables_inline.toml that lies in a
// leaf function config/recarbon.toml bounds keeps its labels and its default
// (0 = a conditional return) inside that function: a borrowed default lies
// before the function's start (the fault SpeedBreaker found in Most Wanted's
// v0.1.2 tables, GitHub issue #8, and survey/fix_switch_tables.py now drops).
// Upstream's three Most Wanted dispatches are left out: their addresses mean
// nothing in Carbon's image. Its v0.1.2 entries are kept only as a planted
// input the check must reject (synthetic data, not Carbon's tables).
// From the repo root:
//   clang++ -std=c++20 -O2 tests/switch_tables_test.cpp -o build/switch_tables_test
//   build/switch_tables_test config/switch_tables.toml config/switch_tables_inline.toml config/recarbon.toml
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Table
{
    uint32_t base = 0;
    uint32_t r = 99;
    bool hasDefault = false;
    uint32_t defaultLabel = 0;
    std::vector<uint32_t> labels;
};

struct Function
{
    uint32_t address = 0;
    uint32_t size = 0;
    bool Contains(uint32_t a) const { return a >= address && a < address + size; }
};

static std::string ReadFile(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

// The subset of TOML the generators write: [[switch]] blocks of
// `base = 0x..`, `r = N`, `default = 0x..` and a `labels = [ 0x.., ]` list.
static std::vector<Table> ParseTables(const std::string& text)
{
    std::vector<Table> tables;
    std::istringstream in(text);
    std::string line;
    bool inLabels = false;
    std::smatch m;
    static const std::regex kKey(R"(^\s*(base|r|default)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*$)");
    static const std::regex kLabel(R"(^\s*(0x[0-9A-Fa-f]+)\s*,?\s*$)");
    while (std::getline(in, line))
    {
        if (line.rfind("[[switch]]", 0) == 0)
        {
            tables.emplace_back();
            inLabels = false;
            continue;
        }
        if (tables.empty() || line.empty() || line[0] == '#')
            continue;
        Table& t = tables.back();
        if (inLabels)
        {
            if (line.find(']') != std::string::npos)
                inLabels = false;
            else if (std::regex_match(line, m, kLabel))
                t.labels.push_back(uint32_t(std::stoul(m[1].str(), nullptr, 16)));
            continue;
        }
        if (line.rfind("labels", 0) == 0)
        {
            inLabels = true;
            continue;
        }
        if (std::regex_match(line, m, kKey))
        {
            uint32_t v = uint32_t(std::stoul(m[2].str(), nullptr, 0));
            if (m[1] == "base")
                t.base = v;
            else if (m[1] == "r")
                t.r = v;
            else
            {
                t.hasDefault = true;
                t.defaultLabel = v;
            }
        }
    }
    return tables;
}

static std::vector<Function> ParseFunctions(const std::string& text)
{
    std::vector<Function> functions;
    static const std::regex kFunction(R"(\{\s*address\s*=\s*(0x[0-9A-Fa-f]+)\s*,\s*size\s*=\s*(0x[0-9A-Fa-f]+)\s*\})");
    for (std::sregex_iterator it(text.begin(), text.end(), kFunction), end; it != end; ++it)
        functions.push_back({ uint32_t(std::stoul((*it)[1].str(), nullptr, 16)), uint32_t(std::stoul((*it)[2].str(), nullptr, 16)) });
    return functions;
}

// Tables inside a bounded leaf function with a label or a default outside
// it, as "base: what" lines.
static std::vector<std::string> Escapes(const std::vector<Table>& tables, const std::vector<Function>& functions)
{
    std::vector<std::string> out;
    char buf[160];
    for (const Table& t : tables)
        for (const Function& f : functions)
        {
            if (!f.Contains(t.base))
                continue;
            if (t.hasDefault && t.defaultLabel != 0 && !f.Contains(t.defaultLabel))
            {
                snprintf(buf, sizeof(buf), "%08X: default %08X outside %08X+%X", t.base, t.defaultLabel, f.address, f.size);
                out.push_back(buf);
            }
            for (uint32_t label : t.labels)
                if (!f.Contains(label))
                {
                    snprintf(buf, sizeof(buf), "%08X: label %08X outside %08X+%X", t.base, label, f.address, f.size);
                    out.push_back(buf);
                }
        }
    return out;
}

// The table XenonRecomp uses at a bctr: the last [[switch]] keyed at or
// before it within the dispatch (XenonAnalyse keys the lis 20 bytes before
// the bctr, find_missing_tables.py the rlwinm 12 bytes before).
// SpeedBreaker v0.1.2's entries for three Most Wanted dispatches (XenonAnalyse's
// output) and their function bounds: a planted input.
static const char kReleased[] = R"(
[[switch]]
base = 0x822C7720
r = 11
default = 0x822C76E4
labels = [
    0x822C7760,
    0x822C7774,
]

[[switch]]
base = 0x822C89F8
r = 10
default = 0x822C89D0
labels = [
    0x822C8A2C,
    0x822C8A34,
]

[[switch]]
base = 0x822E7CE0
r = 3
default = 0x822E7BC4
labels = [
    0x822E7D14,
]
)";
static const char kReleasedFunctions[] = R"(
    { address = 0x822C7720, size = 0x68 },
    { address = 0x822C89F8, size = 0x44 },
    { address = 0x822E7CE0, size = 0x40 },
)";

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: switch_tables_test <switch tables .toml>... <config/recarbon.toml>\n");
        return 2;
    }
    std::vector<Table> tables;
    for (int i = 1; i < argc - 1; i++)
    {
        std::vector<Table> file = ParseTables(ReadFile(argv[i]));
        printf("switch_tables_test: %zu tables in %s\n", file.size(), argv[i]);
        CHECK(!file.empty(), "no tables in %s: the file wasn't read", argv[i]);
        tables.insert(tables.end(), file.begin(), file.end());
    }
    std::vector<Function> functions = ParseFunctions(ReadFile(argv[argc - 1]));
    printf("switch_tables_test: %zu tables, %zu bounded leaf functions\n", tables.size(), functions.size());
    CHECK(tables.size() > 500, "%zu tables: the files weren't read", tables.size());
    CHECK(functions.size() > 100, "%zu functions: the file wasn't read", functions.size());
    for (const Table& t : tables)
        CHECK(t.base != 0 && t.r < 32 && !t.labels.empty(), "an entry at %08X didn't parse (r %u, %zu labels)", t.base, t.r, t.labels.size());

    for (const std::string& e : Escapes(tables, functions))
        CHECK(false, "%s", e.c_str());

    // Planted: the released entries must fail the check.
    std::vector<Table> released = ParseTables(kReleased);
    std::vector<Function> releasedFunctions = ParseFunctions(kReleasedFunctions);
    std::vector<std::string> escapes = Escapes(released, releasedFunctions);
    CHECK(escapes.size() == 3, "the v0.1.2 entries escape their functions %zu times, expected 3 (one default each)", escapes.size());

    if (g_failures)
    {
        printf("switch_tables_test: %d FAILED\n", g_failures);
        return 1;
    }
    printf("switch_tables_test: all passed\n");
    return 0;
}
