#include "jack_handler.h"
#include <fstream>
#include <optional>
#include <stdexcept>
#include <spawn.h>
#include <thread>
#include "parse_utils.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <fcntl.h>
#include <libudev.h>

#define MAX_CARDS 64

using namespace std;

extern char **environ;

bool isAlive(pid_t pid) {
    if (pid <= 0) return false;

    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);

    return result == 0;
}

bool fd_wait(int fd) {
    if (fd < 0) return false;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    struct timeval tv {0, 100000}; // 100ms

    int ret = select(fd + 1, &fds, NULL, NULL, &tv);

    return ret > 0;
}

struct JackHandler::Impl
{
    Impl(const string& configPath) {
        parseCardConfig(configPath);
    }
    ~Impl() {
        killProcess();
        if (t1.joinable()) t1.join();
        if (t2.joinable()) t2.join();
    }

    struct CardSetup {
        string systemId;
        string deviceName;
        int rate;
        int period;
        int nperiod;
        optional<int> inputs;
        optional<int> outputs;
    };

    enum Action {
        SPAWN,
        KILL
    };

    mutex cardMutex;

    vector<CardSetup> registeredCards;
    unordered_map<string, CardSetup*> cardsById;

    void parseCardConfig(const string& configPath) {
        ifstream configFile(configPath);
        if (!configFile.is_open())
            throw runtime_error("Failed to open config: " + configPath);

        string line;
        CardSetup* processingCard = nullptr;
        registeredCards.reserve(MAX_CARDS);

        while(getline(configFile, line)) {
            utils::strip_comment(line);
            utils::trim(line);

            if (line.empty()) continue;

            if(line == "[card]") {
                registeredCards.push_back({
                    .systemId = "",
                    .deviceName = "Generic",
                    .rate = 48000,
                    .period = 51024,
                    .nperiod = 2,
                    .inputs = nullopt,
                    .outputs = nullopt,
                });
                processingCard = &registeredCards.back();

                continue;
            }

            string k, v;
            if (!utils::split_kv(line, k, v)) continue;

            utils::to_lower(k);

            if (processingCard) {
                if (k == "system") {
                    cardsById[v] = &registeredCards.back();
                    processingCard->systemId = v;
                    cout << "Registered card " << v << ";\n";
                }
                else if (k == "device") processingCard->deviceName = v;
                else if (k == "rate") processingCard->rate = utils::parse_int(v);
                else if (k == "period") processingCard->period = utils::parse_int(v);
                else if (k == "nperiod") processingCard->nperiod = utils::parse_int(v);
                else if (k == "inputs") processingCard->inputs = utils::parse_int(v);
                else if (k == "outputs") processingCard->outputs = utils::parse_int(v);
            }
        }
    }

    vector<string> getAlsaCards(){
        ifstream file("/proc/asound/cards");
        vector<string> result;

        if (!file.is_open())
            return result;

        string line;
        while (getline(file, line)) {
            // " 0 [Device     ]: USB-Audio - Device Name"
            auto lb = line.find('[');
            auto rb = line.find(']');

            if (lb != string::npos && rb != string::npos && rb > lb) {
                string name = line.substr(lb + 1, rb - lb - 1);

                while (!name.empty() && isspace(name.back())) name.pop_back();
                while (!name.empty() && isspace(name.front())) name.erase(name.begin());

                result.push_back(name);
            }
        }

        return result;
    }

    void startConnectedCard() {
        auto alsaCards = getAlsaCards();
        for (auto& cfg : registeredCards) {
            cout << "Checking if " << cfg.deviceName << " is connected...\n";
            for (auto& sysCard : alsaCards) {
                if (cfg.deviceName == sysCard) {
                    cout << "Device found; Attempting to connect...\n";
                    asyncProcess(Action::SPAWN, cfg.systemId, "");
                    return;
                }
            }
        }
    }

    thread t1, t2;
    string activeDevPath = "";

