#include "formula.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

#include "FormulaAST.h"

using namespace std::literals;

namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression)
        : ast_(ParseFormulaAST(expression)) {
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        auto resolver = [&sheet](Position pos) -> double {
            if (!pos.IsValid()) {
                throw FormulaError(FormulaError::Category::Ref);
            }
            const CellInterface* cell = sheet.GetCell(pos);
            if (!cell) {
                return 0.0;
            }
            const auto value = cell->GetValue();
            if (const auto* d = std::get_if<double>(&value)) {
                return *d;
            }
            if (const auto* s = std::get_if<std::string>(&value)) {
                if (s->empty()) {
                    return 0.0;
                }
                std::size_t idx = 0;
                try {
                    double result = std::stod(*s, &idx);
                    if (idx != s->size()) {
                        throw FormulaError(FormulaError::Category::Value);
                    }
                    return result;
                } catch (const FormulaError&) {
                    throw;
                } catch (...) {
                    throw FormulaError(FormulaError::Category::Value);
                }
            }
            throw std::get<FormulaError>(value);
        };

        try {
            return ast_.Execute(resolver);
        } catch (const FormulaError& error) {
            return error;
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        // Позиции из AST могут повторяться; убираем дубликаты
        // cells_ уже отсортирован в конструкторе FormulaAST
        std::vector<Position> result;
        for (const Position& pos : ast_.GetReferencedCells()) {
            if (result.empty() || !(result.back() == pos)) {
                result.push_back(pos);
            }
        }
        return result;
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try {
        return std::make_unique<Formula>(std::move(expression));
    } catch (const FormulaException&) {
        throw;
    } catch (const ParsingError& e) {
        throw FormulaException(e.what());
    } catch (const std::exception& e) {
        // ANTLR
        throw FormulaException(e.what());
    } catch (...) {
        throw FormulaException("Invalid formula");
    }
}