#include "Scope.h"
#include <stdexcept>

void Scope::pushScope() {
    localVars.push_back(std::map<std::string, Value>());
}

void Scope::popScope() {
    if (!localVars.empty()) {
        localVars.pop_back();
    }
}

void Scope::setLocal(const std::string& name, const Value& val) {
    if (!localVars.empty()) {
        localVars.back()[name] = val;
    } else {
        globalVars[name] = val;
    }
}

void Scope::set(const std::string& name, const Value& val) {
    if (!localVars.empty() && localVars.back().count(name)) {
        localVars.back()[name] = val;
    } else {
        globalVars[name] = val;
    }
}

Value Scope::get(const std::string& name) {
    if (!localVars.empty() && localVars.back().count(name)) {
        return localVars.back()[name];
    }
    if (globalVars.count(name)) {
        return globalVars[name];
    }
    throw std::runtime_error("NameError: name '" + name + "' is not defined");
}

void Scope::registerFunc(const std::string& name, const FunctionDef& func) {
    functions[name] = func;
}

FunctionDef Scope::getFunc(const std::string& name) {
    if (functions.count(name)) {
        return functions[name];
    }
    throw std::runtime_error("NameError: function '" + name + "' is not defined");
}

bool Scope::hasFunc(const std::string& name) {
    return functions.count(name) > 0;
}
