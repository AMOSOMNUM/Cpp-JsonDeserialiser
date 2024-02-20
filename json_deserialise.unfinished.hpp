    

    template <typename Prototype>
    struct AfterDeserialise : public Prototype {
        using T = typename Prototype::Target;
        const std::function<void(T&)> functor;

        template <typename Functor, typename... Args>
        AfterDeserialise(Functor&& f, Args&&... args)
            : Prototype(args...), functor(std::forward<Functor>(f)) {}

        inline void from_json(const Json& json) {
            Prototype::from_json(json);
            functor(this->template value<T>());
        }
    };

    template <typename Prototype>
    struct AfterSerialise : public Prototype {
        using T = typename Prototype::Target;
        const std::function<void(const T&)> functor;

        template <typename Functor, typename... Args>
        AfterSerialise(Functor&& f, Args&&... args)
            : Prototype(args...), functor(std::forward<Functor>(f)) {}

        inline Json to_json() const {
            auto result = Base::to_json();
            functor(this->template value<T>());
            return result;
        }
    };

    template <typename Prototype>
    struct BeforeDeserialise : public Prototype {
        using T = typename Prototype::Target;
        const std::function<void(T&)> functor;

        template <typename Functor, typename... Args>
        BeforeDeserialise(Functor&& f, Args&&... args)
            : Prototype(args...), functor(std::forward<Functor>(f)) {}

        inline void from_json(const Json& json) {
            functor(this->template value<T>());
            Prototype::from_json(json);
        }
    };

    template <typename Prototype>
    struct BeforeSerialise : public Prototype {
        using T = typename Prototype::Target;
        const std::function<void(const T&)> functor;

        template <typename Functor, typename... Args>
        BeforeSerialise(Functor&& f, Args&&... args)
            : Prototype(args...), functor(std::forward<Functor>(f)) {}

        inline Json to_json() const {
            functor(this->template value<T>());
            return Prototype::to_json();
        }
    };

    template <typename UnionType, auto TypeInfo, typename... MemberInfo>
    struct Union : public DeserialisableBaseHelper<UnionType> {
        using Target = UnionType;
        using Base = DeserialisableBaseHelper<UnionType>;
    };

    template <typename Pointer, typename BaseType, typename DerivedTypes>
    struct BasePointerDeserialise : public DeserialisableBaseHelper<Pointer> {
        using Target = Pointer;
        using Base = DeserialisableBaseHelper<Target>;
        const std::function<int(const Json&)> deductor;

        template <typename Deductor, typename... Args>
        BasePointer(Deductor&& f, Args&&... args)
            : Base(std::forward<Args>(args)...), deductor(std::forward<Deductor>(f)) {}

        template <int N>
        inline void assign_if_eq(int index, const typename Lib::JsonObject& obj) {
            using Type = typename GetType<N, DerivedTypes>::Type;
            if (N == index) {
                auto ptr = new Type();
                DeserialisableType<Type>(*ptr)->from_json(obj);
                this->template value<Target>() = ptr;
            }
        }

        void from_json(const Json& json) {
            int index = deductor1(json);
            if (index == -1)
                throw std::ios_base::failure("Type Unmatch!");
            (assign_if_eq<pack>(index, obj), ...);
        }
    };

    template <typename Prototype, typename Guard, auto member_offset>
    struct MemberLockGuard : public Prototype {
        template <typename... Args>
        MemberLockGuard(Args&&... args) : Prototype(args...) {}

        inline void from_json(const Json& json) {
            Guard guard(this->template value<T>().*member_offset);
            Base::from_json(json);
        }
        inline Json to_json() const {
            Guard guard(this->template value<T>().*member_offset);
            return Base::to_json();
        }
    };

    template <typename Prototype, typename Guard, typename Functor>
    struct FetchLockGuard : public Prototype {
        const std::function<Lock&(const T&)> functor;

        template <typename... Args>
        FetchLockGuard(Functor&& f, Args&&... args)
            : Prototype(args...), functor(std::forward<Functor>(f)) {}

        inline void from_json(const Json& json) {
            Guard guard(functor(this->template value<T>()));
            Base::from_json(json);
        }
        inline Json to_json() const {
            Guard guard(functor(this->template value<T>()));
            return Base::to_json();
        }
    };