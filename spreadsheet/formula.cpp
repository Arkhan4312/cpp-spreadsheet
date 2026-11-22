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
            if (const auto* number = std::get_if<double>(&value)) {
                return *number;
            }
            if (const auto* text = std::get_if<std::string>(&value)) {
                if (text->empty()) {
                    return 0.0;
                }
                std::size_t parsed = 0;
                double result = 0.0;
                try {
                    result = std::stod(*text, &parsed);
                } catch (...) {
                    throw FormulaError(FormulaError::Category::Value);
                }
                if (parsed != text->size()) {
                    throw FormulaError(FormulaError::Category::Value);
                }
                return result;
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
    } catch (const std::exception& e) {
        throw FormulaException(e.what());
    } catch (...) {
        throw FormulaException("Invalid formula");
    }
}