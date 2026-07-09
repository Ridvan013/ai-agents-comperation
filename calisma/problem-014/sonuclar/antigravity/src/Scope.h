#ifndef SCOPE_H
#define SCOPE_H

#include "Value.h"
#include "Python3Parser.h"
#include <string>
#include <map>
#include <vector>

struct FunctionDef {
    Python3Parser::FuncdefContext* ctx;
    std::vector<std::string> params;
    std::vector<Value> defaultValues;
    int numParamsWithoutDefault;
};

class Scope {
public:
    std::map<std::string, Value> globalVars;
    std::vector<std::map<std::string, Value>> localVars;
    std::map<std::string, FunctionDef> functions;

    void pushScope();
    void popScope();
    
    // Set a variable directly in the current local scope (used for parameters)
    void setLocal(const std::string& name, const Value& val);
    
    // Set a variable according to Python scoping rules
    void set(const std::string& name, const Value& val);
    
    // Get a variable's value
    Value get(const std::string& name);

    // Register a function
    void registerFunc(const std::string& name, const FunctionDef& func);
    
    // Get a function
    FunctionDef getFunc(const std::string& name);
    bool hasFunc(const std::string& name);
};

#endif // SCOPE_H
