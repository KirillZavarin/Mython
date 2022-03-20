#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace


    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) : name_(var), rv_(std::move(rv)) {
    }

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        return closure[name_] = rv_->Execute(closure, context);
    }

    VariableValue::VariableValue(const std::string& var_name) {
        dotted_ids_.push_back(var_name);
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids) : dotted_ids_(dotted_ids) {
    }

    ObjectHolder VariableValue::Execute(Closure& closure, Context& /*context*/) {
        ObjectHolder result;
        try {
            result = closure.at(dotted_ids_[0]);
            for (size_t i = 1; i < dotted_ids_.size(); i++) {
                result = result.TryAs<runtime::ClassInstance>()->Fields().at(dotted_ids_[i]);
            }
        }
        catch (...) {
            throw std::runtime_error("Not found"s);
        }
        return result;
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args) : args_(std::move(args)) {
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return unique_ptr<Print>(new Print(std::move(unique_ptr<Statement>(new StringConst(name)))));
    }


    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        for (size_t i = 0; i < args_.size(); i++) {
            auto obj = args_[i]->Execute(closure, context);
            if (obj.TryAs<runtime::String>() != 0) {
                if (closure.count(args_[i]->Execute(closure, context).TryAs<runtime::String>()->GetValue()) != 0) {
                    closure[args_[i]->Execute(closure, context).TryAs<runtime::String>()->GetValue()]->Print(context.GetOutputStream(), context);
                }
                else {
                    args_[i]->Execute(closure, context).TryAs<runtime::String>()->Print(context.GetOutputStream(), context);
                }
                if (i != args_.size() - 1) {
                    context.GetOutputStream() << ' ';
                }
                continue;
            }   
            if (obj) {
                obj->Print(context.GetOutputStream(), context);
            }
            else {
                context.GetOutputStream() << "None"s;
            }
            if (i != args_.size() - 1) {
                context.GetOutputStream() << ' ';
            }
        }
        
        context.GetOutputStream() << '\n';
        return {};
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method, std::vector<std::unique_ptr<Statement>> args) 
        : object_(std::move(object))
        , method_(method)
        , args_(std::move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        if (!object_) {
            return ObjectHolder::None();
        }
        std::vector<runtime::ObjectHolder> actual_args;
        for (size_t i = 0; i < args_.size(); i++) {
            actual_args.push_back(args_[i]->Execute(closure, context));
        }
        return object_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(method_, actual_args, context);
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        if (!argument_->Execute(closure, context)) {
            return ObjectHolder::Own(runtime::String("None"s));
        }
        auto class_ptr = argument_->Execute(closure, context).TryAs<runtime::ClassInstance>();
        if (class_ptr != nullptr) {
            if (class_ptr->HasMethod("__str__"s, 0u)) {
                std::stringstream ss;
                class_ptr->Call("__str__"s, {}, context)->Print(ss, context);
                return ObjectHolder::Own(runtime::String(ss.str()));
            }
            else {
                std::stringstream ss;
                ss << class_ptr;
                return ObjectHolder::Own(runtime::String(ss.str()));
            }
        }
        if (argument_->Execute(closure, context).TryAs<runtime::String>() != nullptr) {
            return ObjectHolder::Own(runtime::String(argument_->Execute(closure, context).TryAs<runtime::String>()->GetValue()));
        }
        if (argument_->Execute(closure, context).TryAs<runtime::Bool>() != nullptr) {
            if (argument_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()) {
                return ObjectHolder::Own(runtime::String("True"s));
            }
            if (!argument_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()) {
                return ObjectHolder::Own(runtime::String("False"s));
            }
        }
        if (argument_->Execute(closure, context).TryAs<runtime::Number>() != nullptr) {
            return ObjectHolder::Own(runtime::String(std::to_string(argument_->Execute(closure, context).TryAs<runtime::Number>()->GetValue())));
        }
        throw std::runtime_error("There is no string representation"s);
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>() != nullptr) {
            if (lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1u)) {
                return lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, { rhs_->Execute(closure, context) }, context);
            }
        }
        if (lhs_->Execute(closure, context).TryAs<runtime::String>() != nullptr && rhs_->Execute(closure, context).TryAs<runtime::String>() != nullptr) {
            return ObjectHolder::Own(runtime::String(lhs_->Execute(closure, context).TryAs<runtime::String>()->GetValue() + rhs_->Execute(closure, context).TryAs<runtime::String>()->GetValue()));
        }
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr && rhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr) {
            return ObjectHolder::Own(runtime::Number(lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue() + rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue()));
        }
        throw std::runtime_error("The Add operation cannot be performed "s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr && rhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr) {
            return ObjectHolder::Own(runtime::Number(lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue() - rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue()));
        }
        throw std::runtime_error("Arguments is not a number"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr && rhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr) {
            return ObjectHolder::Own(runtime::Number(lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue() * rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue()));
        }
        throw std::runtime_error("Arguments is not a number"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr && rhs_->Execute(closure, context).TryAs<runtime::Number>() != nullptr) {
            if (rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue() == 0) {
                throw std::runtime_error("You can't divide by zero"s);
            }
            return ObjectHolder::Own(runtime::Number(lhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue() / rhs_->Execute(closure, context).TryAs<runtime::Number>()->GetValue()));
        }
        throw std::runtime_error("Arguments is not a number"s);
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (size_t i = 0; i < manuals_.size(); i++) {
            auto result = manuals_[i]->Execute(closure, context);
        }
        return ObjectHolder::None();
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        ObjectHolder holder = statement_->Execute(closure, context);
        throw holder;
        return {};
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(std::move(cls)) {
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
        return closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv) : object_(object), name_(field_name), rv_(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        return object_.Execute(closure, context).TryAs<runtime::ClassInstance>()->Fields()[name_] = rv_->Execute(closure, context);
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body, std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        }
        if (else_body_) {
            return else_body_->Execute(closure, context);
        }
        return {};
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(lhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool(true));
        }
        if (runtime::IsTrue(rhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool(true));
        }
        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        if (!runtime::IsTrue(lhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool(false));
        }
        if (!runtime::IsTrue(rhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool(false));
        }
        return ObjectHolder::Own(runtime::Bool(true));
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool(!runtime::IsTrue(argument_->Execute(closure, context))));
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(std::move(cmp)) {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool(cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context)));
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) : instance_(class_), args_(std::move(args)) {
    }

    NewInstance::NewInstance(const runtime::Class& class_) : instance_(class_), args_(nullopt) {
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        if (args_ != nullopt) {
            if (instance_.HasMethod(INIT_METHOD, args_->size())) {
                std::vector<runtime::ObjectHolder> actuel_args;
                for (size_t i = 0; i < args_->size(); i++) {
                    actuel_args.push_back((*args_)[i]->Execute(closure, context));
                }
                instance_.Call(INIT_METHOD, actuel_args, context);
            }
        }
        return ObjectHolder::Share(instance_);
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(std::move(body)) {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_->Execute(closure, context);
        }
        catch (const ObjectHolder& obj) {
            return obj;
        }
        return ObjectHolder::None();
    }

}  // namespace ast