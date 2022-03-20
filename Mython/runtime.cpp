#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (!object) {
            return false;
        }
        if (object.TryAs<Number>() != nullptr) {
            return object.TryAs<Number>()->GetValue() != 0;
        }
        if (object.TryAs<Bool>() != nullptr) {
            return object.TryAs<Bool>()->GetValue();
        }
        if (object.TryAs<String>() != nullptr) {
            return object.TryAs<String>()->GetValue() != ""s;
        }
        if (object.TryAs<Class>() != nullptr || object.TryAs<ClassInstance>() != nullptr) {
            return false;
        }
        throw std::runtime_error("Error converting to the bool type"s);
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        auto __str__ptr = cls_->GetMethod("__str__"s);
        if (__str__ptr == nullptr) {
            os << this;
        }
        else {
            os << this->Call("__str__"s, std::vector<ObjectHolder>{}, context).TryAs<String>()->GetValue();
        }
    }

    bool ClassInstance::HasMethod(const std::string& name_method, size_t argument_count) const {
        auto method = cls_->GetMethod(name_method);
        return method != nullptr && method->formal_params.size() == argument_count;
    }

    Closure& ClassInstance::Fields() {
        return object_fields;
    }

    const Closure& ClassInstance::Fields() const {
        return object_fields;
    }

    ClassInstance::ClassInstance(const Class& cls) : cls_(&cls) {
    }

    ObjectHolder ClassInstance::Call(const std::string& name_method, const std::vector<ObjectHolder>& actual_args, Context& context) {

        if (!HasMethod(name_method, actual_args.size())) {
            throw std::runtime_error("Not implemented"s);
        }

        auto method = cls_->GetMethod(name_method);

        Closure args;

        args.emplace("self"s, ObjectHolder::Share(*this));
        
        for (size_t i = 0; i < actual_args.size(); i++) {
            args.emplace(method->formal_params[i], actual_args[i]);
        }
        return method->body->Execute(args, context);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent) : name_(std::move(name)), store_methods_(std::move(methods)), parent_(parent) {
        for (Method& method : store_methods_) {
            methods_[method.name] = &method;
        }
    }

    const Method* Class::GetMethod(const std::string& name) const {
        const Method* result = nullptr;
        auto it = methods_.find(name);
        if (it == methods_.end()) {
            if (parent_ == nullptr) {
                result = nullptr;
            }
            else {
                result = parent_->GetMethod(name);
            }
        }
        else {
            result = it->second;
        }
        return result;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, Context& /*context*/) {
        os << "Class " << GetName();
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs && !rhs) {
            return true;
        }
        if (lhs.TryAs<Number>() != nullptr && rhs.TryAs<Number>() != nullptr) {
            return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
        }
        if (lhs.TryAs<String>() != nullptr && rhs.TryAs<String>() != nullptr) {
            return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
        }
        if (lhs.TryAs<Bool>() != nullptr && rhs.TryAs<Bool>() != nullptr) {
            return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
        }
        if (lhs.TryAs<ClassInstance>() != nullptr && lhs.TryAs<ClassInstance>()->HasMethod("__eq__"s, 1)) {
            return IsTrue(lhs.TryAs<ClassInstance>()->Call("__eq__"s, { rhs }, context));
        }
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (lhs.TryAs<Number>() != nullptr && rhs.TryAs<Number>() != nullptr) {
            return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
        }
        if (lhs.TryAs<String>() != nullptr && rhs.TryAs<String>() != nullptr) {
            return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
        }
        if (lhs.TryAs<Bool>() != nullptr && rhs.TryAs<Bool>() != nullptr) {
            return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
        }
        if (lhs.TryAs<ClassInstance>() != nullptr && lhs.TryAs<ClassInstance>()->HasMethod("__lt__"s, 1)) {
            return IsTrue(lhs.TryAs<ClassInstance>()->Call("__lt__"s, { rhs }, context));
        }
        throw std::runtime_error("Cannot compare objects for less"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context) && NotEqual(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Greater(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime