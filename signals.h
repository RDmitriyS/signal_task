#pragma once

#include <functional>
#include <memory>
#include <list>

namespace signals {

    template<typename T>
    struct signal;

    template<typename... Args>
    struct signal<void(Args...)> {

        using slot_t = std::function<void(Args...)>;
        using slots_t = std::list<slot_t>;

        struct connection {
            connection() = default;

            connection(signal *sig, typename slots_t::iterator it) :
                sig(sig),
                it(it),
                destroyed(sig ? sig->destroyed : nullptr) {
            }

            connection(connection && other) noexcept :
                sig(other.sig),
                it(std::move(other.it)),
                destroyed(std::move(other.destroyed)) {

                other.sig = nullptr;
                other.destroyed = nullptr;
            }

            connection &operator=(connection && rhs) noexcept {
                if (this != &rhs) {
                    disconnect();
                    sig = rhs.sig;
                    destroyed = rhs.destroyed;
                    it = std::move(rhs.it);
                    rhs.sig = nullptr;
                    rhs.destroyed = nullptr;
                }
                return *this;
            }

            ~connection() {
                disconnect();
            }

            void disconnect() {
                if (sig && !*destroyed) {
                    if (sig->inside_emit) {
                        *it = slot_t();
                    } else {
                        sig->slots.erase(it);
                    }

                    sig = nullptr;
                    destroyed = nullptr;
                }
            }

        private:
            signal *sig = nullptr;
            std::shared_ptr<bool> destroyed;
            typename slots_t::iterator it;
        };

        signal() : destroyed(std::make_shared<bool>(false)) {}

        signal(signal const & other) :
            destroyed(std::make_shared<bool>(false)),
            slots(other.slots) {
        }

        signal &operator=(signal const & rhs) {
            if (this != rhs) {
                slots = rhs.slots;
                destroyed = rhs.destroyed;
            }

            return *this;
        }

        ~signal() {
            *destroyed = true;
        }

        connection connect(slot_t slot) {
            slots.push_back(std::move(slot));
            return connection(this, std::prev(slots.end()));
        }

        void operator()(Args... args) const {
            ++inside_emit;
            std::shared_ptr<bool> local_destroyed = destroyed;

            try {
                for (auto i = slots.begin(); i != slots.end(); ++i) {
                    if (*i) {
                        (*i)(std::forward<Args>(args)...);
                        if (*local_destroyed) {
                            return;
                        }
                    }
                }
            } catch (...) {
                leave_emit();
                throw;
            }

            leave_emit();
        }

    private:
        void leave_emit() const noexcept {
            if (--inside_emit != 0)
                return;

            for (auto i = slots.begin(); i != slots.end();) {
                *i ? ++i : i = slots.erase(i);
            }
        }

        mutable slots_t slots;
        mutable size_t inside_emit = 0;
        mutable std::shared_ptr<bool> destroyed;
    };
}
