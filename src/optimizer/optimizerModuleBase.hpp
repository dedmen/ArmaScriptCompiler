#pragma once
#include "../compiledCode.hpp"
#include <functional>

#include <parser/sqf/sqf_parser.hpp>
using astnode = sqf::parser::sqf::bison::astnode;

template<class Sig>
class Signal;

template<class ReturnType, class... Args>
class Signal<ReturnType(Args...)> {
private:
    typedef std::function<ReturnType(Args...)> Slot;

public:
    void connect(Slot slot) {
        slots.push_back(slot);
    }

    std::vector<ReturnType> operator() (Args&&... args) const {
        return emit(std::forward<Args>(args)...);
    }
    std::vector<ReturnType> emit(Args&&... args) const {
        std::vector<ReturnType> returnData;
        if (slots.empty())
            return;
        for (auto& slot : slots) {
            returnData.push_back(slot(std::forward<Args>(args)...));
        }
        return returnData;
    }
    bool anyOf(Args&&... args) const {
        for (auto& slot : slots) {
            if (slot(std::forward<Args>(args)...)) return true;
        }
        return false;
    }
    void removeAllSlots() {
        slots.clear();
    }
private:
    std::vector<Slot> slots{};
};

class OptimizerModuleBase {
public:
    class Node {
    public:
        Node() {}
        //Node(Node&& mv) noexcept {
        //    type = mv.type;
        //    file = std::move(mv.file);
        //    line = mv.line;
        //    offset = mv.offset;
        //    value = std::move(mv.value);
        //    children = std::move(mv.children);
        //}

        InstructionType type;
      
        std::string file;
        size_t line;
        size_t offset;

        ScriptConstant value;
        std::vector<Node> children;

        void dumpTree(std::ostream& output, size_t indent) const;
        bool buildConstState();//#TODO remove
        bool areChildrenConstant() const;



        std::vector<Node*> bottomUpFlatten();


        bool constant = false;
    };

    static Node nodeFromAST(const astnode& input); //#TODO move to optimizer


    Signal<bool(const Node&, const bool&)> canBinaryBeConst;//#TODO remove
    Signal<bool(const Node&, const bool&)> canUnaryBeConst;
    Signal<bool(const Node&, const bool&)> canNularBeConst;


};
