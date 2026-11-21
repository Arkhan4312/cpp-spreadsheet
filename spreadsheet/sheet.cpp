#include "sheet.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "cell.h"
#include "common.h"

using namespace std::literals;

Sheet::~Sheet() {
    for (auto& [pos, cell] : cells_) {
        cell->ResetGraphLinks();
    }
}

void Sheet::EnsureValid(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }
}

void Sheet::SetCell(Position pos, std::string text) {
    EnsureValid(pos);

    Cell& cell = GetOrCreateCell(pos);
    cell.Set(std::move(text));
    UpdatePrintableSize();
}

const CellInterface* Sheet::GetCell(Position pos) const {
    EnsureValid(pos);
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

CellInterface* Sheet::GetCell(Position pos) {
    EnsureValid(pos);
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

void Sheet::ClearCell(Position pos) {
    EnsureValid(pos);
    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        return;
    }
    it->second->Clear();
    cells_.erase(it);
    UpdatePrintableSize();
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
    const std::function<void(const CellInterface&)>& printCell) const {
    for (int row = 0; row < printable_size_.rows; ++row) {
        for (int col = 0; col < printable_size_.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            auto it = cells_.find(Position{row, col});
            if (it != cells_.end()) {
                printCell(*it->second);
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

void Sheet::UpdatePrintableSize() {
    if (cells_.empty()) {
        printable_size_ = {0, 0};
        return;
    }
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

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}