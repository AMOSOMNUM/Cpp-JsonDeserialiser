    

    template <typename Prototype>
    struct AfterDeserialise : public Prototype {
        using T = typename Prototype::Target;
        const std::function<void(T&)> functor;

        template <typename Functor, typename... Args>
        AfterDeserialise(Functor&& f, Args&&... args)
            : Prototype(args...), functor(std::forward<Functor>(f)) {}

        inline void assign(const Json& json) {
            Prototype::assign(json);
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

        inline void assign(const Json& json) {
            functor(this->template value<T>());
            Prototype::assign(json);
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
    template <typename Pointer, typename BaseType, typename DerivedTypes>
    struct PointerDowncast : public DeserialisableBaseHelper<Pointer> {
        using Target = Pointer;
        using Base = DeserialisableBaseHelper<Target>;
        const std::function<int(const Json&)> deductor1;
        const std::function<int(const BaseType&)> deductor2;

        template <typename Deductor1, typename Deductor2, typename... Args>
        PointerDowncast(Deductor1&& f1, Deductor2&& f2, Args&&... args)
            : Base(std::forward<Args>(args)...), deductor1(std::forward<Deductor1>(f1)),
              deductor2(std::forward<Deductor2>(f2)) {}

        template <int N>
        inline void assign_if_eq(int index, const typename Lib::JsonObject& obj) {
            using Type = typename GetType<N, DerivedTypes>::Type;
            if (N == index) {
                auto ptr = new Type();
                DeserialisableType<Type>(*ptr)->assign(obj);
                this->template value<Target>() = ptr;
            }
        }
        template <int N>
        inline void serialise_if_eq(int index, Json& json) {
            using Type = typename GetType<N, DerivedTypes>::Type;
            if (N == index)
                json = DeserialisableType<Type>(*this->template value<Target>()).to_json();
        }

        void assign(const Json& json) {
            int index = deductor1(json);
            if (index == -1)
                throw std::ios_base::failure("Type Unmatch!");
            (assign_if_eq<pack>(index, obj), ...);
        }
        Json to_json() const {
            Json result;
            int index = deductor2(*this->template value<Target>());
            if (index == -1)
                throw std::ios_base::failure("Type Unmatch!");
            (serialise_if_eq<pack>(index, result), ...);
        }
    };

    template <typename Prototype, typename Guard>
    struct GivenLockGuard;

    template <typename Prototype, template <typename> typename Guard, typename Lock>
    struct GivenLockGuard<Prototype, Guard<Lock>> : public Prototype {
        mutable Lock& lock;

        template <typename... Args>
        GivenLockGuard(Lock& lock, Args&&... args) : Prototype(args...), lock(lock) {}

        inline void assign(const Json& json) {
            Guard guard(lock);
            Prototype::assign(json);
        }
        inline Json to_json() const {
            Guard guard(lock);
            return Prototype::to_json();
        }
    };

    template <typename Prototype, template <typename...> typename Guard, typename Lock>
    struct GivenLockGuard<Prototype, Guard<Lock>> : public Prototype {
        mutable Lock& lock;

        template <typename... Args>
        GivenLockGuard(Lock& lock, Args&&... args) : Prototype(args...), lock(lock) {}

        inline void assign(const Json& json) {
            Guard guard(lock);
            Prototype::assign(json);
        }
        inline Json to_json() const {
            Guard guard(lock);
            return Prototype::to_json();
        }
    };

    template <typename Prototype, typename Guard, auto member_offset>
    struct MemberLockGuard : public Prototype {
        template <typename... Args>
        MemberLockGuard(Args&&... args) : Prototype(args...) {}

        inline void assign(const Json& json) {
            Guard guard(this->template value<T>().*member_offset);
            Base::assign(json);
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

        inline void assign(const Json& json) {
            Guard guard(functor(this->template value<T>()));
            Base::assign(json);
        }
        inline Json to_json() const {
            Guard guard(functor(this->template value<T>()));
            return Base::to_json();
        }
    };