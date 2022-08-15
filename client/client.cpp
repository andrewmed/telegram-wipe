#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include "helpers.h"

const int64_t BACK = 60 * 60 * 24 * 30 * 12; // to search to - one year ago
const int64_t KEEP = 60 * 60 * 24 * 30; // to start search from - one month back
const auto SLEEP = std::chrono::hours(1);

class TdClient : public AuthClient {
public:
    TdClient() = default;;

    bool isOk(td_api::object_ptr<td_api::chat> &chat) override {
        if (chat->last_read_inbox_message_id_ == 0) {
            return false;
        }
        return true;
    };

    void process_response(td::ClientManager::Response &&response) {
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

    void deplete() {
        while (true) {
            auto response = client_manager_->receive(0.1);
            if (!response.object) {
                break;
            }
            process_response(std::move(response));
        }
    }

    void automatic() {
        while(true) {
            deplete();
            if (!handlers_.empty()) {
                continue;
            }
            std::vector<int64_t> ids{};
            for (auto &chat: chats_) {
                auto id = chat.second->id_;
                if (id >= 0) {
                    continue;
                }
                if (!isOk(chat.second)) {
                    continue;
                }
                ids.push_back(id);
            }
            for (auto& id : ids) {
                clean(id);
            }
            std::cerr << ids.size() << " total processed\n";
            std::this_thread::sleep_for(SLEEP);
        }
    }

    void manual() {
        while (true) {
            deplete();
            if (!handlers_.empty()) {
                continue;
            }
            std::string action;
            std::cerr << "Enter action [q] quit [c] show chats "
                         "[me] show self [l] logout [number] for chat: "
                      << std::endl;
            std::string line;
            std::getline(std::cin, line);
            std::istringstream ss(line);

            if (!(ss >> action)) {
                continue;
            }

            if (action == "me") {
                get_me();
                continue;
            }

            if (action == "q") {
                return;
            }

            if (action == "l") {
                std::cerr << "Logging out..." << std::endl;
                send_query(td_api::make_object<td_api::logOut>(),
                           [](Object object) {
                               std::cerr << "logged out\n";
                               exit(0);
                           });
                continue;
            }
            if (action == "c") {
                int counter = 0;
                for (auto &chat: chats_) {
                    auto &id = chat.second->id_;
                    if (id >= 0) {
                        continue;
                    }
                    std::cerr << "[chat_id:" << id << "] [title:" << chat.second->title_ << "]" << std::endl;
                    counter++;
                }
                std::cerr << counter << " total\n";
                continue;
            }
            auto chat_id = std::stoll(action);
            clean(chat_id);
        }
    }

private:
    void clean(int64_t chat_id) {
        int64_t limit = std::time(nullptr) - BACK;
        auto &client_data = chats_[chat_id]->client_data_;
        std::cerr << "processing: " << chats_title_[chat_id] << "  "  << chat_id << "\t";
        if (!client_data.empty()) {
            std::cerr << " using limit from cache ";
            limit = std::atoll(client_data.c_str());
        }
        int64_t processed = 0;
        do_clean(chat_id, 0, 0, std::time(nullptr) - KEEP, limit, processed);
        while(!handlers_.empty()){
            deplete();
        };
        std::cerr << " ..." << processed << " processed\n";
    }

    void store_client_data(int64_t chat, std::string &&data) {
        if (!data.empty()) {
            send_query(td_api::make_object<td_api::setChatClientData>(chat, data),
                       [this, chat, data](Object object) {
                           chats_[chat]->client_data_ = data;
                       });
        }
    }

    void del(int64_t chat_id, std::vector<int64_t> &&ids) {
        auto size = ids.size();
        if (size) {
            send_query(td_api::make_object<td_api::deleteMessages>(chat_id, std::move(ids), true),
                       [size](Object object) {
                           if (!is_error(object)) {
                               std::cerr << size << " deleting success!\n";
                           }
                       });
        }
    }

    void do_clean(int64_t chat_id, int64_t orig_id, int64_t id, int64_t start, int64_t limit, int64_t &processed) {
        send_query(td_api::make_object<td_api::getChatHistory>(chat_id, id, 0, 100, false),
                   [this, orig = orig_id, chat_id, start, limit, &processed](Object object) {
                       if (is_error(object)) {
                           return;
                       }
                       auto orig_id = orig;
                       std::vector<int64_t> ids{};
                       auto messages = td::move_tl_object_as<td_api::messages>(object);
                       int64_t lastId = 0;
                       for (auto &message: messages->messages_) {
                           if (is_error(object)) {
                               continue;
                           }
                           processed++;
                           lastId = message->id_;
                           if (message->date_ < limit) {
                               del(chat_id, std::move(ids));
                               if (orig_id != 0) {
                                   // if we passed the old limit we want to store "from" as a new limit
                                   store_client_data(chat_id, std::to_string(orig_id));
                               }
                               return;
                           }
                           if (message->date_ < start) {
                               if (orig_id == 0) {
                                   orig_id = message->date_;
                               }
                               auto reactionInfo = td::move_tl_object_as<td_api::messageInteractionInfo>(message->interaction_info_);
                               if (reactionInfo) {
                                   for (auto &reactionObject: reactionInfo->reactions_) {
                                       auto reaction = td::move_tl_object_as<td_api::messageReaction>(reactionObject);
                                       if (!reaction) {
                                           continue;
                                       }
                                       if (reaction->is_chosen_) {
                                           send_query(td_api::make_object<td_api::setMessageReaction>(chat_id, message->id_, "", false),
                                                      [](Object object) {
                                                          if (!is_error(object)) {
                                                              std::cerr << "done unsetting reaction\n";
                                                          }
                                                      });
                                           break; // one reaction per post
                                       }
                                       for (auto &senderObject: reaction->recent_sender_ids_) {
                                           auto sender = td::move_tl_object_as<td_api::MessageSender>(senderObject);
                                           if (!sender) {
                                               continue;
                                           }
                                           auto sender_user = td::move_tl_object_as<td_api::messageSenderUser>(sender);
                                           if (sender_user != nullptr && sender_user->user_id_ == me_->id_) {
                                               std::cerr << sender_user->user_id_ << sender_user->user_id_ << "\n";
                                               send_query(td_api::make_object<td_api::setMessageReaction>(chat_id, message->id_, "", false),
                                                          [](Object object) {
                                                              if (!is_error(object)) {
                                                                  std::cerr << "done unsetting reaction\n";
                                                              }
                                                          });
                                               break; // one reaction per post
                                           }
                                       }
                                   }
                               }

                               if (!message->can_be_deleted_for_all_users_) {
                                   continue;
                               }
                               auto sender = td::move_tl_object_as<td_api::messageSenderUser>(message->sender_id_);
                               if (sender != nullptr && sender->user_id_ == me_->id_) {
                                   ids.push_back(message->id_);
                               }
                           }
                       }
                       del(chat_id, std::move(ids));
                       if (lastId == 0) {
                           // if there are no more messages and we have passed "from" mark, we want to store it as new "limit"
                           store_client_data(chat_id, std::to_string(orig_id));
                           return;
                       }
                       // we did not hit the limit yet, repeat
                       do_clean(chat_id, orig_id, lastId, start, limit, processed);
                   }
        );
    }
};

int main() {
    TdClient example{};
    example.automatic();
}