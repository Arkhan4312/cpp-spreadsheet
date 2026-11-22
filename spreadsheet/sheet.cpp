#include "sheet.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "cell.h"
#include "common.h"

using namespace std::literals;

void Sheet::EnsureValid(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }
}

void Sheet::ExpandPrintableSize(Position pos) {
    printable_size_.rows = std::max(printable_size_.rows, pos.row + 1);
    printable_size_.cols = std::max(printable_size_.cols, pos.col + 1);
}

void Sheet::RecalculatePrintableSize() {
    int max_row = -1;
    int max_col = -1;
    for (const auto& [pos, cell] : cells_) {
        if (!cell->GetText().empty()) {
            max_row = std::max(max_row, pos.row);
            max_col = std::max(max_col, pos.col);
        }
    }
    if (max_row < 0) {
        printable_size_ = {0, 0};
    } else {
        printable_size_ = {max_row + 1, max_col + 1};
    }
}

void Sheet::SetCell(Position pos, std::string text) {
    EnsureValid(pos);

    const bool existed = cells_.count(pos) > 0;

    Cell& cell = GetOrCreateCell(pos);

    try {
        cell.Set(std::move(text));
    } catch (...) {
        if (!existed) {
            cells_.erase(pos);
        }
        throw;
    }

    if (cell.GetText().empty()) {
        RecalculatePrintableSize();
    } else {
        ExpandPrintableSize(pos);
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    EnsureValid(pos);
    return GetConcreteCell(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
    EnsureValid(pos);
    return GetConcreteCell(pos);
}

void Sheet::ClearCell(Position pos) {
    EnsureValid(pos);
    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        return;
    }
    it->second->Clear();
    if (!it->second->IsReferenced()) {
        cells_.erase(it);
    }

    RecalculatePrintableSize();
}

Size Sheet::GetPrintableSize() const {
    return printable_size_;
}

void Sheet::PrintValues(std::ostream& output) const {
    PrintCells(output, [&output](const CellInterface& cell) {
        const auto value = cell.GetValue();
        std::visit([&output](const auto& v) { output << v; }, value);
    });
}

void Sheet::PrintTexts(std::ostream& output) const {
    PrintCells(output, [&output](const CellInterface& cell) {
        output << cell.GetText();
    });
}

void Sheet::PrintCells(
    std::ostream& output,
    const std::function<void(const CellInterface&)>& print_cell) const {
    for (int row = 0; row < printable_size_.rows; ++row) {
        for (int col = 0; col < printable_size_.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            if (const Cell* cell = GetConcreteCell(Position{row, col})) {
                print_cell(*cell);
            }
        }
        output << '\n';
    }
}

const Cell* Sheet::GetConcreteCell(Position pos) const {
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

Cell* Sheet::GetConcreteCell(Position pos) {
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

Cell& Sheet::GetOrCreateCell(Position pos) {
    auto it = cells_.find(pos);
    if (it != cells_.end()) {
        return *it->second;
    }
    auto cell = std::make_unique<Cell>(*this);
    Cell* raw = cell.get();
    cells_[pos] = std::move(cell);
    return *raw;
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}