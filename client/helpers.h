#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <future>

namespace td_api = td::td_api;
using Object = td_api::object_ptr<td_api::Object>;

// ======================== Utility functions
bool is_error(td_api::object_ptr<td_api::Object> &object) {
    if (object == nullptr) {
        return false;
    }
    if (object->get_id() == td_api::error::ID) {
        auto error = td::move_tl_object_as<td_api::error>(object);
        std::cerr << "ERROR: " << error->message_ << "\n";
        return true;
    }
    return false;
}

// overloaded
namespace detail {
    template<class... Fs>
    struct overload;

    template<class F>
    struct overload<F> : public F {
        explicit overload(F f) : F(f) {
        }
    };

    template<class F, class... Fs>
    struct overload<F, Fs...>
            : public overload<F>, public overload<Fs...> {
        overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
        }

        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
}  // namespace detail

template<class... F>
auto overloaded(F... f) {
    return detail::overload<F...>(f...);
}

class AuthClient {
public:
    AuthClient() {
        td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
        client_manager_ = std::make_unique<td::ClientManager>();
        client_id_ = client_manager_->create_client_id();
        send_query(td_api::make_object<td_api::getOption>("version"), {});
        auth();
        load_chats();
        get_me();
    }

    virtual bool isOk(td_api::object_ptr<td_api::chat> &chat) {
        return true;
    };

    void auth() {
        while (!authorized_) {
            process_response_auth(client_manager_->receive(1));
        }
        std::cerr << "authorized!\n";
    }

    void load_chats() {
        send_query(td_api::make_object<td_api::loadChats>(nullptr, 100), [](Object object) {
        });
    }

    void get_me() {
        send_query(td_api::make_object<td_api::getMe>(),
                   [this](Object object) {
                       me_ = td::move_tl_object_as<td_api::user>(object);
                       std::cout << to_string(me_) << std::endl;
                   });
    }

    void process_response_auth(td::ClientManager::Response &&response) {
        if (!response.object) {
            return;
        }
        if (response.request_id == 0) {
            return process_update_base(std::move(response.object));
        }
        auto it = handlers_.find(response.request_id);
        if (it != handlers_.end()) {
            it->second(std::move(response.object));
            handlers_.erase(it);
        }
    }

    void process_update_base(td_api::object_ptr<td_api::Object> update) {
        td_api::downcast_call(
                *update, overloaded(
                        [this](td_api::updateAuthorizationState &update_authorization_state) {
                            authorization_state_ = std::move(update_authorization_state.authorization_state_);
                            on_authorization_state_update();
                        },
                        [this](td_api::updateNewChat &update_new_chat) {
                            chats_title_[update_new_chat.chat_->id_] = update_new_chat.chat_->title_;
                            chats_[update_new_chat.chat_->id_] = std::move(update_new_chat.chat_);
                        },
                        [this](td_api::updateChatTitle &update_chat_title) {
                            chats_title_[update_chat_title.chat_id_] = update_chat_title.title_;
                        },
                        [this](td_api::updateUser &update_user) {
                            auto user_id = update_user.user_->id_;
                            users_[user_id] = std::move(update_user.user_);
                        },
                        [](td_api::updateOption &update_option) {
                        },
                        [](td_api::updateChatLastMessage &msg) {
                        },
                        [](td_api::updateChatPosition &msg) {
                        },
                        [](td_api::updateSupergroup &msg) {
                        },
                        [](td_api::updateSupergroupFullInfo &msg) {
                        },
                        [](td_api::updateConnectionState &msg) {
                            std::cerr << to_string(msg) << "\n";
                        },
                        [](auto &update) {
                        }));
    }

    void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
        auto query_id = ++current_query_id_;
        if (handler) {
            handlers_.emplace(query_id, std::move(handler));
        }
        client_manager_->send(client_id_, query_id, std::move(f));
    }

    std::unique_ptr<td::ClientManager> client_manager_;
    std::int32_t client_id_{0};
    std::uint64_t current_query_id_{0};
    std::map<std::uint64_t, std::function<void(Object)>> handlers_;

    td::tl_object_ptr<td::td_api::user> me_;
    std::map<std::int64_t, td_api::object_ptr<td_api::user>> users_;
    std::map<std::int64_t, std::string> chats_title_;
    std::map<std::int64_t, td_api::object_ptr<td_api::chat>> chats_;

private:
    td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
    bool authorized_{false};

    std::uint64_t authentication_query_id_{0};

    auto create_authentication_query_handler() {
        return [this, id = authentication_query_id_](Object object) {
            if (id == authentication_query_id_) {
                check_authentication_error(std::move(object));
            }
        };
    }

    void on_authorization_state_update() {
        authentication_query_id_++;
        td_api::downcast_call(
                *authorization_state_,
                overloaded(
                        [this](td_api::authorizationStateReady &) {
                            authorized_ = true;
                            std::cerr << "Got authorization" << std::endl;
                        },
                        [this](td_api::authorizationStateLoggingOut &) {
                            authorized_ = false;
                            std::cerr << "Logging out" << std::endl;
                        },
                        [](td_api::authorizationStateClosing &) { std::cerr << "Closing" << std::endl; },
                        [this](td_api::authorizationStateClosed &) {
                            authorized_ = false;
                            std::cerr << "Terminated" << std::endl;
                        },
                        [this](td_api::authorizationStateWaitCode &) {
                            std::cerr << "Enter authentication code: " << std::flush;
                            std::string code;
                            std::cin >> code;
                            send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitRegistration &) {
                            std::string first_name;
                            std::string last_name;
                            std::cerr << "Enter your first name: " << std::flush;
                            std::cin >> first_name;
                            std::cerr << "Enter your last name: " << std::flush;
                            std::cin >> last_name;
                            send_query(td_api::make_object<td_api::registerUser>(first_name, last_name),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitPassword &) {
                            std::cerr << "Enter authentication password: " << std::flush;
                            std::string password;
                            std::getline(std::cin, password);
                            send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                                       create_authentication_query_handler());
                        },
                        [](td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
                            std::cerr << "Confirm this login link on another device: " << state.link_ << std::endl;
                        },
                        [this](td_api::authorizationStateWaitPhoneNumber &) {
                            std::cerr << "Enter phone number: " << std::flush;
                            std::string phone_number;
                            std::cin >> phone_number;
                            send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitEncryptionKey &) {
                            send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(""),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitTdlibParameters &) {
                            auto parameters = td_api::make_object<td_api::tdlibParameters>();
                            parameters->database_directory_ = "tdlib";
                            parameters->use_message_database_ = true;
                            parameters->use_secret_chats_ = true;
                            parameters->api_id_ = std::atoll(std::getenv("API_ID"));
                            parameters->api_hash_ = std::getenv("API_HASH");
                            parameters->system_language_code_ = "en";
                            parameters->device_model_ = "cleanup";
                            parameters->application_version_ = "1.0";
                            parameters->enable_storage_optimizer_ = true;
                            send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                                       create_authentication_query_handler());
                        }));
    }

    void check_authentication_error(Object object) {
        if (object->get_id() == td_api::error::ID) {
            auto error = td::move_tl_object_as<td_api::error>(object);
            std::cerr << "Error: " << to_string(error) << std::flush;
            on_authorization_state_update();
        }
    }
};