    void asyncProcess(Action action, string cardId, string devPath) {
        switch (action) {
            case SPAWN: {
                if (t1.joinable()) t1.join();
                t1 = thread([this, cardId, devPath](){
                    auto card = cardsById.find(cardId);
                    if (card != cardsById.end()) {
                        spawnProcess(*card->second, devPath);
                    } else {
                        cerr << "Card " << cardId << " not registered, doing nothing;\n";
                    }
                });
                break;
            }
            case KILL: {
                if (t2.joinable()) t2.join();
                t2 = thread([this](){
                    killProcess();
                });
            }
        }
    }

    void watchUdev(const atomic<bool>& shouldQuit) {
        struct udev* udev = udev_new();
        struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");

        udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", NULL);
        udev_monitor_enable_receiving(mon);

        int fd = udev_monitor_get_fd(mon);

        cout << "Watching for UDEV events at registered cards;\n";

        while (!shouldQuit) {
            if (!fd_wait(fd)) continue;

            struct udev_device* dev = udev_monitor_receive_device(mon);

            if (dev) {
                string action = udev_device_get_action(dev);
                const char* model_ = udev_device_get_property_value(dev, "ID_USB_MODEL");
                const char* devPath_ = udev_device_get_devpath(dev);
                string model = model_ ? string(model_) : "";
                string devPath = devPath_ ? string(devPath_) : "";

                if (action == "bind" && model_) {
                    asyncProcess(Action::SPAWN, model, devPath);
                } else if (action == "remove" && (devPath == activeDevPath || devPath.empty())) {
                    asyncProcess(Action::KILL, model, devPath);
                }
                udev_device_unref(dev);
            }
        }
        udev_unref(udev);
    }

    pid_t pid;
    bool isRunning = false;

    void spawnProcess(CardSetup& card, string devPath) {
        lock_guard<mutex> lock(cardMutex);
        if (isRunning) return;
        cout
        << "----------------------------------------------\n"
        << "--------- Attempting to start JACK -----------\n"
        << "----------------------------------------------\n\n";
        cout << "Using sound card " << card.deviceName << ";\n\n";

        vector<string> args = {
            "jackd",
            "-d", "alsa",
            "-d", "hw:" + card.deviceName,
            "-r", to_string(card.rate),
            "-p", to_string(card.period),
            "-n", to_string(card.nperiod),
        };
        if (card.inputs) {
            args.push_back("-i" + to_string(*card.inputs));
        }
        if (card.outputs) {
            args.push_back("-o" + to_string(*card.outputs));
        }
        vector<char*> argv;
        for (auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        int res = posix_spawnp(
            &pid,
            "jackd",
            nullptr,
            nullptr,
            argv.data(),
            environ 
        );
        if (res == 0) {
            this_thread::sleep_for(chrono::milliseconds(300));

            if (isAlive(pid)) {
                isRunning = true;
                activeDevPath = devPath;
            } else {
                waitpid(pid, nullptr, 0);
            }
        }
    }

    void killProcess() {
        lock_guard<mutex> lock(cardMutex);
        if (isRunning) {
            cout << "---- Stop Signal received, attempting to KILL JACK. ----\n\n";
            kill(pid, SIGTERM);
            for (int i = 0; i < 10; ++i) {
                if (!isAlive(pid)) break;
                std::this_thread::sleep_for(50ms);
            }

            if (isAlive(pid)) {
                kill(pid, SIGKILL);
            }

            waitpid(pid, nullptr, 0);

            isRunning = false;
            activeDevPath = "";
        }
    }
};

JackHandler::~JackHandler() = default;

JackHandler::JackHandler(const string& configPath)
: impl(make_unique<Impl>(configPath)) {}

void JackHandler::watch(const atomic<bool>& shouldQuit) {
    impl->watchUdev(shouldQuit);
}

void JackHandler::bootstrap() {
    impl->startConnectedCard();
}