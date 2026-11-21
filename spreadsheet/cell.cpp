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
    virtual Value GetValue() const = 0;
    virtual std::string GetText() const = 0;
    virtual std::vector<Position> GetReferencedCells() const {
        return {};
    }
    virtual const FormulaInterface* GetFormula() const {
        return nullptr;
    }
};

class Cell::EmptyImpl final : public Cell::Impl {
public:
    Value GetValue() const override {
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

    Value GetValue() const override {
        if (!text_.empty() && text_.front() == ESCAPE_SIGN) {
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

    // Значение формулы считает Cell::ComputeValue через GetFormula()
    Value GetValue() const override {
        return FormulaError(FormulaError::Category::Value);
    }

    std::string GetText() const override {
        return FORMULA_SIGN + formula_->GetExpression();
    }

    std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }

    const FormulaInterface* GetFormula() const override {
        return formula_.get();
    }

private:
    std::unique_ptr<FormulaInterface> formula_;
};

// CELL

Cell::Cell(Sheet& sheet) : impl_(std::make_unique<EmptyImpl>()), sheet_(sheet) {
}

Cell::~Cell() {
    for (const Position& pos : referenced_cells_) {
        if (Cell* ref = sheet_.GetConcreteCell(pos)) {
            ref->RemoveDependent(this);
        }
    }
}

void Cell::ResetGraphLinks() {
    referenced_cells_.clear();
    dependent_cells_.clear();
}

void Cell::Set(std::string text) {
    if (impl_->GetText() == text) {
        return;
    }
    std::unique_ptr<Impl> new_impl;
    std::vector<Position> new_refs;

    if (text.size() > 1 && text.front() == FORMULA_SIGN) {
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
        if (Cell* ref = sheet_.GetConcreteCell(pos)) {
            ref->RemoveDependent(this);
        }
    }

    for (const Position& pos : new_refs) {
        Cell& ref_cell = sheet_.GetOrCreateCell(pos);
        ref_cell.AddDependent(this);
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
        cached_value_ = ComputeValue();
    }
    return *cached_value_;
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return referenced_cells_;
}

void Cell::AddDependent(Cell* dependent) {
    dependent_cells_.insert(dependent);
}

void Cell::RemoveDependent(Cell* dependent) {
    dependent_cells_.erase(dependent);
}

void Cell::InvalidateCache() {
    cached_value_.reset();
    for (Cell* dep : dependent_cells_) {
        dep->RecursiveInvalidateCache();
    }
}

void Cell::RecursiveInvalidateCache() {
    if (!cached_value_) {
        return;
    }
    cached_value_.reset();
    for (Cell* dep : dependent_cells_) {
        dep->RecursiveInvalidateCache();
    }
}

CellInterface::Value Cell::ComputeValue() const {
    if (const FormulaInterface* formula = impl_->GetFormula()) {
        auto result = formula->Evaluate(sheet_);
        if (auto* d = std::get_if<double>(&result)) {
            return *d;
        }
        return std::get<FormulaError>(result);
    }
    return impl_->GetValue();
}

bool Cell::HasCycle(const std::vector<Position>& new_refs) const {
    std::unordered_set<Position, PositionHasher> visited;

    std::function<bool(Position)> dfs = [&](Position p) -> bool {
        Cell* cell = sheet_.GetConcreteCell(p);
        if (cell == this) {
            return true;
        }
        if (!cell) {
            return false;
        }
        if (visited.count(p)) {
            return false;
        }
        visited.insert(p);

        for (const Position& next : cell->referenced_cells_) {
            if (dfs(next)) {
                return true;
            }
        }
        return false;
    };

    for (const Position& ref : new_refs) {
        if (dfs(ref)) {
            return true;
        }
    }
    return false;
}