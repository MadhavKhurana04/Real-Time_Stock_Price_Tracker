    #define SDL_MAIN_HANDLED

    #include <SDL.h>
    #include "imgui.h"
    #include "imgui_impl_sdl2.h"
    #include "imgui_impl_sdlrenderer2.h"
    #include "implot.h"
    #include <vector>
    #include <string>
    #include <map>
    #include <chrono>
    #include <ctime>
    #include <sstream>
    #include <iomanip>
    #include <random>
    #include <thread>
    #include <mutex>
    #include <fstream>
    #include "json.hpp"
    #include <iostream>
    #include <curl/curl.h> // Add at the top if not already included


    using namespace std;
    using json = nlohmann::json;

    mutex dataMutex;
    vector<string> watchlist;
    map<string, double> prices;
    map<string, vector<float>> stockHistory;
    map<string, float> alertMinThresholds;
    map<string, float> alertMaxThresholds;
    vector<string> alertMessages;
    bool dataUpdated = false;
    bool showAlertPopup = false;

    size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

double fetchSimulatedPrice(const string& symbol) {
    CURL* curl;
    CURLcode res;
    string readBuffer;

    string url = "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" + symbol + "&apikey=CVY5GIBL3IXRKHBG";

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            try {
                auto j = json::parse(readBuffer);
                string priceStr = j["Global Quote"]["05. price"];
                return stod(priceStr);
            } catch (...) {
                cerr << "[ERROR] Failed to parse JSON or extract price for " << symbol << endl;
            }
        } else {
            cerr << "[ERROR] CURL fetch failed for " << symbol << ": " << curl_easy_strerror(res) << endl;
        }
    }

    // Fallback simulated price if API fails
    static default_random_engine engine(time(nullptr));
    static uniform_real_distribution<double> dist(-2.0, 2.0);
    static map<string, double> lastPrice;

    if (!lastPrice.count(symbol))
        lastPrice[symbol] = 100.0 + (rand() % 1000) / 10.0;

    lastPrice[symbol] += dist(engine);
    return lastPrice[symbol];
}


    string getCurrentTimestamp() {
        time_t now = time(nullptr);
        tm* local = localtime(&now);
        stringstream ss;
        ss << put_time(local, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void saveToFile(const string& filename) {
        lock_guard<mutex> lock(dataMutex);
        json j;
        j["watchlist"] = watchlist;
        j["min_alerts"] = alertMinThresholds;
        j["max_alerts"] = alertMaxThresholds;
        ofstream out(filename);
        out << j.dump(4);
    }

    void loadFromFile(const string& filename) {
        lock_guard<mutex> lock(dataMutex);
        ifstream in(filename);
        if (in) {
            json j;
            in >> j;

            if (j.contains("watchlist") && j["watchlist"].is_array())
                watchlist = j["watchlist"].get<vector<string>>();
            else
                watchlist.clear();

            alertMinThresholds.clear();
            alertMaxThresholds.clear();

            if (j.contains("alerts") && j["alerts"].is_object()) {
                for (auto& [symbol, thresholds] : j["alerts"].items()) {
                    if (thresholds.contains("min"))
                        alertMinThresholds[symbol] = thresholds["min"].get<float>();
                    if (thresholds.contains("max"))
                        alertMaxThresholds[symbol] = thresholds["max"].get<float>();
                }
            }
        }
    }



    void backgroundPriceUpdater() {
        static map<string, double> lastKnownPrices;

        while (true) {
            this_thread::sleep_for(chrono::seconds(10));
            lock_guard<mutex> lock(dataMutex);

            for (const auto& symbol : watchlist) {
                double newPrice = fetchSimulatedPrice(symbol);
                double oldPrice = prices[symbol];
                prices[symbol] = newPrice;

                stockHistory[symbol].push_back(static_cast<float>(newPrice));
                if (stockHistory[symbol].size() > 50)
                    stockHistory[symbol].erase(stockHistory[symbol].begin());

                // Alert check
                bool hasMin = alertMinThresholds.count(symbol);
                bool hasMax = alertMaxThresholds.count(symbol);
                float minThreshold = hasMin ? alertMinThresholds[symbol] : 0.0f;
                float maxThreshold = hasMax ? alertMaxThresholds[symbol] : 0.0f;

                bool minCrossed = hasMin && oldPrice >= minThreshold && newPrice < minThreshold;
                bool maxCrossed = hasMax && oldPrice <= maxThreshold && newPrice > maxThreshold;

                std::cout << "[DEBUG] " << symbol 
                << " old=" << oldPrice 
                << ", new=" << newPrice 
                << ", min=" << (hasMin ? std::to_string(minThreshold) : "n/a") 
                << ", max=" << (hasMax ? std::to_string(maxThreshold) : "n/a") 
                << ", minCrossed=" << minCrossed 
                << ", maxCrossed=" << maxCrossed 
                << std::endl;
                if (minCrossed || maxCrossed) {
                    stringstream ss;
                    ss << "Alert: " << symbol << " price = " << fixed << setprecision(2) << newPrice;
                    if (minCrossed) ss << " dropped below min = " << minThreshold;
                    if (maxCrossed) ss << " exceeded max = " << maxThreshold;

                    alertMessages.push_back(ss.str());
                    cout << "[ALERT] " << ss.str() << endl;

                    showAlertPopup = true;
                }
            }

            dataUpdated = true;
        }
    }


    int main(int, char**) {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
        SDL_Window* window = SDL_CreateWindow("Real-Time Stock Tracker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 900, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsLight();
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        loadFromFile("watchlist.json");
        thread updater(backgroundPriceUpdater);
        updater.detach();

        bool running = true;
        SDL_Event event;
        static char inputBuf[32] = "";
        string lastUpdatedStr = getCurrentTimestamp();
        static int selectedStockIndex = -1;
        static const char* stockOptions[] = { "AAPL", "GOOGL", "MSFT", "AMZN", "TSLA" };

        while (running) {
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT)
                    running = false;
            }

            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::Begin("Stock Tracker", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

            ImGui::Text("Search and Add Stock:");
            ImGui::SetNextItemWidth(200);
            bool focusInput = false;
            bool enterPressed = ImGui::InputTextWithHint("##input", "Enter Symbol", inputBuf, IM_ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            ImGui::SetNextItemWidth(180);
            if (ImGui::BeginCombo("##Dropdown", selectedStockIndex >= 0 ? stockOptions[selectedStockIndex] : "Select from list")) {
                for (int n = 0; n < IM_ARRAYSIZE(stockOptions); n++) {
                    bool is_selected = (selectedStockIndex == n);
                    if (ImGui::Selectable(stockOptions[n], is_selected)) {
                        selectedStockIndex = n;
                        strncpy(inputBuf, stockOptions[n], IM_ARRAYSIZE(inputBuf));
                        enterPressed = true;
                        focusInput = true;
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (focusInput) {
                ImGui::SetKeyboardFocusHere(-1);
            }

            if (ImGui::Button("Add") || enterPressed) {
                string symbol(inputBuf);
                transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
                if (!symbol.empty()) {
                    {
                        lock_guard<mutex> lock(dataMutex);
                        if (find(watchlist.begin(), watchlist.end(), symbol) == watchlist.end()) {
                            watchlist.push_back(symbol);
                            prices[symbol] = fetchSimulatedPrice(symbol);
                            stockHistory[symbol] = {};
                            alertMinThresholds[symbol] = 0.0f;
                            alertMaxThresholds[symbol] = 0.0f;
                        }
                    }
                    saveToFile("watchlist.json");
                    inputBuf[0] = '\0';
                    selectedStockIndex = -1;
                }
            }

            if (ImGui::Button("Refresh Now")) {
                if (dataMutex.try_lock()) {
                    for (auto& symbol : watchlist) {
                        prices[symbol] = fetchSimulatedPrice(symbol);
                    }
                    lastUpdatedStr = getCurrentTimestamp();
                    dataMutex.unlock();
                }
            }

            if (ImGui::Button("Quit")) running = false;

            ImGui::Text("Last updated: %s", lastUpdatedStr.c_str());
            ImGui::Separator();

            ImGui::BeginChild("StockList", ImVec2(360, 0), true);
            int toRemoveIndex = -1;
            string toRemoveSymbol;
            bool shouldSave = false;

            if (dataMutex.try_lock()) {
                for (int i = 0; i < watchlist.size(); ++i) {
                    const string& symbol = watchlist[i];
                    double price = prices[symbol];
                    ImGui::Text("%s: $%.2f", symbol.c_str(), price);
                    ImGui::SameLine();
                    if (ImGui::Button(("Remove##" + symbol).c_str())) {
                        toRemoveIndex = i;
                        toRemoveSymbol = symbol;
                    }

                    ImGui::PushID(symbol.c_str());
                    ImGui::SetNextItemWidth(60);
                    ImGui::InputFloat("Min", &alertMinThresholds[symbol], 0.0f, 0.0f, "%.1f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(60);
                    ImGui::InputFloat("Max", &alertMaxThresholds[symbol], 0.0f, 0.0f, "%.1f");
                    ImGui::SameLine();
                    ImGui::Text("Alerts");
                    ImGui::PopID();
                }

                if (toRemoveIndex != -1) {
                    watchlist.erase(watchlist.begin() + toRemoveIndex);
                    prices.erase(toRemoveSymbol);
                    stockHistory.erase(toRemoveSymbol);
                    alertMinThresholds.erase(toRemoveSymbol);
                    alertMaxThresholds.erase(toRemoveSymbol);
                    shouldSave = true;
                }

                dataMutex.unlock();
            }

            if (shouldSave) {
                saveToFile("watchlist.json");
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("Graph", ImVec2(0, 0), true);
            if (dataMutex.try_lock()) {
                for (const auto& symbol : watchlist) {
                    const vector<float>& history = stockHistory[symbol];
                    if (!history.empty()) {
                        string plotTitle = "Trend - " + symbol;
                        if (ImPlot::BeginPlot(plotTitle.c_str(), ImVec2(-1, 150))) {
                            static vector<float> x;
                            x.resize(history.size());
                            for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<float>(i);
                            ImPlot::PlotLine("Price", x.data(), history.data(), (int)history.size());
                            ImPlot::EndPlot();
                        }
                    }
                }
                dataMutex.unlock();
            }
            ImGui::EndChild();

            if (showAlertPopup && !alertMessages.empty()) {
                ImGui::OpenPopup("Alert");
                showAlertPopup = false;
            }

            if (ImGui::BeginPopupModal("Alert", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Stock Price Alerts:");
                ImGui::Separator();

                ImGui::BeginChild("AlertScroll", ImVec2(400, 100), true);
                for (const auto& msg : alertMessages) {
                    ImGui::TextWrapped("%s", msg.c_str());
                }
                ImGui::EndChild();

                if (ImGui::Button("OK")) {
                    alertMessages.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::End();
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        }

        ImPlot::DestroyContext();
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
