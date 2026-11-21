#pragma once

#include <forward_list>
#include <functional>
#include <stdexcept>

#include "FormulaLexer.h"
#include "common.h"

namespace ASTImpl {
class Expr;
}

class ParsingError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class FormulaAST {
public:
    // Резольвер: по позиции возвращает число.
    // Обязан бросить FormulaError, если ячейка содержит ошибку
    // или позиция некорректна. Пустая ячейка - 0.0
    using CellValueResolver = std::function<double(Position)>;

    explicit FormulaAST(std::unique_ptr<ASTImpl::Expr> root_expr,
                        std::forward_list<Position> cells);
    FormulaAST(FormulaAST&&) = default;
    FormulaAST& operator=(FormulaAST&&) = default;
    ~FormulaAST();

    // Бросает FormulaError при ошибке вычисления
    double Execute(const CellValueResolver& resolver) const;
    void Print(std::ostream& out) const;
    void PrintFormula(std::ostream& out) const;
    void PrintCells(std::ostream& out) const;

    const std::forward_list<Position>& GetReferencedCells() const;

private:
    std::unique_ptr<ASTImpl::Expr> root_expr_;
    std::forward_list<Position> cells_;
};

FormulaAST ParseFormulaAST(std::istream& in);
FormulaAST ParseFormulaAST(const std::string& in_str);