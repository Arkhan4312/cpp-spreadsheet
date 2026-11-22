#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>

#include "cell.h"
#include "common.h"

struct PositionHasher {
    std::size_t operator()(const Position& pos) const {
        return std::hash<int>{}(pos.row) * 31 + std::hash<int>{}(pos.col);
    }
};

class Sheet : public SheetInterface {
public:
    ~Sheet() override = default;

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    const Cell* GetConcreteCell(Position pos) const;
    Cell* GetConcreteCell(Position pos);

    // Создаёт пустую ячейку по позиции, если её нет, и возвращает её.
    // Используется Cell::Set при установке ссылок
    Cell& GetOrCreateCell(Position pos);

private:
    void EnsureValid(Position pos) const;
    void ExpandPrintableSize(Position pos);
    void RecalculatePrintableSize();

    void PrintCells(
        std::ostream& output,
        const std::function<void(const CellInterface&)>& print_cell) const;

    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher> cells_;
    Size printable_size_;
};