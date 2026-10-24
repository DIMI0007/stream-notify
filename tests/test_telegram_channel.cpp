#include "../src/telegram_channel.hpp"
#include <iostream>
int main() {
    const struct {const char* input;const char* expected;} cases[]={
        {" @otmopozok1101 ","@otmopozok1101"},
        {"https://t.me/ozk_live","@ozk_live"},
        {"t.me/ozk_live/42","@ozk_live"},
        {"https://t.me/s/ozk_live/42","@ozk_live"},
        {"https://t.me/c/1234567890/42","-1001234567890"},
        {"-1001234567890","-1001234567890"},
        {"https://t.me/+invite","https://t.me/+invite"},
        {"https://t.me/joinchat/invite","https://t.me/joinchat/invite"},
        {"https://evil.example/ozk_live","https://evil.example/ozk_live"},
        {"https://t.me/c/not-a-number/42","https://t.me/c/not-a-number/42"}
    };
    for(const auto& item:cases) {
        if(telegramChannel(QString::fromUtf8(item.input))!=QString::fromUtf8(item.expected)){
            std::cerr<<"Telegram recipient normalization failed\n";return 1;
        }
    }
    std::cout<<"Telegram recipient tests passed\n";
}
