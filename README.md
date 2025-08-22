# Real-Time Stock Price Tracker

A C++ GUI application to track real-time stock prices with search, watchlist, alerts, and live graph plotting.  
Built using **SDL2**, **Dear ImGui**, and **ImPlot**.  

---

## 🚀 Features
- 🔎 **Search & Add Stocks** – Add stocks to your watchlist via search box.  
- 📊 **Real-Time Prices** – Fetches stock data from [Alpha Vantage API](https://www.alphavantage.co/).  
- 🖼 **Clean GUI** – Built with **Dear ImGui** and **ImPlot** for smooth user interface & charts.  
- 🔔 **Price Alerts** – Set alerts with popup notifications.  
- 💾 **Save & Load Watchlist** – Persistence of stock data and alerts.  
- ⚡ **Multithreaded Updates** – Background fetching for smooth UI experience.  

---

## 🛠️ Tech Stack
- **Language**: C++17  
- **GUI**: [Dear ImGui](https://github.com/ocornut/imgui) (submodule)  
- **Graphs**: [ImPlot](https://github.com/epezent/implot) (submodule)  
- **Windowing/Renderer**: SDL2 + OpenGL3 backend  
- **API**: Alpha Vantage (stock price data)  

---

## 🔧 Build Instructions

### 1. Clone with submodules
```bash
git clone --recursive https://github.com/<your-username>/<your-repo>.git
cd <your-repo>
If you already cloned without --recursive, run:
git submodule update --init --recursive
2. Install dependencies
Make sure you have:
SDL2
OpenGL
CMake or your preferred build system

On Ubuntu/Debian:
sudo apt-get install libsdl2-dev libgl1-mesa-dev
On Windows:
Install SDL2 development libraries.
Ensure compiler supports C++17.

3. Build
Using g++ directly:

g++ -std=c++17 -Iimgui -Iimplot -I/usr/include/SDL2 \
    main.cpp imgui/*.cpp implot/*.cpp \
    -lSDL2 -lGL -ldl -lpthread -o stock_tracker
Or with CMake:
mkdir build && cd build
cmake ..
make

4. Run
./stock_tracker
