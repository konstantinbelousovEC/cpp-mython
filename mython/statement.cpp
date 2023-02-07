#include "statement.h"
#include <sstream>
#include <utility>

using namespace std::literals;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const std::string ADD_METHOD = "__add__"s;
        const std::string INIT_METHOD = "__init__"s;
        const std::string NONE_LITERAL = "None"s;
    }

    ObjectHolder Assignment::Execute(Closure &closure, Context &context) {
        closure[var_] = rv_->Execute(closure, context);
        return closure.at(var_);
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
            : var_(std::move(var)), rv_(std::move(rv)) {}

    VariableValue::VariableValue(const std::string &var_name)
            : names_({var_name}) {}

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
            : names_(std::move(dotted_ids)) {}

    ObjectHolder VariableValue::Execute(Closure &closure, [[maybe_unused]] Context &context) {
        if (closure.count(names_.front()) > 0) {
            ObjectHolder obj_holder = closure.at(names_.front());
            for (size_t i = 1; i < names_.size(); i++) {
                auto class_instance_ptr = obj_holder.TryAs<runtime::ClassInstance>();
                if (class_instance_ptr != nullptr) {
                    obj_holder = class_instance_ptr->Fields().at(names_[i]);
                }
            }
            return obj_holder;
        } else {
            throw std::runtime_error("unknown variable name: "s.append(names_.front()));
        }
    }

    std::unique_ptr<Print> Print::Variable(const std::string &name) {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    Print::Print(std::unique_ptr<Statement> argument) {
        arguments_.push_back(std::move(argument));
    }

    Print::Print(std::vector<std::unique_ptr<Statement>> args)
            : arguments_(std::move(args)) {}

    ObjectHolder Print::Execute(Closure &closure, Context &context) {
        std::ostream &output = context.GetOutputStream();
        for (size_t i = 0; i < arguments_.size(); i++) {
            ObjectHolder obj_holder = arguments_[i]->Execute(closure, context);
            auto obj_ptr = obj_holder.Get();
            if (obj_ptr != nullptr) {
                obj_ptr->Print(output, context);
            } else {
                output << NONE_LITERAL;
            }
            if (i != arguments_.size() - 1) output << ' ';
        }
        output << '\n';
        return {};
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                           std::vector<std::unique_ptr<Statement>> args)
            : object_(std::move(object)), method_(std::move(method)), arguments_(std::move(args)) {}

    ObjectHolder MethodCall::Execute(Closure &closure, Context &context) {
        auto class_instance_ptr = object_->Execute(closure, context).TryAs<runtime::ClassInstance>();
        std::vector<runtime::ObjectHolder> args;
        for (auto &arg: arguments_) {
            args.push_back(arg->Execute(closure, context));
        }
        return class_instance_ptr->Call(method_, args, context);
    }

    ObjectHolder Stringify::Execute(Closure &closure, Context &context) {
        ObjectHolder obj_holder = argument_->Execute(closure, context);
        if (!obj_holder) return ObjectHolder::Own(runtime::String(NONE_LITERAL));
        std::ostringstream output;
        obj_holder->Print(output, context);
        return ObjectHolder::Own(runtime::String(output.str()));
    }

    ObjectHolder Add::Execute(Closure &closure, Context &context) {
        ObjectHolder obj_holder_lhs = lhs_->Execute(closure, context);
        ObjectHolder obj_holder_rhs = rhs_->Execute(closure, context);
        if (obj_holder_lhs.TryAs<runtime::Number>() && obj_holder_rhs.TryAs<runtime::Number>()) {
            return ObjectHolder::Own(runtime::Number(obj_holder_lhs.TryAs<runtime::Number>()->GetValue()
                                                     + obj_holder_rhs.TryAs<runtime::Number>()->GetValue()));
        } else if (obj_holder_lhs.TryAs<runtime::String>() && obj_holder_rhs.TryAs<runtime::String>()) {
            return runtime::ObjectHolder::Own(runtime::String(obj_holder_lhs.TryAs<runtime::String>()->GetValue()
                                                              + obj_holder_rhs.TryAs<runtime::String>()->GetValue()));
        } else if (obj_holder_lhs.TryAs<runtime::ClassInstance>()) {
            return obj_holder_lhs.TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, {obj_holder_rhs}, context);
        } else {
            throw std::runtime_error("Adding was failed");
        }
    }

    ObjectHolder Sub::Execute(Closure &closure, Context &context) {
        auto lhs_ptr = lhs_->Execute(closure, context).TryAs<runtime::Number>();
        auto rhs_ptr = rhs_->Execute(closure, context).TryAs<runtime::Number>();
        if (lhs_ptr && rhs_ptr)
            return runtime::ObjectHolder::Own(runtime::Number(lhs_ptr->GetValue() - rhs_ptr->GetValue()));
        throw std::runtime_error("Subtraction was failed");
    }

    ObjectHolder Mult::Execute(Closure &closure, Context &context) {
        auto lhs_ptr = lhs_->Execute(closure, context).TryAs<runtime::Number>();
        auto rhs_ptr = rhs_->Execute(closure, context).TryAs<runtime::Number>();
        if (lhs_ptr && rhs_ptr)
            return runtime::ObjectHolder::Own(runtime::Number(lhs_ptr->GetValue() * rhs_ptr->GetValue()));
        throw std::runtime_error("Multiplication was failed");
    }

    ObjectHolder Div::Execute(Closure &closure, Context &context) {
        auto lhs_ptr = lhs_->Execute(closure, context).TryAs<runtime::Number>();
        auto rhs_ptr = rhs_->Execute(closure, context).TryAs<runtime::Number>();
        if (lhs_ptr && rhs_ptr)
            return runtime::ObjectHolder::Own(runtime::Number(lhs_ptr->GetValue() / rhs_ptr->GetValue()));
        throw std::runtime_error("Division was failed");
    }


    void Compound::AddStatement(std::unique_ptr<Statement> stmt) {
        instructions_.emplace_back(std::move(stmt));
    }

    ObjectHolder Compound::Execute(Closure &closure, Context &context) {
        for (auto &instruction: instructions_) {
            instruction->Execute(closure, context);
        }
        return {};
    }

    Return::Return(std::unique_ptr<Statement> statement)
            : statement_(std::move(statement)) {}

    ObjectHolder Return::Execute(Closure &closure, Context &context) {
        throw statement_->Execute(closure, context);
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls)
            : cls_(std::move(cls)) {}

    ObjectHolder ClassDefinition::Execute(Closure &closure, [[maybe_unused]] Context &context) {
        closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
        return {};
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
            : object_(std::move(object)), field_name_(std::move(field_name)), rv_(std::move(rv)) {}

    ObjectHolder FieldAssignment::Execute(Closure &closure, Context &context) {
        ObjectHolder obj_holder = object_.Execute(closure, context);
        auto class_instance_ptr = obj_holder.TryAs<runtime::ClassInstance>();
        if (class_instance_ptr != nullptr) {
            class_instance_ptr->Fields()[field_name_] = rv_->Execute(closure, context);
        }
        return class_instance_ptr->Fields().at(field_name_);
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
                   std::unique_ptr<Statement> else_body)
            : condition_(std::move(condition)), if_body_(std::move(if_body)), else_body_(std::move(else_body)) {}

    ObjectHolder IfElse::Execute(Closure &closure, Context &context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        } else if (else_body_) {
            return else_body_->Execute(closure, context);
        }
        return {};
    }

    ObjectHolder Or::Execute(Closure &closure, Context &context) {
        if (IsTrue(lhs_->Execute(closure, context))) return ObjectHolder::Own(runtime::Bool(true));
        return ObjectHolder::Own(runtime::Bool(IsTrue(rhs_->Execute(closure, context))));
    }

    ObjectHolder And::Execute(Closure &closure, Context &context) {
        if (IsTrue(lhs_->Execute(closure, context)))
            return ObjectHolder::Own(runtime::Bool(IsTrue(rhs_->Execute(closure, context))));;
        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder Not::Execute(Closure &closure, Context &context) {
        return ObjectHolder::Own(runtime::Bool(!IsTrue(argument_->Execute(closure, context))));
    }

    Comparison::Comparison(Comparator comparator, std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
            : BinaryOperation(std::move(lhs), std::move(rhs)), comparator_(std::move(comparator)) {}

    ObjectHolder Comparison::Execute(Closure &closure, Context &context) {
        return ObjectHolder::Own(
                runtime::Bool(comparator_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context)));
    }

    NewInstance::NewInstance(const runtime::Class &class_, std::vector<std::unique_ptr<Statement>> args)
            : class_instance_(class_), arguments_(std::move(args)) {}

    NewInstance::NewInstance(const runtime::Class &class_)
            : class_instance_(class_) {}

    ObjectHolder NewInstance::Execute(Closure &closure, Context &context) {
        if (class_instance_.HasMethod(INIT_METHOD, arguments_.size())) {
            std::vector<runtime::ObjectHolder> args;
            for (auto &arg: arguments_) {
                args.push_back(arg->Execute(closure, context));
            }
            class_instance_.Call(INIT_METHOD, args, context);
        }
        return ObjectHolder::Share(class_instance_);
    }

    MethodBody::MethodBody(std::unique_ptr<Statement> &&body)
            : body_(std::move(body)) {}

    ObjectHolder MethodBody::Execute(Closure &closure, Context &context) {
        try {
            body_->Execute(closure, context);
            return {};
        } catch (runtime::ObjectHolder &obj_holder) {
            return obj_holder;
        }
    }

}