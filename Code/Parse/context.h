#ifndef Athena_Parser_context_h
#define Athena_Parser_context_h

#include "../General/compiler.h"
#include "../General/maybe.h"
#include "../General/hash.h"
#include "../General/map.h"
#include <string>
#include <cassert>

namespace athena {
namespace ast {

struct Qualified {
    Qualified* qualifier = nullptr;
    std::string name{""};
};

enum class Assoc : U16 {
    Left,
    Right
};

struct OpProperties {
    U16 precedence;
    Assoc associativity;
};

struct CompileContext {
    CompileContext(const CompileSettings& settings) : settings(settings) {}

    const CompileSettings& settings;

    /**
     * Adds an operator to the list with unknown precedence and associativity.
     * These properties can be updated later.
     */
    void addOp(Id op) {
        ops.add(op, OpProperties{9, Assoc::Left}, false);
    }

    /**
     * Adds an operator to the list with the provided properties,
     * or updates the properties of an existing operator.
     */
    void addOp(Id op, U32 prec, Assoc assoc) {
        ops.add(op, OpProperties{(U16)prec, assoc}, true);
    }

    /**
     * Returns the properties of the provided operator.
     * The operator must exist.
     */
    OpProperties findOp(Id op) {
        auto res = ops.get(op);
        if(res)
            return *res.force();
        else
            return {9, Assoc::Left};
    }

    OpProperties* tryFindOp(Id op) {
        return ops.get(op).get();
    }

    Qualified& find(Id id) {
        auto res = names.get(id);
        assert(res == true);
        return *res.force();
    }

    Id addUnqualifiedName(const std::string& str) {
        return addUnqualifiedName(str.c_str(), str.length());
    }

    Id addUnqualifiedName(const char* chars, Size count) {
        Qualified q;
        q.name = std::string(chars, count);
        return addName(&q);
    }

    Id addName(Qualified* q) {
        Hasher h;

        auto qu = q;
        while(qu) {
            h.addData(qu->name.c_str(), (U32)(qu->name.length()));
            qu = qu->qualifier;
        }

        return addName(Id((U32)h), q);
    }

    Id addName(Id id, Qualified* q) {
        names.add(id, *q, false);
        return id;
    }

    /**
     * Allocates memory from the current parsing context.
     */
    void* alloc(Size size) {
        //TODO: Fix this.
        return malloc(size);
    }

    /**
     * Allocates memory from the current parsing context
     * and constructs an object with the provided parameters.
     */
    template<class T, class... P>
    T* build(P&&... p) {
        auto obj = (T*)alloc(sizeof(T));
        new (obj) T(p...);
        return obj;
    }

private:
    Tritium::Map<Id, Qualified> names{256};
    Tritium::Map<Id, OpProperties> ops{64};
};

}}

#endif // Athena_Parser_context_h