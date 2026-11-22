#pragma once

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "common.h"
#include "formula.h"

class Sheet;  // fwd: ячейка создается только из Sheet и хранит ссылку на него

class Cell : public CellInterface {
public:
    explicit Cell(Sheet& sheet);
    ~Cell() override;

    // Может бросить FormulaException или CircularDependencyException.
    // При исключении состояние ячейки не меняется.
    // Разбирает формулу один раз и сам управляет рёбрами графа.
    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    // true, если на эту ячейку кто-то ссылается
    bool IsReferenced() const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    // Управление входящими рёбрами (кого инвалидировать при изменении)
    void AddDependent(Cell* dependent);
    void RemoveDependent(Cell* dependent);

    // Сброс кэша из Set/Clear: обходит зависимых безусловно
    void InvalidateCache();
    // Сброс кэша по рекурсии: ранний выход по пустому кэшу
    void RecursiveInvalidateCache();

    // Проверка цикла: обход от каждой позиции из new_refs по графу
    // зависимостей. Если дошли до ячейки this - есть цикл.
    bool HasCycle(const std::vector<Position>& new_refs) const;

    static bool IsFormula(const std::string& text);
    static bool IsEscaped(const std::string& text);

    std::unique_ptr<Impl> impl_;
    Sheet& sheet_;
    std::vector<Position> referenced_cells_;
    std::unordered_set<Cell*> dependent_cells_;
    mutable std::optional<Value> cached_value_;
};