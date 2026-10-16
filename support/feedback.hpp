#pragma once
#include "feedback_contract.hpp"
#include <mutex>
#include <thread>
namespace shaker {
enum class FeedbackState {Idle,Sending,Sent,Failed};
struct FeedbackStatus {FeedbackState state=FeedbackState::Idle;std::string message;};
class FeedbackClient {
    mutable std::mutex mutex_;
    FeedbackStatus status_;
    std::jthread worker_;
public:
    FeedbackStatus status()const;
    void send(const Json& payload);
};
}
