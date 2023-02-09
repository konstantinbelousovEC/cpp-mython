#include "runtime.h"
#include <cassert>
#include <optional>
#include <sstream>
#include <algorithm>

using namespace std::literals;

namespace runtime {

    namespace {
        const std::string EQUAL_METHOD = "__eq__"s;
        const std::string LESS_METHOD = "__lt__"s;
        const std::string STR_METHOD = "__str__"s;
        const std::string SELF = "self"s;
        const std::string TRUE = "True"s;
        const std::string FALSE = "False"s;
        const std::string NONE_LITERAL = "None"s;
    }

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
            : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object &object) {
        return ObjectHolder(std::shared_ptr<Object>(&object, []([[maybe_unused]] auto *p) {  /*do nothing*/  }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object &ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object *ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object *ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder &object) {
        Object *ptr = nullptr;
        if ((ptr = object.TryAs<Bool>())) {
            return static_cast<Bool*>(ptr)->GetValue();
        } else if ((ptr = object.TryAs<Number>())) {
            return static_cast<Number*>(ptr)->GetValue();
        } else if ((ptr = object.TryAs<String>())) {
            return !static_cast<String*>(ptr)->GetValue().empty();
        }
        return false;
    }

    void ClassInstance::Print(std::ostream &os, Context &context) {
        if (HasMethod(STR_METHOD, 0)) {
            Call(STR_METHOD, {}, context).Get()->Print(os, context);
        } else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string &method, size_t argument_count) const {
        auto method_ptr = cls_.GetMethod(method);
        if (method_ptr == nullptr) {
            return false;
        } else if (method_ptr->formal_params.size() != argument_count) {
            return false;
        } else {
            return true;
        }
    }

    Closure &ClassInstance::Fields() {
        return fields_;
    }

    const Closure &ClassInstance::Fields() const {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class &cls)
            : cls_(cls) {}

    ObjectHolder ClassInstance::Call(const std::string &method,
                                     const std::vector<ObjectHolder> &actual_args,
                                     Context &context) {
        if (!HasMethod(method, actual_args.size())) throw std::runtime_error("no method to call"s);
        Closure fields;
        fields[SELF] = ObjectHolder::Share(*this);
        auto method_ptr = cls_.GetMethod(method);
        for (size_t i = 0; i < actual_args.size(); ++i) {
            fields[method_ptr->formal_params[i]] = actual_args[i];
        }
        return method_ptr->body->Execute(fields, context);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class *parent)
            : name_(std::move(name)), methods_(std::move(methods)), parent_(parent) {}

    const Method *Class::GetMethod(const std::string &name) const {
        auto method_iter = std::find_if(methods_.begin(), methods_.end(), [&name](const Method& method) {
            return method.name == name;
        });
        if (method_iter != methods_.end()) {
            return &(*method_iter);
        } else {
            if (parent_) return parent_->GetMethod(name);
        }
        return nullptr;
    }

    [[nodiscard]] const std::string &Class::GetName() const {
        return name_;
    }

    void Class::Print(std::ostream &os, [[maybe_unused]] Context &context) {
        os << "Class "s << name_;
    }

    void Bool::Print(std::ostream &os, [[maybe_unused]] Context &context) {
        os << (GetValue() ? TRUE : FALSE);
    }

    bool Equal(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
        if (!lhs && !rhs) return true;

        if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
            return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
        } else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
            return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
        } else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
            return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
        } else if (lhs.TryAs<ClassInstance>() && rhs.TryAs<ClassInstance>()) {
            if (lhs.TryAs<ClassInstance>()->HasMethod(EQUAL_METHOD, 1)) {
                return IsTrue(lhs.TryAs<ClassInstance>()->Call(EQUAL_METHOD, {rhs}, context));
            }
        }
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool Less(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
        if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
            return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
        } else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
            return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
        } else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
            return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
        } else if (lhs.TryAs<ClassInstance>() && rhs.TryAs<ClassInstance>()) {
            if (lhs.TryAs<ClassInstance>()->HasMethod(LESS_METHOD, 1)) {
                return IsTrue(lhs.TryAs<ClassInstance>()->Call(LESS_METHOD, {rhs}, context));
            }
        }
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool NotEqual(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
        return !Less(lhs, rhs, context);
    }

}