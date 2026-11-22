#include "cell.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

#include "sheet.h"

// IMPL

class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual Value GetValue(const SheetInterface& sheet) const = 0;
    virtual std::string GetText() const = 0;
    virtual std::vector<Position> GetReferencedCells() const {
        return {};
    }
};

class Cell::EmptyImpl final : public Cell::Impl {
public:
    Value GetValue(const SheetInterface& /*sheet*/) const override {
        return std::string{};
    }
    std::string GetText() const override {
        return {};
    }
};

class Cell::TextImpl final : public Cell::Impl {
public:
    explicit TextImpl(std::string text) : text_(std::move(text)) {
    }

    Value GetValue(const SheetInterface& /*sheet*/) const override {
        if (IsEscaped(text_)) {
            return text_.substr(1);
        }
        return text_;
    }

    std::string GetText() const override {
        return text_;
    }

private:
    std::string text_;
};

class Cell::FormulaImpl final : public Cell::Impl {
public:
    explicit FormulaImpl(std::unique_ptr<FormulaInterface> formula)
        : formula_(std::move(formula)) {
    }

    Value GetValue(const SheetInterface& sheet) const override {
        auto result = formula_->Evaluate(sheet);
        if (auto* number = std::get_if<double>(&result)) {
            return *number;
        }
        return std::get<FormulaError>(result);
    }

    std::string GetText() const override {
        return FORMULA_SIGN + formula_->GetExpression();
    }

    std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }

private:
    std::unique_ptr<FormulaInterface> formula_;
};

// CELL

Cell::Cell(Sheet& sheet) : impl_(std::make_unique<EmptyImpl>()), sheet_(sheet) {
}

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    if (impl_->GetText() == text) {
        return;
    }
    std::unique_ptr<Impl> new_impl;
    std::vector<Position> new_refs;

    if (IsFormula(text)) {
        auto formula = ParseFormula(text.substr(1));
        new_refs = formula->GetReferencedCells();

        if (HasCycle(new_refs)) {
            throw CircularDependencyException("Circular dependency detected");
        }

        new_impl = std::make_unique<FormulaImpl>(std::move(formula));
    } else if (text.empty()) {
        new_impl = std::make_unique<EmptyImpl>();
    } else {
        new_impl = std::make_unique<TextImpl>(std::move(text));
    }

    for (const Position& pos : referenced_cells_) {
        if (Cell* referenced = sheet_.GetConcreteCell(pos)) {
            referenced->RemoveDependent(this);
        }
    }

    for (const Position& pos : new_refs) {
        Cell& referenced = sheet_.GetOrCreateCell(pos);
        referenced.AddDependent(this);
    }

    referenced_cells_ = std::move(new_refs);
    impl_ = std::move(new_impl);

    InvalidateCache();
}

void Cell::Clear() {
    Set("");
}

CellInterface::Value Cell::GetValue() const {
    if (!cached_value_) {
        cached_value_ = impl_->GetValue(sheet_);
    }
    return *cached_value_;
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return referenced_cells_;
}

bool Cell::IsReferenced() const {
    return !dependent_cells_.empty();
}

void Cell::AddDependent(Cell* dependent) {
    dependent_cells_.insert(dependent);
}

void Cell::RemoveDependent(Cell* dependent) {
    dependent_cells_.erase(dependent);
}

void Cell::InvalidateCache() {
    cached_value_.reset();
    for (Cell* dependent : dependent_cells_) {
        dependent->RecursiveInvalidateCache();
    }
}

void Cell::RecursiveInvalidateCache() {
    if (!cached_value_) {
        return;
    }
    cached_value_.reset();
    for (Cell* dependent : dependent_cells_) {
        dependent->RecursiveInvalidateCache();
    }
}

bool Cell::HasCycle(const std::vector<Position>& new_refs) const {
    std::unordered_set<Position, PositionHasher> visited;

    std::function<bool(Position)> dfs = [&](Position pos) -> bool {
        Cell* cell = sheet_.GetConcreteCell(pos);
        if (cell == this) {
            return true;
        }
        if (!cell) {
            return false;
        }
        if (visited.count(pos)) {
            return false;
        }
        visited.insert(pos);

        for (const Position& next : cell->referenced_cells_) {
            if (dfs(next)) {
                return true;
            }
        }
        return false;
    };

    for (const Position& referenced : new_refs) {
        if (dfs(referenced)) {
            return true;
        }
    }
    return false;
}

bool Cell::IsFormula(const std::string& text) {
    return text.size() > 1 && text.front() == FORMULA_SIGN;
}

bool Cell::IsEscaped(const std::string& text) {
    return !text.empty() && text.front() == ESCAPE_SIGN;
}
